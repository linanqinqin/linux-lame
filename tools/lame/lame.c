/* linanqinqin */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/lame.h>

static inline long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

int set_idt2_nmi(void)
{
    int lame_fd = open(LAME_DEV_PATH, O_RDWR);
    if (lame_fd < 0) {
        fprintf(stderr, "LAME ioctl device open failed with errno: %d\n", errno);
        return -1;
    }

    if (ioctl(lame_fd, LAME_IDT2_SET_NMI)) {
        fprintf(stderr, "LAME ioctl device set IDT[2] to stock NMI handler failed with errno: %d\n", errno);
        close(lame_fd);
        return -1;
    }
    close(lame_fd);
    return 0;
}

int set_idt2_lame(void)
{
    int lame_fd = open(LAME_DEV_PATH, O_RDWR);
    if (lame_fd < 0) {
        fprintf(stderr, "LAME ioctl device open failed with errno: %d\n", errno);
        return -1;
    }

    if (ioctl(lame_fd, LAME_IDT2_SET_LAME)) {
        fprintf(stderr, "LAME ioctl device set IDT[2] to LAME kernel trampoline failed with errno: %d\n", errno);
        close(lame_fd);
        return -1;
    }
    close(lame_fd);
    return 0;
}

int enable_lame(pid_t pid, uint64_t sample_period)
{
    /* set IDT[2] to the LAME kernel trampoline */
    if (set_idt2_lame()) {
        return -1;
    }

    /* configure LLC load miss event */
    struct perf_event_attr pea;
    memset(&pea, 0, sizeof(pea));

    pea.type = PERF_TYPE_RAW;
    pea.size = sizeof(struct perf_event_attr);

    // MEM_LOAD_RETIRED.L3_MISS: event=0x2E, umask=0x41
    pea.config = (0x41ULL << 8) | 0x2E;

    pea.sample_period = sample_period; // e.g. every Nth LLC miss
    pea.precise_ip = 2;                // request PEBS (precise sampling)
    pea.exclude_kernel = 1;            // only count user-space
    pea.exclude_hv = 1;                // skip hypervisor
    pea.disabled = 0;                  // start immediately
    pea.pinned = 1;                    // pin to a counter (avoid multiplex)

    int fd = perf_event_open(&pea, pid, -1, -1, 0);
    if (fd == -1) {
        fprintf(stderr, "perf_event_open failed with errno: %d\n", errno);
        if (set_idt2_nmi()) {
            fprintf(stderr, "Manual IDT[2] reset is required\n");
        }
        return -1;
    }

    fprintf(stdout, "LAME emulation enabled on pid %d with sample period %lu\n", pid, sample_period);
    return fd;
}

int disable_lame(int fd)
{
    if (fd >= 0) {
        close(fd);
        fprintf(stdout, "LAME emulation disabled\n");
    }

    if (set_idt2_nmi()) {
        fprintf(stderr, "Manual IDT[2] reset is required\n");
    }

    return 0;
}

static void usage(const char *progname)
{
    fprintf(stderr, "Usage: %s -i <pid> -p <sample_period>\n", progname);
    fprintf(stderr, "  -i <pid>         Target process ID\n");
    fprintf(stderr, "  -p <period>      Sample period (every Nth LLC miss)\n");
    fprintf(stderr, "  -n               Set IDT[2] to stock NMI handler\n");
    fprintf(stderr, "  -l               Set IDT[2] to LAME kernel trampoline\n");
    fprintf(stderr, "  -h               Show this help message\n");
    fprintf(stderr, "\nExample: %s -i 1234 -p 10\n", progname);
    fprintf(stderr, "\nExample: %s -n\n", progname);
    fprintf(stderr, "\nExample: %s -l\n", progname);
}

int main(int argc, char **argv)
{
    pid_t pid = 0;
    uint64_t period = 0;
    int do_set_idt2_nmi = 0;
    int do_set_idt2_lame = 0;
    int opt;
    
    while ((opt = getopt(argc, argv, "i:p:nlh")) != -1) {
        switch (opt) {
        case 'i':
            pid = atoi(optarg);
            if (pid <= 0) {
                fprintf(stderr, "Error: Invalid PID '%s'\n", optarg);
                return 1;
            }
            break;
        case 'p':
            period = strtoull(optarg, NULL, 0);
            if (period == 0) {
                fprintf(stderr, "Error: Invalid sample period '%s'\n", optarg);
                return 1;
            }
            break;
        case 'n':
            do_set_idt2_nmi = 1;
            break;
        case 'l':
            do_set_idt2_lame = 1;
            break;
        case 'h':
            usage(argv[0]);
            return 0;
        case '?':
        default:
            usage(argv[0]);
            return 1;
        }
    }
    
    if (do_set_idt2_nmi && !do_set_idt2_lame && pid == 0 && period == 0) {
        return set_idt2_nmi();
    }
    else if (!do_set_idt2_nmi && do_set_idt2_lame && pid == 0 && period == 0) {
        return set_idt2_lame();
    }
    else if (!do_set_idt2_nmi && !do_set_idt2_lame && pid && period) {

        int fd = enable_lame(pid, period);
        if (fd < 0) return 1;

        // Let it run until user presses Enter
        fprintf(stdout, "Press Enter to disable LAME emulation...\n");
        getchar();

        disable_lame(fd);
    }
    else {
        fprintf(stderr, "Error: Invalid options\n");
        usage(argv[0]);
        return -1;
    }

    return 0;
}
/* end */
