/* linanqinqin */
// SPDX-License-Identifier: GPL-2.0-only
/*
 * LAME (Latency-Aware Memory Exception) Runtime Configuration Module
 *
 * This module provides a character device interface for runtime configuration
 * of the LAME exception handler through ioctl commands.
 *
 * Copyright (C) 2025 Nanqinqin Li <linanqinqin@princeton.edu>
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <asm/desc.h>
#include <asm/trapnr.h>
#include <asm/set_memory.h>
#include <asm/segment.h>
#include <uapi/linux/lame.h>

/* Constants for IDT entry configuration */
#define DEFAULT_STACK 0
#define DPL3 0x3

/* External declarations for IDT management */
extern gate_desc idt_table[];
extern struct desc_ptr idt_descr;

/**
 * pack_gate_lame - Create a gate descriptor for LAME handler
 * @gate: Pointer to gate_desc structure to fill
 * @type: Gate type (GATE_TRAP, GATE_INTERRUPT, etc.)
 * @func: Handler function address
 * @dpl: Descriptor Privilege Level
 * @ist: Interrupt Stack Table index
 * @seg: Code segment selector
 *
 * Similar to pack_gate but allows custom segment selection for x86_64.
 */
static void pack_gate_lame(gate_desc *gate, unsigned type, unsigned long func,
                          unsigned dpl, unsigned ist, unsigned seg)
{
    gate->offset_low    = (u16) func;
    gate->bits.p        = 1;
    gate->bits.dpl      = dpl;
    gate->bits.zero     = 0;
    gate->bits.type     = type;
    gate->offset_middle = (u16) (func >> 16);
#ifdef CONFIG_X86_64
    gate->segment       = seg;  /* Use provided segment instead of __KERNEL_CS */
    gate->bits.ist      = ist;
    gate->reserved      = 0;
    gate->offset_high   = (u32) (func >> 32);
#else
    gate->segment       = seg;
    gate->bits.ist      = 0;
#endif
}

/* Device number - auto-assign */
static int major = 0;

/* Global variables */
static dev_t lame_dev;
static struct cdev *lame_cdev;
static struct class *lame_class;
static struct device *lame_device;

/* Forward declarations */
static int lame_open(struct inode *inode, struct file *file);
static int lame_release(struct inode *inode, struct file *file);
static long lame_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int lame_enable_handler(__u64 handler_addr);
static int lame_disable_handler(void);
static void pack_gate_lame(gate_desc *gate, unsigned type, unsigned long func,
                          unsigned dpl, unsigned ist, unsigned seg);

/* File operations structure */
static const struct file_operations lame_fops = {
    .owner = THIS_MODULE,
    .open = lame_open,
    .release = lame_release,
    .unlocked_ioctl = lame_ioctl,
    .compat_ioctl = lame_ioctl,  /* For 32-bit compatibility */
};

/* File operation implementations */
static int lame_open(struct inode *inode, struct file *file)
{
    pr_debug("[lame_open] LAME device opened\n");
    return 0;
}

static int lame_release(struct inode *inode, struct file *file)
{
    pr_debug("[lame_release] LAME device closed\n");
    return 0;
}

/**
 * lame_register_direct - Register or unregister a LAME handler directly into the IDT table
 * the installed handler will replace the original IDT[X86_TRAP_LAME] handler and be globally visible
 * @file: The ioctl file pointer
 * @arg: The argument pointer to the lame_arg structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int lame_register_direct(struct file *file, unsigned long arg)
{
    struct lame_arg user_arg;
    int ret = 0;
    
    pr_debug("[lame_register_direct] LAME_REGISTER ioctl called\n");
    
    /* Copy argument from user space */
    if (copy_from_user(&user_arg, (void __user *)arg, sizeof(user_arg))) {
        pr_err("[lame_register_direct] Failed to copy argument from user space\n");
        return -EFAULT;
    }
    
    pr_debug("[lame_register_direct] LAME_REGISTER: present=%d, handler_addr=0x%llx\n", 
             user_arg.present, user_arg.handler_addr);
    
    /* Implement actual LAME logic here */
    if (user_arg.present) {
        pr_info("[lame_register_direct] LAME_REGISTER: enabling LAME handler at 0x%llx\n", 
                user_arg.handler_addr);
        
        /* Enable LAME handler */
        ret = lame_enable_handler(user_arg.handler_addr);
        if (ret < 0) {
            pr_err("[lame_register_direct] Failed to enable LAME handler: %d\n", ret);
            return ret;
        }
    } else {
        pr_info("[lame_register_direct] LAME_REGISTER: disabling LAME\n");
        
        /* Disable LAME handler */
        ret = lame_disable_handler();
        if (ret < 0) {
            pr_err("[lame_register_direct] Failed to disable LAME handler: %d\n", ret);
            return ret;
        }
    }
    
    return ret;
}

