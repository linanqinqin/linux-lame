/* linanqinqin */
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * LAME (Latency-Aware Memory Exception) user-space interface
 *
 * This header defines the ioctl interface for the LAME kernel module.
 */

#ifndef _UAPI_LINUX_LAME_H
#define _UAPI_LINUX_LAME_H

#include <linux/ioctl.h>
#include <linux/types.h>

/* IOCTL command codes */
#define LAME_IOC_MAGIC 'L'
/* register userspace handler*/
#define LAME_REGISTER_PEBS _IOW(LAME_IOC_MAGIC, 1, struct lame_arg)
#define LAME_REGISTER_INT _IOW(LAME_IOC_MAGIC, 2, struct lame_arg)
/* manipulate IDT[2] for pebs emulation */
#define LAME_IDT2_SET_NMI _IO(LAME_IOC_MAGIC, 3)
#define LAME_IDT2_SET_LAME _IO(LAME_IOC_MAGIC, 4)
/* LAME internal monitor */
#define LAME_COUNTER_READ _IOR(LAME_IOC_MAGIC, 5, __u64)

#define LAME_DEV_NAME "lame"
#define LAME_DEV_PATH "/dev/" LAME_DEV_NAME

/* Argument structure */
struct lame_arg {
    __u8 present;        /* Use __u8 instead of bool for ABI stability */
    __u64 handler_addr; /* Use __u64 for 64-bit compatibility */
};

#endif /* _UAPI_LINUX_LAME_H */ 
/* end */
