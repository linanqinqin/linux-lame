/* linanqinqin */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <errno.h>
#include <getopt.h>

static inline long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

int enable_lame(pid_t pid, uint64_t sample_period)
{
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
        perror("perf_event_open failed with errno: %d", errno);
        return -1;
    }

    printf("LAME emulation enabled on pid %d with sample period %llu\n", pid, sample_period);
    return fd;
}

int disable_lame(int fd)
{
    if (fd >= 0) {
        close(fd);
        printf("LAME emulation disabled\n");
    }
    return 0;
}

static void usage(const char *progname)
{
    fprintf(stderr, "Usage: %s -i <pid> -p <sample_period>\n", progname);
    fprintf(stderr, "  -i <pid>         Target process ID\n");
    fprintf(stderr, "  -p <period>      Sample period (every Nth LLC miss)\n");
    fprintf(stderr, "\nExample: %s -i 1234 -p 10\n", progname);
}

int main(int argc, char **argv)
{
    pid_t pid = 0;
    uint64_t period = 0;
    int opt;
    
    while ((opt = getopt(argc, argv, "i:p:h")) != -1) {
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
        case 'h':
            usage(argv[0]);
            return 0;
        case '?':
        default:
            usage(argv[0]);
            return 1;
        }
    }
    
    if (pid == 0 || period == 0) {
        fprintf(stderr, "Error: Both -i and -p options are required\n");
        usage(argv[0]);
        return 1;
    }

    int fd = enable_lame(pid, period);
    if (fd < 0) return 1;

    // Let it run until user presses Enter
    printf("Press Enter to disable LAME emulation...\n");
    getchar();

    disable_lame(fd);
    return 0;
}
/* end */