/**
 * lame_register_per_task - Register or unregister a LAME handler for the current task
 * This function populates the lame_cfg in the current task's task_struct
 * @file: The ioctl file pointer
 * @arg: The argument pointer to the lame_arg structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int lame_register_per_task(struct file *file, unsigned long arg)
{
    struct lame_arg user_arg;

    /* Copy argument from user space */
    if (copy_from_user(&user_arg, (void __user *)arg, sizeof(user_arg))) {
        pr_err("[lame_register_per_task] Failed to copy argument from user space\n");
        return -EFAULT;
    }
    
    /* Access current task using the 'current' macro */
    if (user_arg.present) {
        
        /* Populate lame_cfg in current task's task_struct */
        current->lame_cfg.is_active = 1;
        current->lame_cfg.handler_addr = (unsigned long)user_arg.handler_addr;
        current->lame_cfg.sample_period = 1000; /* Default sample period, will add to lame_arg later */
        
        pr_info("[lame_register_per_task] LAME registered for task %d: handler=0x%lx, period=%llu\n",
                current->pid, current->lame_cfg.handler_addr, current->lame_cfg.sample_period);
    } else {
        
        /* Clear lame_cfg in current task's task_struct */
        current->lame_cfg.is_active = 0;
        current->lame_cfg.handler_addr = 0;
        current->lame_cfg.sample_period = 0;

        pr_info("[lame_register_per_task] LAME unregistered for task %d\n", current->pid);
    }
    
    return 0;
}

/* Main IOCTL dispatcher */
static long lame_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    long ret = 0;
    
    /* Check if the command is for us */
    if (_IOC_TYPE(cmd) != LAME_IOC_MAGIC) {
        pr_err("[lame_ioctl] Invalid ioctl magic number\n");
        return -ENOTTY;
    }
    
    /* Check if the command number is valid */
    if (_IOC_NR(cmd) > 1) {
        pr_err("[lame_ioctl] Invalid ioctl command number\n");
        return -ENOTTY;
    }
    
    /* Dispatch to appropriate handler */
    switch (cmd) {
    case LAME_REGISTER:
        ret = lame_register_per_task(file, arg);
        break;
    case LAME_REGISTER_DIRECT:
        ret = lame_register_direct(file, arg);
        break;
    default:
        pr_err("[lame_ioctl] Unknown ioctl command: 0x%x\n", cmd);
        ret = -ENOTTY;
        break;
    }
    
    return ret;
}

/* LAME handler management functions */

/**
 * lame_enable_handler - Enable the LAME exception handler
 * @handler_addr: User-space handler address
 *
 * Creates a new IDT entry for X86_TRAP_LAME with the specified handler.
 * The entry is configured as a trap gate with DPL3 and user code segment.
 */
static int lame_enable_handler(__u64 handler_addr)
{
    gate_desc new_entry;
    int ret = 0;
    
    pr_debug("[lame_enable_handler] Setting up LAME handler at 0x%llx\n", handler_addr);
    
    /* Validate handler address */
    if (!handler_addr) {
        pr_err("[lame_enable_handler] Invalid handler address: 0x%llx\n", handler_addr);
        return -EINVAL;
    }
    
    /* Create the new IDT entry exactly as specified */
    pack_gate_lame(&new_entry, GATE_TRAP, handler_addr, DPL3, DEFAULT_STACK, __USER_CS);
    
    pr_debug("[lame_enable_handler] New entry: addr=0x%llx, type=%d, dpl=%d\n",
             handler_addr, GATE_TRAP, DPL3);
    
    /* Make IDT writable temporarily */
    ret = set_memory_rw((unsigned long)idt_table, 1);
    if (ret < 0) {
        pr_err("[lame_enable_handler] Failed to make IDT writable: %d\n", ret);
        return ret;
    }
    
    /* Write the new entry to the IDT */
    write_idt_entry(idt_table, X86_TRAP_LAME, &new_entry);
    
    /* Reload the IDT */
    load_idt(&idt_descr);
    
    /* Make IDT read-only again */
    ret = set_memory_ro((unsigned long)idt_table, 1);
    if (ret < 0) {
        pr_err("[lame_enable_handler] Failed to make IDT read-only: %d\n", ret);
        /* Continue anyway as the handler is already installed */
    }
    
    pr_info("[lame_enable_handler] LAME handler successfully installed at 0x%llx\n", handler_addr);
    return 0;
}

