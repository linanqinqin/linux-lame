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
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

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

int config_lame(bool enable, pid_t pid, uint64_t sample_periods)
{
    struct lame_pmu_arg lame_pmu_arg;
    lame_pmu_arg.enable = enable;
    lame_pmu_arg.pid = pid;
    lame_pmu_arg.sample_periods = sample_periods;

    int lame_fd = open(LAME_DEV_PATH, O_RDWR);
    if (lame_fd < 0) {
        fprintf(stderr, "LAME ioctl device open failed with errno: %d\n", errno);
        return -1;
    }

    if (ioctl(lame_fd, LAME_CONFIG_PMU, &lame_pmu_arg)) {
        fprintf(stderr, "LAME ioctl device config failed with errno: %d\n", errno);
        close(lame_fd);
        return -1;
    }
    close(lame_fd);
    return 0;
}

static int get_lame_counter(struct lame_counter *cntr_vals)
{
    int lame_fd = open(LAME_DEV_PATH, O_RDWR);
    if (lame_fd < 0) {
        fprintf(stderr, "LAME ioctl device open failed with errno: %d\n", errno);
        return -1;
    }

    if (ioctl(lame_fd, LAME_COUNTER_READ, cntr_vals)) {
        fprintf(stderr, "LAME ioctl device read counter failed with errno: %d\n", errno);
        close(lame_fd);
        return -1;
    }
    close(lame_fd);

    return 0;
}

/* Global variables for cleanup */
#define MAX_CPUS 128
static int global_fds[MAX_CPUS];
static int global_cpu_start = -1;
static int global_cpu_end = -1;
static pid_t global_pid = -1;
static volatile sig_atomic_t shutdown_requested = 0;

/* Signal handler for graceful shutdown */
static void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        shutdown_requested = 1;
    }
}

/* Cleanup function for LAME emulation */
static void disable_lame(void)
{
    if (global_cpu_start >= 0 && global_cpu_end >= 0) {
        for (int cpu = global_cpu_start; cpu <= global_cpu_end; cpu++) {
            if (global_fds[cpu] >= 0) {
                close(global_fds[cpu]);
            }
        }
    }
}

/* Check if target PID is still running */
static int is_pid_running(pid_t pid)
{
    if (pid <= 0) return 1; /* No PID to monitor */
    
    int result = kill(pid, 0);
    if (result == 0) {
        return 1; /* PID is running */
    } else if (errno == ESRCH) {
        return 0; /* PID does not exist */
    } else {
        return 1; /* Other error, assume running */
    }
}

int enable_lame(pid_t pid, int cpu_start, int cpu_end, uint64_t sample_periods)
{

    if ((pid < 0 && cpu_start < 0) || sample_periods <= 0) { 
        fprintf(stderr, "Error: Invalid options\n");
        return -1;
    }
    if (cpu_end >= MAX_CPUS) {
        fprintf(stderr, "Error: CPU range exceeds MAX_CPUS=128\n");
        return -1;
    }

    /* Set up signal handlers for graceful shutdown */
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    /* Store global state for cleanup */
    global_pid = pid;
    global_cpu_start = cpu_start;
    global_cpu_end = cpu_end;
    
    /* configure LAME emulation */
    if (pid > 0) {
        if (config_lame(true, pid, sample_periods)) {
            fprintf(stderr, "config_lame failed\n");
            return -1;
        }
    }

    struct lame_counter cntr_vals_start;
    get_lame_counter(&cntr_vals_start);

    /* configure LLC load miss event */
    struct perf_event_attr pea;
    memset(&pea, 0, sizeof(pea));

    pea.type = PERF_TYPE_RAW;
    pea.size = sizeof(struct perf_event_attr);

    // MEM_LOAD_RETIRED.L3_MISS: event=0x2E, umask=0x41
    pea.config = (0x41ULL << 8) | 0x2E;

    pea.sample_period = 2026ULL;       // this would be meaningless; config_lame sets the periods 
    pea.precise_ip = 0;                // request regular PMU counting
    pea.exclude_kernel = 1;            // only count user-space
    pea.exclude_hv = 1;                // skip hypervisor
    pea.disabled = 0;                  // start immediately
    pea.pinned = 1;                    // pin to a counter (avoid multiplex)

    for (int cpu = cpu_start; cpu <= cpu_end; cpu++) {
        int fd = perf_event_open(&pea, pid, cpu, -1, 0);
        if (fd == -1) {
            fprintf(stderr, "perf_event_open failed with errno: %d\n", errno);
            for (int i = cpu_start; i<cpu; i++) {
                close(global_fds[i]);
            }
            return -1;
        }
        global_fds[cpu] = fd;
    }

    if (pid > 0 && cpu_start >= 0) {
        fprintf(stdout, "LAME emulation enabled on pid %d on cores %d-%d\n", pid, cpu_start, cpu_end);
    }
    else if (pid > 0 && cpu_start < 0) {
        fprintf(stdout, "LAME emulation enabled on pid %d\n", pid);
    }
    else if (pid < 0 && cpu_start >= 0) {
        fprintf(stdout, "LAME emulation enabled on cores %d-%d\n", cpu_start, cpu_end);
    }
    
    /* Main monitoring loop */
    while (!shutdown_requested) {
        if (pid > 0 && !is_pid_running(pid)) {
            fprintf(stdout, "Target PID %d has terminated\n", pid);
            break;
        }
        usleep(100000); /* Sleep for 100ms */
    }
    
    /* Cleanup */
    disable_lame();
    if (pid > 0) {
        config_lame(false, pid, 0);
    }

    struct lame_counter cntr_vals_end;
    get_lame_counter(&cntr_vals_end);
    fprintf(stdout, "LAME counted: %llu, %llu\n", cntr_vals_end.nmi_entry - cntr_vals_start.nmi_entry, 
            cntr_vals_end.handler_upcall - cntr_vals_start.handler_upcall);

    return 0;
}

