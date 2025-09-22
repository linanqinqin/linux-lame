/* linanqinqin */
#ifndef _LINUX_LAME_H
#define _LINUX_LAME_H

#include <linux/types.h>

struct lame_config {
	int is_active;
	unsigned long handler_addr;
	u64 sample_period;
	u64 pebs_enable; /* PEBS enable bits; populated in perf_event_open() */
};

#endif /* _LINUX_LAME_H */
/* end */
