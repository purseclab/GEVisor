#ifndef __PAGING_UTILS_H_
#define __PAGING_UTILS_H_

#define SGX_PAGES 100000

extern uint64_t epc_base_address;
extern uint64_t epc_size;
extern uint64_t modified_pages;
extern uint64_t channel_paddr ;

extern int global_pml4;
extern int global_pdpt;
extern int global_pdt;
extern int global_pt;

// EPT-based PTEs for all enclave pages
extern eapis::intel_x64::ept::mmap::entry_type sgx_guest_pte_shadow[];
extern eapis::intel_x64::ept::mmap g_guest_map;
extern uint64_t sgx_paddrs[];
extern uint64_t sgx_vaddrs[];

extern uint64_t mr, mr1, mr2;
extern uint64_t mrs, mrs1, mrs2;

uint64_t tmp_vaddr = 0;

void locate_epc() {
  unsigned int eax, ebx, ecx, edx;
  eax = 0x12;     // CPUID for EPC
  ecx = 2;        // What linux SGX driver says!

  // call cpuid
  _cpuid(&eax, &ebx, &ecx, &edx);

  // Taken from the SGX driver
  epc_base_address = ((uint64_t)(ebx & 0xfffff) << 32) + (uint64_t)(eax & 0xfffff000);
  epc_size = ((uint64_t)(edx & 0xfffff) << 32) + (uint64_t)(ecx & 0xfffff000);

  bfdebug_nhex(0, "EPC BASE:", epc_base_address);
  bfdebug_nhex(0, "EPC SIZE:", epc_size);
  bfdebug_nhex(0, "EPC END:", epc_base_address+epc_size);
}

int check_pages = 0;

bool access_all_pt(uint64_t pd_pte) {
  uintptr_t from = ::x64::pt::from;
  uint64_t pt_paddr = 0;
  bool flag = false;
  auto pt_map =
    bfvmm::x64::make_unique_map<uintptr_t>(::x64::pd::entry::phys_addr::get(pd_pte));
  unsigned long last, tocheck = 0;

  global_pt = 0;

  for (auto pt_idx = 0; pt_idx < 512; pt_idx++) {
    auto pt_pte = pt_map.get()[pt_idx];
    pt_paddr = ::x64::pt::entry::phys_addr::get(pt_pte);

    //if ((::x64::pt::entry::phys_addr::get(pt_pte) != 0) &&
    //  (::x64::pt::entry::present::is_enabled(pt_pte))) {

    if ((::x64::pt::entry::phys_addr::get(pt_pte) != 0)) {
      if (pt_paddr >= epc_base_address &&
            pt_paddr <= (epc_base_address+epc_size)) {
        ::x64::pt::entry::accessed::enable(pt_pte);
        if (modified_pages < SGX_PAGES) {
          sgx_paddrs[modified_pages] = pt_paddr;
        }
        modified_pages++;
        flag = true;
      }

        //if (tmp_vaddr >= 0x7f0000000000 && tmp_vaddr <=0x800000000000 && check_pages<600) {
        //  bfdebug_nhex(0, "physical address: ", pt_paddr);
        //  bfdebug_nhex(0, "virtual address: ", tmp_vaddr);
        //  check_pages++;
        // }

        //if ((pt_paddr >= mr && pt_paddr <= mr + mrs) ||
        //  (pt_paddr >= mr1 && pt_paddr <= mr1 + mrs1) ||
        //  (pt_paddr >= mr2 && pt_paddr <= mr2 + mrs2))
        //{


        /*
        if (pt_paddr >= 0xf00000000 && pt_paddr <= 0xf0000000000){
          uint64_t tm1 = global_pml4*512*512*512;
          uint64_t tm2 = tm1*4096;

          tmp_vaddr = tm2 + (0x40000000*global_pdpt) +
                         (0x200000*global_pdt) +
                         (0x1000*global_pt);


          bfdebug_nhex(0, "physical address: ", pt_paddr);
          bfdebug_nhex(0, "virtual address: ", tmp_vaddr);
        }
        */
      }
    global_pt++;

  }

  return flag;
}

void access_all_pdt(uint64_t pdpt_pte) {
  uintptr_t from = ::x64::pd::from;
  bool flag = false;
  auto pd_map =
     bfvmm::x64::make_unique_map<uintptr_t>(::x64::pdpt::entry::phys_addr::get(pdpt_pte));

  global_pdt = 0;

  for (auto pd_idx = 0; pd_idx < 512; pd_idx++) {
    auto pd_pte = pd_map.get()[pd_idx];

    if((::x64::pd::entry::phys_addr::get(pd_pte) != 0)
       && (::x64::pd::entry::present::is_enabled(pd_pte))) {
      // XXX. check this out
      if (!(::x64::pd::entry::ps::is_enabled(pd_pte))) {
        flag = access_all_pt(pd_pte);

        // basically avoid any changes to the page tables
        if (flag == true) {
          ::x64::pd::entry::rw::disable(pd_pte);
        }
      }
    }
    global_pdt++;
  }
}