int print_lame_counter(void)
{
    struct lame_counter cntr_vals;
    if (get_lame_counter(&cntr_vals)) {
        fprintf(stderr, "get_lame_counter failed\n");
        return -1;
    }
    fprintf(stdout, "LAME counter values: %llu, %llu\n", cntr_vals.nmi_entry, cntr_vals.handler_upcall);
    return 0;
}


#define ACCURACY 100
uint64_t percentage_to_periods(uint64_t x_percent) {

    int n = ACCURACY;

    if (n % x_percent == 0) {
        return (uint64_t)(n/x_percent) << 48 | (uint64_t)(n/x_percent) << 32 | (uint64_t)ACCURACY << 16 | (uint64_t)ACCURACY;
    }

    // target fraction = x_percent / 100
    int r = 100 / x_percent;   // floor approx
    int a = r;
    int b = r + 1;
    if (a < 1) a = 1;

    // Compute ideal k (using integer math, careful with scaling)
    int num = (int)n * x_percent * a * b;  // numerator before /100
    int rhs = num / 100 - (int)n * a;
    int den = b - a;

    int k_est = rhs / den;  // floor division
    if (k_est < 0) k_est = 0;
    if (k_est > n) k_est = n;

    // Round to nearest integer by checking k_est and k_est+1
    int best_k = k_est;
    int best_diff = 1e9;  

    for (int cand = k_est; cand <= k_est + 1 && cand <= n; cand++) {
        int sum_num = cand * b + (n - cand) * a; // numerator
        int sum_den = a * b;                     // denominator

        // compare |sum/den/n - x/100|
        // cross-multiply diff = |sum_num*100 - n*x_percent*sum_den|
        int diff = abs(sum_num * 100 - (int)n * (int)x_percent * sum_den);

        if (diff < best_diff) {
            best_diff = diff;
            best_k = cand;
        }
    }

    uint64_t periods = (uint64_t)a << 48; 
    periods |= (uint64_t)b << 32;
    periods |= (uint64_t)best_k << 16;
    periods |= (uint64_t)(n - best_k);

    return periods;
}

static void usage(const char *progname)
{
    fprintf(stderr, "Usage: %s -i <pid> -e <sample_period>\n", progname);
    fprintf(stderr, "  -i <pid>         Target process ID\n");
    fprintf(stderr, "  -u <cpu>         Target CPU ID or a range (e.g. 0-7)\n");
    fprintf(stderr, "  -p <percentage>  Sample percentage (N%% of LLC misses)\n");
    fprintf(stderr, "  -e <period>      Sample period (every Nth LLC miss)\n");
    fprintf(stderr, "  -n               Set IDT[2] to stock NMI handler\n");
    fprintf(stderr, "  -l               Set IDT[2] to LAME kernel trampoline\n");
    fprintf(stderr, "  -c               Print LAME counter value\n");
    fprintf(stderr, "  -h               Show this help message\n");
    fprintf(stderr, "\nExample: %s -i 1234 -u 0-7 -e 10\n", progname);
    fprintf(stderr, "Example: %s -n\n", progname);
    fprintf(stderr, "Example: %s -l\n", progname);
    fprintf(stderr, "Example: %s -c\n", progname);
}

