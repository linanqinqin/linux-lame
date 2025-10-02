/* linanqinqin */
#ifndef _LINUX_LAME_H
#define _LINUX_LAME_H

#include <linux/types.h>

#define LAME_PERIOD_LEFT_COUNT 32 	/* must be a power of 2 */

struct lame_config {
	int is_active;
	unsigned long handler_addr;
	
	/* percentage of LLC misses that will be emulated as LAME; range [1, 100], read as percentage/100 */
	u64 percentage;
	/* a fixed period for resetting the counter */
	s64 period_left; 
	
	/* the pre-calculated left periods for resetting the counter that approximates the percentage */
	// s64 period_left[LAME_PERIOD_LEFT_COUNT];

	/* deprecated */
	// u64 pebs_enable; /* PEBS enable bits; populated in perf_event_open() */
};

#endif /* _LINUX_LAME_H */
/* end */
