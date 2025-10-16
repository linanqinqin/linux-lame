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
#include <linux/perf_event.h>

/* IOCTL command codes */
#define LAME_IOC_MAGIC 'L'
/* register userspace handler*/
#define LAME_REGISTER_PMU _IOW(LAME_IOC_MAGIC, 1, struct lame_arg)
#define LAME_REGISTER_INT _IOW(LAME_IOC_MAGIC, 2, struct lame_arg)
/* manipulate IDT[2] for pebs emulation */
#define LAME_IDT2_SET_NMI _IO(LAME_IOC_MAGIC, 3)
#define LAME_IDT2_SET_LAME _IO(LAME_IOC_MAGIC, 4)
/* LAME internal monitor */
#define LAME_COUNTER_READ _IOR(LAME_IOC_MAGIC, 5, struct lame_counter)
/* LAME PMU emulation configure */
#define LAME_CONFIG_PMU _IOW(LAME_IOC_MAGIC, 6, struct lame_pmu_arg)

#define LAME_DEV_NAME "lame"
#define LAME_DEV_PATH "/dev/" LAME_DEV_NAME

/* arguments for end users to register/unregister LAME */
struct lame_arg {
    __u8 present;           /* if present is 1, register the LAME handler; if 0, unregister it */
    __u64 handler_addr;     /* the address of the userspace LAME handler */
};

#define LAME_CONFIG_OPTION_NONE 0x0
#define LAME_CONFIG_OPTION_UPCALL 0x1
#define LAME_CONFIG_OPTION_STALL 0x2

/* arguments for configuring LAME emulation via PMU*/
struct lame_pmu_arg {

    /* the configuration options for LAME emulation 
     * bit 0: enable upcall to the userspace handler 
     * bit 1: enable stall emulation 
     * upper 16 bits: TSC duration for stall emulation */
    __u32 config;           
    
    /* the pid of the target task; this is needed becuase LAME emulation 
     * is not (and should not be) configured directly by the user program */
    pid_t pid; 
    
    /* the periods string for specifying a tuple of sample periods and their number of occurrences 
     * the 64-bit string is divided into four 16-bit fields
     * the higher two fields are the two sample periods
     * the lower two fields are the number of occurrences of the two sample periods, respectively
     * if either the period or the occurrence is 0, it is ignored */
    __u64 sample_periods;    
};

/* LAME counters */
struct lame_counter {
    __u64 nmi_entry;
    __u64 handler_upcall;
    __u64 stall_emulation;
    __u64 stall_duration_total;
};

#endif /* _UAPI_LINUX_LAME_H */ 
/* end */