/**
 * lame_disable_handler - Disable the LAME exception handler
 *
 * Disables the LAME exception handler by setting the present bit to 0.
 */
static int lame_disable_handler(void)
{
    gate_desc non_entry;
    int ret = 0;
    
    pr_debug("[lame_disable_handler] Disabling LAME handler\n");
    
    /* Create a minimal entry with just the present bit set to 0 */
    memset(&non_entry, 0, sizeof(non_entry));
    non_entry.bits.p = 0;  /* This is all we need to disable the entry */
    
    pr_debug("[lame_disable_handler] Disabling LAME entry (present=0)\n");
    
    /* Make IDT writable temporarily */
    ret = set_memory_rw((unsigned long)idt_table, 1);
    if (ret < 0) {
        pr_err("[lame_disable_handler] Failed to make IDT writable: %d\n", ret);
        return ret;
    }
    
    /* Write the modified entry back to the IDT */
    write_idt_entry(idt_table, X86_TRAP_LAME, &non_entry);
    
    /* Reload the IDT */
    load_idt(&idt_descr);
    
    /* Make IDT read-only again */
    ret = set_memory_ro((unsigned long)idt_table, 1);
    if (ret < 0) {
        pr_err("[lame_disable_handler] Failed to make IDT read-only: %d\n", ret);
        /* Continue anyway as the handler is already disabled */
    }
    
    pr_info("[lame_disable_handler] LAME handler successfully disabled\n");
    return 0;
}

/* Module initialization */
static int __init lame_init(void)
{
    int ret;
    
    /* Allocate device number */
    if (major) {
        lame_dev = MKDEV(major, 0);
        ret = register_chrdev_region(lame_dev, 1, LAME_DEV_NAME);
    } else {
        ret = alloc_chrdev_region(&lame_dev, 0, 1, LAME_DEV_NAME);
    }
    
    if (ret < 0) {
        pr_err("[LAME module] Failed to allocate device number: %d\n", ret);
        return ret;
    }
    
    /* Create character device */
    lame_cdev = cdev_alloc();
    if (!lame_cdev) {
        pr_err("[LAME module] Failed to allocate cdev\n");
        ret = -ENOMEM;
        goto error_unregister;
    }
    
    cdev_init(lame_cdev, &lame_fops);
    lame_cdev->owner = THIS_MODULE;
    
    ret = cdev_add(lame_cdev, lame_dev, 1);
    if (ret < 0) {
        pr_err("[LAME module] Failed to add cdev: %d\n", ret);
        goto error_cdev_del;
    }
    
    /* Create device class */
    lame_class = class_create(LAME_DEV_NAME);
    if (IS_ERR(lame_class)) {
        pr_err("[LAME module] Failed to create class: %ld\n", PTR_ERR(lame_class));
        ret = PTR_ERR(lame_class);
        goto error_cdev_del;
    }
    
    /* Create device file */
    lame_device = device_create(lame_class, NULL, lame_dev, NULL, LAME_DEV_NAME);
    if (IS_ERR(lame_device)) {
        pr_err("[LAME module] Failed to create device: %ld\n", PTR_ERR(lame_device));
        ret = PTR_ERR(lame_device);
        goto error_class_destroy;
    }
    
    pr_info("[LAME module] LAME device: /dev/%s (major=%d, minor=%d)\n", 
            LAME_DEV_NAME, MAJOR(lame_dev), MINOR(lame_dev));
    
    return 0;
    
error_class_destroy:
    class_destroy(lame_class);
error_cdev_del:
    cdev_del(lame_cdev);
error_unregister:
    unregister_chrdev_region(lame_dev, 1);
    return ret;
}

/* Module cleanup */
static void __exit lame_exit(void)
{
    /* Remove device file */
    if (lame_device) {
        device_destroy(lame_class, lame_dev);
    }
    
    /* Remove device class */
    if (lame_class) {
        class_destroy(lame_class);
    }
    
    /* Remove character device */
    if (lame_cdev) {
        cdev_del(lame_cdev);
    }
    
    /* Unregister device number */
    unregister_chrdev_region(lame_dev, 1);
    
    pr_info("[LAME module] unloaded\n");
}

/* Built-in module initialization */
device_initcall(lame_init); 
/* end */