void access_all_pdpt(uint64_t pml4_pte) {
  uintptr_t from;
  from = ::x64::pdpt::from;
  auto pdpt_map =
    bfvmm::x64::make_unique_map<uintptr_t>(::x64::pml4::entry::phys_addr::get(pml4_pte));

  global_pdpt = 0;

  // LEVEL 2: PDPT
  for (auto pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
        auto pdpt_pte = pdpt_map.get()[pdpt_idx];

        if ((::x64::pdpt::entry::phys_addr::get(pdpt_pte) != 0) &&
          (::x64::pdpt::entry::present::is_enabled(pdpt_pte))) {
          // XXX. check this out
          if (!(::x64::pdpt::entry::ps::is_enabled(pdpt_pte))) {
            access_all_pdt(pdpt_pte);
          }
        }
        global_pdpt++;
    }
}


void access_all_pml4(uint64_t cr3) {
  uintptr_t from;
  expects(cr3 != 0);
  auto pml4_map =
       bfvmm::x64::make_unique_map<uintptr_t>(bfn::upper(cr3));

  global_pml4 = 0;

  for (auto pml4_idx = 0; pml4_idx < 512; pml4_idx++) {
    from = ::x64::pml4::from;
    auto pml4_pte = pml4_map.get()[pml4_idx];

    // make sure the entry is present
    if ((::x64::pml4::entry::phys_addr::get(pml4_pte) != 0) &&
       (::x64::pml4::entry::present::is_enabled(pml4_pte)))
    {
      access_all_pdpt(pml4_pte);
    }

    global_pml4++;
  }
  //bfdebug_ndec(0, "[*] EPC Setup Complete!", 2);
}

inline uintptr_t
custom_virt_to_phys_with_cr3(
    uintptr_t virt, uintptr_t cr3) 
{
    uintptr_t from;

    expects(cr3 != 0);
    expects(virt != 0);

    from = ::x64::pml4::from;
    auto pml4_idx = ::x64::pml4::index(virt);
    auto pml4_map = bfvmm::x64::make_unique_map<uintptr_t>(bfn::upper(cr3));
    auto pml4_pte = pml4_map.get()[pml4_idx];

    if (::x64::pml4::entry::phys_addr::get(pml4_pte) == 0 ||
        ::x64::pml4::entry::present::is_enabled(pml4_pte) == 0) {
      return 0x0;
    }

    from = ::x64::pdpt::from;
    auto pdpt_idx = ::x64::pdpt::index(virt);
    auto pdpt_map = bfvmm::x64::make_unique_map<uintptr_t>(::x64::pml4::entry::phys_addr::get(pml4_pte));
    auto pdpt_pte = pdpt_map.get()[pdpt_idx];

    if (::x64::pdpt::entry::phys_addr::get(pdpt_pte) == 0 ||
      ::x64::pdpt::entry::present::is_enabled(pdpt_pte) == 0) {
      return 0x0;
    }

    if (::x64::pdpt::entry::ps::is_enabled(pdpt_pte)) {
        return bfn::upper(::x64::pdpt::entry::phys_addr::get(pdpt_pte), from) |
               bfn::lower(virt, from);
    }

    from = ::x64::pd::from;
    auto pd_idx = ::x64::pd::index(virt);
    auto pd_map = bfvmm::x64::make_unique_map<uintptr_t>(::x64::pdpt::entry::phys_addr::get(pdpt_pte));
    auto pd_pte = pd_map.get()[pd_idx];

    if (::x64::pd::entry::phys_addr::get(pd_pte) == 0 ||
        ::x64::pd::entry::present::is_enabled(pd_pte) == 0) {
      return 0x0;
    }

    if (::x64::pd::entry::ps::is_enabled(pd_pte)) {
        return bfn::upper(::x64::pd::entry::phys_addr::get(pd_pte), from) |
               bfn::lower(virt, from);
    }

    from = ::x64::pt::from;
    auto pt_idx = ::x64::pt::index(virt);
    auto pt_map = bfvmm::x64::make_unique_map<uintptr_t>(::x64::pd::entry::phys_addr::get(pd_pte));
    auto pt_pte = pt_map.get()[pt_idx];

    if(::x64::pt::entry::phys_addr::get(pt_pte) == 0 ||
       ::x64::pt::entry::present::is_enabled(pt_pte) == 0) {
      return 0x0;
    }

    return bfn::upper(::x64::pt::entry::phys_addr::get(pt_pte), from) |
           bfn::lower(virt, from);
}