int main(int argc, char **argv)
{
    pid_t pid = -1;
    uint64_t percentage = 0;
    uint64_t period = 0;
    int cpu_start = -1;         /* range start if specified */
    int cpu_end = -1;           /* range end if specified */
    int do_set_idt2_nmi = 0;
    int do_set_idt2_lame = 0;
    int do_print_lame_counter = 0;
    int opt;
    
    while ((opt = getopt(argc, argv, "i:u:p:e:nlch")) != -1) {
        switch (opt) {
        case 'i':
            pid = atoi(optarg);
            if (pid <= 0) {
                fprintf(stderr, "Error: Invalid PID '%s'\n", optarg);
                return 1;
            }
            break;
        case 'u': {
            char *dash = strchr(optarg, '-');
            if (dash) {
                /* parse range start-end */
                *dash = '\0';
                char *start_str = optarg;
                char *end_str = dash + 1;
                errno = 0;
                long s = strtol(start_str, NULL, 0);
                long e = strtol(end_str, NULL, 0);
                if (errno != 0 || s < 0 || e < 0 || s > e) {
                    fprintf(stderr, "Error: Invalid CPU range '%s-%s'\n", start_str, end_str);
                    return 1;
                }
                cpu_start = (int)s;
                cpu_end = (int)e;
            } else {
                /* parse single cpu */
                errno = 0;
                long c = strtol(optarg, NULL, 0);
                if (errno != 0 || c < 0) {
                    fprintf(stderr, "Error: Invalid CPU '%s'\n", optarg);
                    return 1;
                }
                cpu_start = (int)c;
                cpu_end = (int)c;
            }
            break;
        }
        case 'p':
            percentage = strtoull(optarg, NULL, 0);
            if (percentage <= 0 || percentage > 50) {
                fprintf(stderr, "Error: Invalid sample percentage '%s'; must be between 1 and 50\n", optarg);
                return 1;
            }
            break;
        case 'e':
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
        case 'c':
            do_print_lame_counter = 1;
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
    
    /* option validation */
    int enable_lame_options = 0;
    int exclusive_options = do_set_idt2_nmi + do_set_idt2_lame + do_print_lame_counter;
    
    /* Check if enable_lame options are present */
    if (pid != -1 || cpu_start != -1 || period != 0 || percentage != 0) {
        enable_lame_options = 1;
    }
    
    /* Validation rules:
     * 1. Only one group can be present: either enable_lame options OR exclusive options
     * 2. For enable_lame: at least one of -i or -u must be present
     * 3. For enable_lame: exactly one of -p or -e must be present
     */
    
    /* Rule 1: Only one group can be present */
    if (enable_lame_options && exclusive_options) {
        fprintf(stderr, "Error: Cannot mix enable_lame options (-i, -u, -p, -e) with exclusive options (-n, -l, -c)\n");
        usage(argv[0]);
        return -1;
    }
    
    /* Rule 2: For enable_lame, at least one of -i or -u must be present */
    if (enable_lame_options) {
        if (pid == -1 && cpu_start == -1) {
            fprintf(stderr, "Error: For enable_lame, at least one of -i (PID) or -u (CPU) must be specified\n");
            usage(argv[0]);
            return -1;
        }
        
        /* Rule 3: Exactly one of -p or -e must be present */
        if ((period == 0 && percentage == 0) || (period != 0 && percentage != 0)) {
            fprintf(stderr, "Error: For enable_lame, exactly one of -p (percentage) or -e (period) must be specified\n");
            usage(argv[0]);
            return -1;
        }
    }
    
    /* If no options are specified, show usage */
    if (!enable_lame_options && !exclusive_options) {
        fprintf(stderr, "Error: No valid options specified\n");
        usage(argv[0]);
        return -1;
    }
    /* end */

    if (do_set_idt2_nmi) {
        return set_idt2_nmi();
    }
    else if (do_set_idt2_lame) {
        return set_idt2_lame();
    }
    else if (do_print_lame_counter) {
        return print_lame_counter();
    }
    else if (enable_lame_options) {
        uint64_t sample_periods;
        if (percentage != 0) {
            sample_periods = percentage_to_periods(percentage);
        } else {
            sample_periods = period << 48 | period << 32 | 100 << 16 | 100;
        }
        return enable_lame(pid, cpu_start, cpu_end, sample_periods);
    }
    else {
        fprintf(stderr, "Error: Invalid options\n");
        usage(argv[0]);
        return -1;
    }

    return 0;
}
/* end */
