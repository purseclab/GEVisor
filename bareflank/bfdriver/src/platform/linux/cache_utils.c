#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <asm-generic/atomic-long.h>

#include <linux/mm.h>
#include <linux/mmzone.h>
#include "common.h"

#define INV_SET_START 512
#define INV_SET_END   528

int paddr_to_cset(uint64_t phys_addr)
{
  static uint64_t mask;
  mask = ((uint64_t) 1 << 17) - 1;
  return (phys_addr & mask) >> 6;
}

bool is_invalid_page(struct page* page)
{
  uint64_t paddr;
  int cset;

  paddr = page_to_phys(page);
  cset = paddr_to_cset(paddr);
  if (cset >= INV_SET_START && cset <= INV_SET_END)
    return true;

  return false;
}
