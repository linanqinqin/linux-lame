/* linanqinqin */
#ifndef _LINUX_LAME_H
#define _LINUX_LAME_H

#include <linux/types.h>

#define LAME_PERIODS_COUNT 2 	

struct lame_config {
	int is_active;
	unsigned long handler_addr;
	
	bool do_upcall;
	bool do_stall;
	u64 stall_duration;

	/* a list of fixed periods for resetting the counter */
	s64 sample_periods[LAME_PERIODS_COUNT]; 
	/* number of occurrences for each sample period */
	u64 num_occurrences[LAME_PERIODS_COUNT];
	
	/* deprecated */
	// u64 pebs_enable; /* PEBS enable bits; populated in perf_event_open() */
};

struct lame_context {
	u64 last_deadline_tsc; 
};

#endif /* _LINUX_LAME_H */
/* end */