bool
change_phy_addr(
    uintptr_t virt, uintptr_t phy, uintptr_t cr3)
{
    uintptr_t from;

    expects(cr3 != 0);
    expects(virt != 0);

    from = ::x64::pml4::from;
    auto pml4_idx = ::x64::pml4::index(virt);
    auto pml4_map = bfvmm::x64::make_unique_map<uintptr_t>(bfn::upper(cr3));
    auto pml4_pte = pml4_map.get()[pml4_idx];

    if (::x64::pml4::entry::phys_addr::get(pml4_pte) == 0 ||
        ::x64::pml4::entry::present::is_enabled(pml4_pte) == 0) {
      return false;
    }

    from = ::x64::pdpt::from;
    auto pdpt_idx = ::x64::pdpt::index(virt);
    auto pdpt_map = bfvmm::x64::make_unique_map<uintptr_t>(::x64::pml4::entry::phys_addr::get(pml4_pte));
    auto pdpt_pte = pdpt_map.get()[pdpt_idx];

    if (::x64::pdpt::entry::phys_addr::get(pdpt_pte) == 0 ||
      ::x64::pdpt::entry::present::is_enabled(pdpt_pte) == 0) {
      return false;
    }

    if (::x64::pdpt::entry::ps::is_enabled(pdpt_pte)) {
      //bfdebug_info(0, "[WRNG] Unkown behavior");
      //return false;
      ::x64::pdpt::entry::phys_addr::set(pdpt_pte, phy);
      return true;
    }

    from = ::x64::pd::from;
    auto pd_idx = ::x64::pd::index(virt);
    auto pd_map = bfvmm::x64::make_unique_map<uintptr_t>(::x64::pdpt::entry::phys_addr::get(pdpt_pte));
    auto pd_pte = pd_map.get()[pd_idx];

    if (::x64::pd::entry::phys_addr::get(pd_pte) == 0 ||
        ::x64::pd::entry::present::is_enabled(pd_pte) == 0) {
      return false;
    }

    if (::x64::pd::entry::ps::is_enabled(pd_pte)) {
        ::x64::pd::entry::phys_addr::set(pd_pte, phy);
        return true;
    }

    from = ::x64::pt::from;
    auto pt_idx = ::x64::pt::index(virt);
    auto pt_map = bfvmm::x64::make_unique_map<uintptr_t>(::x64::pd::entry::phys_addr::get(pd_pte));
    auto pt_pte = pt_map.get()[pt_idx];

    if(::x64::pt::entry::phys_addr::get(pt_pte) == 0 ||
       ::x64::pt::entry::present::is_enabled(pt_pte) == 0) {
      return 0x0;
    }

    ::x64::pt::entry::phys_addr::set(pt_pte, phy);
    return true;
}

inline void
change_page_to_user_mode(uintptr_t virt, uintptr_t cr3)
{
    uintptr_t from;

    expects(cr3 != 0);
    expects(virt != 0);

    from = ::x64::pml4::from;
    auto pml4_idx = ::x64::pml4::index(virt);
    auto pml4_map = bfvmm::x64::make_unique_map<uintptr_t>(bfn::upper(cr3));
    auto pml4_pte = pml4_map.get()[pml4_idx];

    if (::x64::pml4::entry::phys_addr::get(pml4_pte) == 0 ||
        ::x64::pml4::entry::present::is_enabled(pml4_pte) == 0) {
      return;
    }

    from = ::x64::pdpt::from;
    auto pdpt_idx = ::x64::pdpt::index(virt);
    auto pdpt_map =
        bfvmm::x64::make_unique_map<uintptr_t>(::x64::pml4::entry::phys_addr::get(pml4_pte));
    auto pdpt_pte = pdpt_map.get()[pdpt_idx];

    if (::x64::pdpt::entry::phys_addr::get(pdpt_pte) == 0 ||
      ::x64::pdpt::entry::present::is_enabled(pdpt_pte) == 0) {
      return;
    }

    if (::x64::pdpt::entry::ps::is_enabled(pdpt_pte)) {
      return;
    }

    from = ::x64::pd::from;
    auto pd_idx = ::x64::pd::index(virt);
    auto pd_map = bfvmm::x64::make_unique_map<uintptr_t>(::x64::pdpt::entry::phys_addr::get(pdpt_pte));
    auto pd_pte = pd_map.get()[pd_idx];

    if (::x64::pd::entry::phys_addr::get(pd_pte) == 0 ||
        ::x64::pd::entry::present::is_enabled(pd_pte) == 0) {
      return;
    }

    if (::x64::pd::entry::ps::is_enabled(pd_pte)) {
      return;
    }

    from = ::x64::pt::from;
    auto pt_idx = ::x64::pt::index(virt);
    auto pt_map = bfvmm::x64::make_unique_map<uintptr_t>(::x64::pd::entry::phys_addr::get(pd_pte));
    auto pt_pte = pt_map.get()[pt_idx];

    if(::x64::pt::entry::phys_addr::get(pt_pte) == 0 ||
       ::x64::pt::entry::present::is_enabled(pt_pte) == 0) {
      return;
    }

    // debugging
    bfdebug_info(0, "Found the required page");
    bfdebug_ndec(0, "User Mode: ", ::x64::pt::entry::us::is_enabled(pt_pte));

    // change to user-mode
    ::x64::pt::entry::us::enable(pt_pte);
    bfdebug_info(0, "Set page to user");
    bfdebug_ndec(0, "User Mode: ", ::x64::pt::entry::us::is_enabled(pt_pte));
}

#endif
