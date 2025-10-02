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
#define LAME_REGISTER_PEBS _IOW(LAME_IOC_MAGIC, 1, struct lame_arg)
#define LAME_REGISTER_INT _IOW(LAME_IOC_MAGIC, 2, struct lame_arg)
/* manipulate IDT[2] for pebs emulation */
#define LAME_IDT2_SET_NMI _IO(LAME_IOC_MAGIC, 3)
#define LAME_IDT2_SET_LAME _IO(LAME_IOC_MAGIC, 4)
/* LAME internal monitor */
#define LAME_COUNTER_READ _IOR(LAME_IOC_MAGIC, 5, __u64)
/* LAME PMU emulation configure */
#define LAME_CONFIG_PMU _IOW(LAME_IOC_MAGIC, 6, struct lame_pmu_arg)

#define LAME_DEV_NAME "lame"
#define LAME_DEV_PATH "/dev/" LAME_DEV_NAME

/* arguments for end users to register/unregister LAME */
struct lame_arg {
    __u8 present;           /* if present is 1, register the LAME handler; if 0, unregister it */
    __u64 handler_addr;     /* the address of the userspace LAME handler */
};

/* arguments for configuring LAME emulation via PMU*/
struct lame_pmu_arg {
    /* percentage takes precedence over sample_period */
    __u64 percentage;       /* percentage of LLC misses that will be emulated as LAME; range [1, 100], read as percentage/100 */
    __u64 sample_period;    /* a fixed period for resetting the counter */
};

#endif /* _UAPI_LINUX_LAME_H */ 
/* end */
