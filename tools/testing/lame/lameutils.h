/*
 * LAME (Latency-Aware Memory Exception) utility functions
 *
 * This header provides helper functions for working with the LAME kernel module.
 */

#ifndef _LAMEUTILS_H
#define _LAMEUTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <linux/lame.h>

/**
 * lame_handler_register_direct - Register or unregister a LAME handler directly into the IDT table
 * @handler: Pointer to the handler function (NULL to unregister)
 * @enable: 1 to register, 0 to unregister
 *
 * Returns: 0 on success, negative error code on failure
 */
static inline int lame_handler_register_direct(void *handler, int enable)
{
    int fd;
    struct lame_arg arg;
    int ret;

    /* Open the LAME device */
    fd = open(LAME_DEV_PATH, O_RDWR);
    if (fd < 0) {
        return -1;
    }

    /* Set up the argument structure */
    arg.present = enable ? 1 : 0;
    arg.handler_addr = enable ? (__u64)handler : 0;

    /* Perform the ioctl */
    ret = ioctl(fd, LAME_REGISTER_DIRECT, &arg);
    
    /* Close the device */
    close(fd);

    return ret;
}

/**
 * lame_handler_register_self - Register or unregister a LAME handler for the current task
 * @handler: Pointer to the handler function (NULL to unregister)
 * @enable: 1 to register, 0 to unregister
 *
 * Returns: 0 on success, negative error code on failure
 */
static inline int lame_handler_register_self(void *handler, int enable)
{
    int fd;
    struct lame_arg arg;
    int ret;
    
    /* Open the LAME device */
    fd = open(LAME_DEV_PATH, O_RDWR);
    if (fd < 0) {
        return -1;
    }

    /* Set up the argument structure */
    arg.present = enable ? 1 : 0;
    arg.handler_addr = enable ? (__u64)handler : 0;

    /* Perform the ioctl */
    ret = ioctl(fd, LAME_REGISTER, &arg);
    
    /* Close the device */
    close(fd);

    return ret;
}

/**
 * lame_int - Invoke the LAME interrupt (INT 0x1F)
 *
 * This is an inline function that simply invokes int 0x1f.
 * The handler registered via lame_handler_register will be called.
 */
static inline void lame_int(void)
{
    asm volatile("int $0x1f");
}

#endif /* _LAMEUTILS_H */ 
