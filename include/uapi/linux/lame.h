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
#include <sys/syscall.h>

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

/* helper function for enabling LAME PMU emulation via perf_event_open */
static inline int lame_event_open(pid_t pid, uint64_t sample_period) {
    /* configure LLC load miss event */
    struct perf_event_attr pea;
    memset(&pea, 0, sizeof(pea));

    pea.type = PERF_TYPE_RAW;
    pea.size = sizeof(struct perf_event_attr);

    // MEM_LOAD_RETIRED.L3_MISS: event=0x2E, umask=0x41
    pea.config = (0x41ULL << 8) | 0x2E;

    pea.sample_period = sample_period; // this is only needed by Linux perf for the initial counter setup; only affects the first overflow
    pea.precise_ip = 0;                // request regular PMU counting
    pea.exclude_kernel = 1;            // only count user-space
    pea.exclude_hv = 1;                // skip hypervisor
    pea.disabled = 0;                  // start immediately
    pea.pinned = 1;                    // pin to a counter (avoid multiplex)

    /* tie the event to the pid */
    return syscall(__NR_perf_event_open, &pea, pid, -1, -1, 0);
}

#endif /* _UAPI_LINUX_LAME_H */ 
/* end */
