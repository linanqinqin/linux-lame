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
#define LAME_REGISTER _IOW(LAME_IOC_MAGIC, 1, struct lame_arg)
#define LAME_REGISTER_DIRECT _IOW(LAME_IOC_MAGIC, 2, struct lame_arg)

#define LAME_DEV_NAME "lame"
#define LAME_DEV_PATH "/dev/" LAME_DEV_NAME

/* Argument structure */
struct lame_arg {
    __u8 present;        /* Use __u8 instead of bool for ABI stability */
    __u64 handler_addr; /* Use __u64 for 64-bit compatibility */
};

#endif /* _UAPI_LINUX_LAME_H */ 
/* end */
