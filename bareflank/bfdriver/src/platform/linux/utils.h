#ifndef __UTILS_H__
#define __UTILS_H__

struct pglist_data *next_online_pgdat(struct pglist_data *pgdat);
struct pglist_data *first_online_pgdat(void);
struct zone *next_zone(struct zone *zone);

#endif