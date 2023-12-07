#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <asm-generic/atomic-long.h>

#include <linux/mm.h>
#include <linux/mmzone.h>

#include "common.h"


struct pglist_data *next_online_pgdat(struct pglist_data *pgdat)
{
  int nid = next_online_node(pgdat->node_id);

  if (nid == MAX_NUMNODES)
    return NULL;
  return NODE_DATA(nid);
}

struct pglist_data *first_online_pgdat(void)
{
  return NODE_DATA(first_online_node);
}

struct zone *next_zone(struct zone *zone)
{
  pg_data_t *pgdat = zone->zone_pgdat;
  if (zone < pgdat->node_zones + MAX_NR_ZONES - 1)
    zone++;
  else {
    pgdat = next_online_pgdat(pgdat);
    if (pgdat)
      zone = pgdat->node_zones;
    else
      zone = NULL;
  }
  return zone;
}
