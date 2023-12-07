#ifndef __PCI_UTILS_H__
#define __PCI_UTILS_H__

/* NOTES

  0xCF8 is the I/O location for CONFIG_ADDRESS
  0xCFC is the I/O location for CONFIG_DATA

  (3,0,0,0) --> Nvidia GTX Titan Black card

  // Nvidia GT 420 card
  (4,0,0,0) --> GPU Vendor ID (0x108e for Nvidia)
  (4,0,0,2) --> GPU Device ID (0xde2 for our card)

  PCI Device. (4,0,x,x)
  register offset purpose
  0        0      deviceID
  0        2      vendorID
  1        4      status
           0x10   BAR #0
           0x14   BAR #1
           0x18   BAR #2
           0x1C   BAR #3
           0x20   BAR #4
           0x2C   BAR #5
*/

extern uint64_t mr, mr1, mr2;
extern uint64_t mrs, mrs1, mrs2;

extern uint64_t act_bar0, act_bar1, act_bar2;

static inline void outl(uint16_t port, uint32_t val)
{
    asm volatile ( "outl %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t ret;
    asm volatile ( "inl %1, %0"
                   : "=a"(ret)
                   : "Nd"(port) );
    return ret;
}

uint16_t pciConfigReadWord (uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;

    /* create configuration address as per Figure 1 */
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));

    /* write out the address */
    outl(0xCF8, address);

    /* read in the data */
    /* (offset & 2) * 8) = 0 will choose the first word of the 32 bits register */
    tmp = (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xffff);
    return (tmp);
}


uint32_t pciConfigReadLong (uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint32_t tmp = 0;

    /* create configuration address as per Figure 1 */
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));

    /* write out the address */
    outl(0xCF8, address);

    /* read in the data */
    /* (offset & 2) * 8) = 0 will choose the first word of the 32 bits register */
    // tmp = ((inl(0xCFC) >> ((offset & 2) * 8)) & 0xffff);

    tmp = inl(0xCFC);
    return (tmp);
}

void map_mmio_range(uint64_t start_paddr, size_t size)
{
  uint64_t chan_paddr = start_paddr + 0xc00000;
  bfdebug_nhex(0, "channel #1: ", chan_paddr);

  // Makes a 4K map of the range
  auto mmio_map =
    bfvmm::x64::make_unique_map<uintptr_t>(chan_paddr);

  auto mmio_map_word = mmio_map.get()[0];

  //bfdebug_nhex(0, "address: ", mmio_map_word);
  // bfdebug_ndec(0, "physical address: ",
  //    ::x64::pt::entry::phys_addr::get(mmio_map_word));
}

void test_pci_config(void)
{
  bfdebug_info(0, "Testing PCI Config Space");
  uint16_t ret;
  uint32_t retl;
  ret = pciConfigReadWord(0,0,0,0);
  bfdebug_nhex(0, "(0,0,0,0) => ", ret);

#if 0
  // Bus 4, Slot 0 is for Nvidia card in our system
  bfdebug_info(0, "Nvidia GT420 card");
  ret = pciConfigReadWord(4,0,0,0);
  bfdebug_nhex(0, "VendorID => ", ret);

  ret = pciConfigReadWord(4,0,0,2);
  bfdebug_nhex(0, "DeviceID => ", ret);

  ret = pciConfigReadWord(4,0,1,4);
  bfdebug_nhex(0, "Status => ", ret);

  ret = pciConfigReadWord(4,0,0,6);
  bfdebug_nhex(0, "Status => ", ret);

  retl = pciConfigReadLong(4,0,0,0x10);
  bfdebug_nhex(0, "BAR#0 => ", retl & 0xFFFFFFFC);

  retl = pciConfigReadLong(4,0,0,0x14);
  bfdebug_nhex(0, "BAR#1 => ", retl & 0xFFFFFFFC);

  retl = pciConfigReadLong(4,0,0,0x18);
  bfdebug_nhex(0, "BAR#2 => ", retl & 0xFFFFFFFC);

  retl = pciConfigReadLong(4,0,0,0x1C);
  bfdebug_nhex(0, "BAR#3 => ", retl & 0xFFFFFFFC);

  retl = pciConfigReadLong(4,0,0,0x20);
  bfdebug_nhex(0, "BAR#4 => ", retl & 0xFFFFFFFC);

  retl = pciConfigReadLong(4,0,0,0x24);
  bfdebug_nhex(0, "BAR#5 => ", retl & 0xFFFFFFFC);

  bfdebug_info(0, "Nvidia GT420 card end\n");
#endif

  // Nvidia Titan Black card
  bfdebug_info(0, "Nvidia Titan Card");
  // Bus 4, Slot 0 is for Nvidia card in our system
  ret = pciConfigReadWord(1,0,0,0);
  bfdebug_nhex(0, "VendorID => ", ret);

  ret = pciConfigReadWord(1,0,0,2);
  bfdebug_nhex(0, "DeviceID => ", ret);

  ret = pciConfigReadWord(1,0,1,4);
  bfdebug_nhex(0, "Status => ", ret);

  ret = pciConfigReadWord(1,0,0,6);
  bfdebug_nhex(0, "Status => ", ret);

  retl = pciConfigReadLong(1,0,0,0x10);
  bfdebug_nhex(0, "BAR#0 => ", retl & 0xFFFFFFFC);
  uint64_t raw_bar0 = retl & 0xFFFFFFFC;

  retl = pciConfigReadLong(1,0,0,0x14);
  bfdebug_nhex(0, "BAR#1 => ", retl & 0xFFFFFFFC);
  uint64_t raw_bar1 = retl & 0xFFFFFFFC;

  retl = pciConfigReadLong(1,0,0,0x18);
  bfdebug_nhex(0, "BAR#2 => ", retl & 0xFFFFFFFC);
  uint64_t raw_bar2 = retl & 0xFFFFFFFC;

  retl = pciConfigReadLong(1,0,0,0x1C);
  bfdebug_nhex(0, "BAR#3 => ", retl & 0xFFFFFFFC);
  uint64_t raw_bar3 = retl & 0xFFFFFFFC;

  retl = pciConfigReadLong(1,0,0,0x20);
  bfdebug_nhex(0, "BAR#4 => ", retl & 0xFFFFFFFC);
  uint64_t raw_bar4 = retl & 0xFFFFFFFC;

  retl = pciConfigReadLong(1,0,0,0x24);
  bfdebug_nhex(0, "BAR#5 => ", retl & 0xFFFFFFFC);

  // Find actual base addresses.

  act_bar0 = raw_bar0 & 0xFFFFFFF0;
  bfdebug_nhex(0, "Base address (32-bit) =>  ", act_bar0);

  act_bar1 = (raw_bar1 & 0xFFFFFFF0) + ((raw_bar2 & 0xFFFFFFFF) << 32);
  bfdebug_nhex(0, "Base address (64-bit) =>  ", act_bar1);

  act_bar2 = (raw_bar3 & 0xFFFFFFF0) + ((raw_bar4 & 0xFFFFFFFF) << 32);
  bfdebug_nhex(0, "Base address (64-bit) =>  ", act_bar2);

  uint64_t bar0_size = 16*1024*1024;
  map_mmio_range(act_bar1, bar0_size);

}

#endif
