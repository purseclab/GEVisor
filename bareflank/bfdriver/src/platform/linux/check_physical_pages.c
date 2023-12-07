/* 
   This file will check the currently loaded
   processes in the OS to find if any of them are actually
   using "invalid" pages.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
// #include <linux/sched/signal.h>
#include <asm-generic/atomic-long.h>

#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/mmzone.h>

#include "utils.h"
#include "common.h"

/* This struct will hold the list of invalid pages */
#define MAX_INVALID_PAGES 10000
typedef struct {
  struct page* pages[MAX_INVALID_PAGES];
  unsigned long nr_count;
} invalid_page;
invalid_page INV_PAGE;

void check_current_processes(void)
{
  int i, j, invalid_pages = 0;
  unsigned long count;
  struct task_struct *task;
  struct zone *zone;
  struct list_head* cur;
  atomic_long_t zone_stats;

  struct free_area *area;
  struct page *page;

  // Reset the count of all invalid pages
  INV_PAGE.nr_count = 0;

  pr_info("[Step-1]. Checking all existing processes\n");
  for_each_process(task)
        pr_info("%s [%d]\n", task->comm, task->pid);

  // Each zone belongs to a pg_data, typically 1 for non-NUMA systems
  // In our case, there is only a single pg_data available for the system.
  pr_info("[Step-2]. Checking all memory zones\n");
  for_each_zone(zone){
      pr_info("-----------------------------------\n");
      pr_info("ZONE: %s\n", zone->name);

      pr_info("start_pfn: %ld, span: %ld\n", zone->zone_start_pfn,
              zone->spanned_pages);

      // getting some zone statistics!
      zone_stats = zone->vm_stat[NR_FREE_PAGES];
      count = atomic_long_read(&zone_stats);
      pr_info("free Pages: %ld\n", count);

      // only check the zones actually configured!
      if (count > 0) {
        for (i = 0; i < 1; i++) {

          pr_info("----------\n");
          pr_info("Num free pages of order %d: %ld\n", i, zone->free_area[i].nr_free);
          area = &(zone->free_area[i]);

          for (j = 1; j < 3; j++) {
            list_for_each(cur, &area->free_list[j]) {
              page = list_entry(cur, struct page, lru);
              // pr_info("Page: %llx\n", page_to_phys(page));
              if (is_invalid_page(page)) {

                // remove from the list
                __list_del_entry(cur);
                area->nr_free--;

                // our own checking
                invalid_pages++;
                INV_PAGE.pages[INV_PAGE.nr_count] = page;
                INV_PAGE.nr_count += 1;
              }

              // SANITY CHECK!
              if (INV_PAGE.nr_count >= MAX_INVALID_PAGES){
                WARN_ON("Filled up all the memory for invalid pages\n");
                INV_PAGE.nr_count = 0;
              }
            }
            pr_info("Total Invalid Pages (Group: %d): %d\n", j, invalid_pages);
            invalid_pages = 0;
          }

          pr_info("----------\n");
        }

      }
      pr_info("-----------------------------------\n");
  }

}
