enum zone_stat_item {
  /* First 128 byte cacheline (assuming 64 bit words) */
  NR_FREE_PAGES,
  NR_LRU_BASE,
  NR_INACTIVE_ANON = NR_LRU_BASE, /* must match order of LRU_[IN]ACTIVE */
  NR_ACTIVE_ANON,   /*  "     "     "   "       "         */
  NR_INACTIVE_FILE, /*  "     "     "   "       "         */
  NR_ACTIVE_FILE,   /*  "     "     "   "       "         */
  NR_UNEVICTABLE,   /*  "     "     "   "       "         */
  NR_MLOCK,   /* mlock()ed pages found and moved off LRU */
  NR_ANON_PAGES,  /* Mapped anonymous pages */
  NR_FILE_MAPPED, /* pagecache pages mapped into pagetables.
         only modified from process context */
  NR_FILE_PAGES,
  NR_FILE_DIRTY,
  NR_WRITEBACK,
  NR_SLAB_RECLAIMABLE,
  NR_SLAB_UNRECLAIMABLE,
  NR_PAGETABLE,   /* used for pagetables */
  NR_KERNEL_STACK,
  /* Second 128 byte cacheline */
  NR_UNSTABLE_NFS,  /* NFS unstable pages */
  NR_BOUNCE,
  NR_VMSCAN_WRITE,
  NR_VMSCAN_IMMEDIATE,  /* Prioritise for reclaim when writeback ends */
  NR_WRITEBACK_TEMP,  /* Writeback using temporary buffers */
  NR_ISOLATED_ANON, /* Temporary isolated pages from anon lru */
  NR_ISOLATED_FILE, /* Temporary isolated pages from file lru */
  NR_SHMEM,   /* shmem pages (included tmpfs/GEM pages) */
  NR_DIRTIED,   /* page dirtyings since bootup */
  NR_WRITTEN,   /* page writings since bootup */
#ifdef CONFIG_NUMA
  NUMA_HIT,   /* allocated in intended node */
  NUMA_MISS,    /* allocated in non intended node */
  NUMA_FOREIGN,   /* was intended here, hit elsewhere */
  NUMA_INTERLEAVE_HIT,  /* interleaver preferred this zone */
  NUMA_LOCAL,   /* allocation from local node */
  NUMA_OTHER,   /* allocation from other node */
#endif
  NR_ANON_TRANSPARENT_HUGEPAGES,
  NR_VM_ZONE_STAT_ITEMS };

        /* free areas of different sizes
          MAX_ORDER => 11
          i.e, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, and 1024 contiguous page frames
          struct zone {
            struct free_area free_area[MAX_ORDER];
          }
          struct free_area {
            struct list_head free_list[MIGRATE_TYPES];
            unsigned long nr_free;
          }
          enum migratetype {
            MIGRATE_UNMOVABLE,
            MIGRATE_MOVABLE,
            MIGRATE_RECLAIMABLE,
            MIGRATE_PCPTYPES
            MIGRATE_HIGHATOMIC = MIGRATE_PCPTYPES,
            #ifdef CONFIG_CMA
              MIGRATE_CMA,
            #endif
            #ifdef CONFIG_MEMORY_ISOLATION
              MIGRATE_ISOLATE,
            #endif
            MIGRATE_TYPES
          };
        */



static __always_inline
struct page *__rmqueue_smallest(struct zone *zone, unsigned int order,
            int migratetype)
{
  unsigned int current_order;
  struct free_area *area;
  struct page *page;

  /* Find a page of the appropriate size in the preferred list */
  for (current_order = order; current_order < MAX_ORDER; ++current_order) {
    area = &(zone->free_area[current_order]);
    page = list_first_entry_or_null(&area->free_list[migratetype],
              struct page, lru);
    if (!page)
      continue;
    list_del(&page->lru);
    rmv_page_order(page);
    area->nr_free--;
    expand(zone, page, order, current_order, area, migratetype);
    set_pcppage_migratetype(page, migratetype);
    return page;
  }

  return NULL;
}

void split_page(struct page *page, unsigned int order)
{
  int i;

  VM_BUG_ON_PAGE(PageCompound(page), page);
  VM_BUG_ON_PAGE(!page_count(page), page);

  for (i = 1; i < (1 << order); i++)
    set_page_refcounted(page + i);
  split_page_owner(page, order);
}
EXPORT_SYMBOL_GPL(split_page);