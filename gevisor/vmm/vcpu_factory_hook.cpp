#include <bfcallonce.h>
#include <bfdebug.h>
#include <bfvmm/vcpu/vcpu_factory.h>
#include <bfvmm/memory_manager/arch/x64/unique_map.h>
#include <eapis/hve/arch/intel_x64/vcpu.h>
#include <eapis/hve/arch/intel_x64/encls.h>
#include <eapis/hve/arch/intel_x64/apis.h>

#include "lapic_utils.h"
#include "paging_utils.h"
#include "pci_utils.h"
#include "lapic.h"

using namespace eapis::intel_x64;

#define ALIGN(x,a) ((uint64_t)((x)/(a))*(a))

// -----------
// GPU-related
// -----------

#define GPU_BUFFER_NUM 100
#define GPU_PADDR_NUM (1024*1024)
#define ENCLU 0xd7010f
char key[10]="ABCDEFGHI";
typedef struct{
  uint64_t vaddr;
  uint64_t paddr;
  uint64_t size;
  uint64_t num_pages;
}gpu_buffer_t;

int count_gpu_buffers = 0;
gpu_buffer_t gpu_buffers[GPU_BUFFER_NUM];

int count_gpu_paddrs = 0;
unsigned long gpu_paddrs[GPU_PADDR_NUM];


// -----------------------------------------------------------------------------
// vCPU
// -----------------------------------------------------------------------------
uint64_t epc_base_address = 0;
uint64_t epc_size = 0;
int count_sgx_paddrs = 0;
uint64_t modified_pages = 0;
uint64_t channel_paddr = 0;

// debugging related
int page_num = 0;
int global_pml4 = 0;
int global_pdpt = 0;
int global_pdt = 0;
int global_pt = 0;

// assuming only one enclave at a time.
uint64_t current_enclave_cr3 = 0;
int enclave_vcpuid = -1;
unsigned long enclave_aep = 0;

bool handle_interrupt = true;
bool enclave_running = false;
bool channel_running = false;

// EPT-related maps
ept::mmap g_guest_map{};
eapis::intel_x64::ept::mmap::entry_type sgx_guest_pte_shadow[SGX_PAGES];

uint64_t sgx_paddrs[SGX_PAGES];
uint64_t sgx_vaddrs[SGX_PAGES];
int cache_flag = 0;

uint64_t mr, mr1, mr2;
uint64_t mrs, mrs1, mrs2;

uint64_t act_bar0, act_bar1, act_bar2;
uint64_t act_bar2_size = 32*1024*1024;

bfvmm::x64::unique_map_ptr<uintptr_t> channel_map;

namespace test
{

// For setting up EPT once the hypervisor boots up
bfn::once_flag flag_ept{};
bfn::once_flag flag_epc{};
bfn::once_flag flag_cat{};
bfn::once_flag flag_aep{};
bfn::once_flag flag_pci{};

ept::mmap::entry_type tmp_guest_pte_shadow{};
ept::mmap::entry_type g_guest_pte_shadow{};
ept::mmap::entry_type enclu_pte_shadow{};

char dummy_page[4096] __attribute__((aligned(4096)));

class vcpu : public eapis::intel_x64::vcpu
{
private:
  const unsigned long EINIT_EXIT  = 0x02;
  const unsigned long EREMOVE_EXIT = 0x03;
  const unsigned long ERESUME_EXIT =0x03;

  std::reference_wrapper<ept::mmap::entry_type> tmp_pte{tmp_guest_pte_shadow};
  std::reference_wrapper<ept::mmap::entry_type> m_pte{g_guest_pte_shadow};
  std::reference_wrapper<ept::mmap::entry_type> enclu_pte{enclu_pte_shadow};

  std::list<uint64_t, object_allocator<uint64_t, 1>> m_vectors;

public:

    vcpu(vcpuid::type id) :
        eapis::intel_x64::vcpu{id}
    {
        using wrcr3_delegate_t = control_register_handler::handler_delegate_t;
        using mt_delegate_t = monitor_trap_handler::handler_delegate_t;
        using eptv_delegate_t = ept_violation_handler::handler_delegate_t;

#if 1
        // bfdebug_ndec(0, "Enabling ENCLS Trapping!", 10);
        eapis()->enable_encls();

        // add an exit handler which corresponds to the execution of ENCLS
        // instruction. You also have to set the ENCLS bitmap indices corresponding
        // to the type of ENCLS to ensure that the trap actually happens.
        exit_handler()->add_handler(
            intel_x64::vmcs::exit_reason::basic_exit_reason::encls,
            ::handler_delegate_t::create<vcpu, &vcpu::encls_handler>(this)
        );
#endif
        bfdebug_ndec(0, "after ENCLS Trapping!", 10);
        // add an EPT violation handler (for read access)
        eapis()->add_ept_read_violation_handler(
          eptv_delegate_t::create<vcpu, &vcpu::ept_read_violation_handler>(this)
        );
        bfdebug_ndec(0, "after ept_read Trapping!", 10);
        // add an EPT violation handler (for write access)
        eapis()->add_ept_write_violation_handler(
          eptv_delegate_t::create<vcpu, &vcpu::ept_write_violation_handler>(this)
        );

        // add an EPT violation handler (for execute access)
        eapis()->add_ept_execute_violation_handler(
          eptv_delegate_t::create<vcpu, &vcpu::ept_execute_violation_handler>(this)
        );

        bfdebug_ndec(0, "encls_exit_bitmap exists:",
          ::intel_x64::vmcs::encls_exiting_bitmap::exists());

#if 1
        unsigned long encls_exiting_set = 0x0;
        encls_exiting_set |= 1 << EINIT_EXIT; 
        encls_exiting_set |= 1 << EREMOVE_EXIT;
        ::intel_x64::vmcs::encls_exiting_bitmap::set(encls_exiting_set);

        // Locate the physical address of the EPC
        bfn::call_once(flag_epc, [&] {locate_epc();});
#endif

#if 1
        bfn::call_once(flag_ept, [&] {
            ept::identity_map(
                g_guest_map,
                MAX_PHYS_ADDR
            );
        });
        eapis()->set_eptp(g_guest_map);
        bfdebug_ndec(0, "[*] EPT Setup Complete!", 1);
#endif

#if 1
        // Testing the external interrupt handler
        eapis()->add_external_interrupt_handler(
             external_interrupt_handler::handler_delegate_t::create<vcpu,
            &vcpu::external_interrupt_handler>(this)
        );
#endif

        // Adding for VM Call
        exit_handler()->add_handler(
            intel_x64::vmcs::exit_reason::basic_exit_reason::vmcall,
            ::handler_delegate_t::create<vcpu, &vcpu::vmcall_handler>(this)
        );

#if 1 
        // Testing interrupt window handler
        eapis()->add_interrupt_window_handler(
             interrupt_window_handler::handler_delegate_t::create<vcpu,
            &vcpu::interrupt_window_handler>(this)
        );
        bfdebug_info(0, "Interrupt Handlers Setup Complete!\n");
#endif

        // monitor trap handler
        eapis()->add_monitor_trap_handler(
             monitor_trap_handler::handler_delegate_t::create<vcpu,
            &vcpu::monitor_trap_handler>(this)
        );
        bfdebug_info(0, "Interrupt Handlers Setup Complete!\n");

        // Add a trap for IN/OUT instructions
        // eapis()->add_handler(
        //  intel_x64::vmcs::exit_reason::basic_exit_reason::io_instruction,
        //  ::handler_delegate_t::create<vcpu, &vcpu::io_instruction_handler>(this)
        //);

#if 0
        eapis()->add_io_instruction_handler(
          0xCF8,
          io_instruction_handler::handler_delegate_t::create<vcpu,
            &vcpu::io_instruction_handler>(this),
          io_instruction_handler::handler_delegate_t::create<vcpu,
            &vcpu::io_instruction_handler>(this)
        );

        eapis()->add_io_instruction_handler(
          0xCFC,
          io_instruction_handler::handler_delegate_t::create<vcpu,
            &vcpu::io_instruction_handler>(this),
          io_instruction_handler::handler_delegate_t::create<vcpu,
            &vcpu::io_instruction_handler>(this)
        );
#endif

        // Testing PCI configuration
        bfn::call_once(flag_pci, [&] {
        test_pci_config();
        });

#if 0
        // Testing the BARS
        remove_permission_bars();
#endif

        remove_permission_channel();
    }

    bool io_instruction_handler(gsl::not_null<vmcs_t *> vmcs,
        io_instruction_handler::info_t &info)
    {
      bfignored(vmcs);
      bfignored(info);
      if (enclave_running) {
        bfdebug_info(0, "I/O Access");
        bfdebug_ndec(0, "Port: ", info.port_number);
      }
      return false;
    }

    void gpu_prog_init(gsl::not_null<vmcs_t *> vmcs)
    {
      uint64_t cr3 = ::intel_x64::vmcs::guest_cr3::get();
      bfdebug_info(0, "Checking the GPU pages allocated\n");
      access_all_pml4(cr3);
    }

    bool
    vmcall_handler(
        gsl::not_null<vmcs_t *> vmcs)
    {
        guard_exceptions([&] {
            switch(vmcs->save_state()->rax) {
                case 0:
                    bfdebug_info(0, "GPU Program Starts!");
                    gpu_prog_init(vmcs);
                    break;

                default:
                    bfdebug_info(0, "GPU Program Terminates!");
                    break;
            };
        });
        return advance(vmcs);
    }

    unsigned long inter_count = 1000000;
    bool
    interrupt_window_handler(
        gsl::not_null<vmcs_t *> vmcs, interrupt_window_handler::info_t &info)
    {
        bfignored(vmcs);
        bfignored(info);

        eapis()->inject_external_interrupt(m_vectors.back());
        m_vectors.pop_back();
        if (!m_vectors.empty()) {
            info.ignore_disable = true;
        }
        return true;
    }

    bool
    external_interrupt_handler(
        gsl::not_null<vmcs_t *> vmcs, external_interrupt_handler::info_t &info)
    {
        bfignored(vmcs);
        // bfdebug_info(0, "external_interrupt_handler!");
        // 0xEE -> Hypervisor Interrupt, I'm guessing.
        if (enclave_running && (vmcs->save_state()->vcpuid == enclave_vcpuid)){
          read_enclave_channel(::intel_x64::vmcs::guest_cr3::get());
          trap_enclave_entry_new_2m();
         // trap_enclave_entry_new();
         // trap_enclave_entry();
         // remove_permission_channel();
          enclave_running = false;
       }

        if (eapis()->is_interrupt_window_open()) {
            eapis()->inject_external_interrupt(info.vector);
        }
        else {
            eapis()->trap_on_next_interrupt_window();
            m_vectors.push_back(info.vector);
        }


        return true;
    }

    bool encls_handler(
        gsl::not_null<vmcs_t *> vmcs)
    {
        bfignored(vmcs);

        // Find the value of saved RAX
        // Check the reason for EXIT
        unsigned long encls_exiting_set = 0x0;
        unsigned long rax = vmcs->save_state()->rax;

        if (rax == EREMOVE_EXIT) {
          enclave_running = false;
          encls_exiting_set = 0x0;
          encls_exiting_set |= 1 << EINIT_EXIT;
          ::intel_x64::vmcs::encls_exiting_bitmap::set(encls_exiting_set);

          // RESET.
          current_enclave_cr3 = 0;
          page_num = 0;
          global_pml4 = 0;
          global_pdpt = 0;
          global_pdt = 0;
          global_pt = 0;

          for (int i = 0; i < modified_pages; i++) {
            sgx_vaddrs[i] = 0;
            sgx_paddrs[i] = 0;
          }

          count_gpu_buffers = 0;
          count_gpu_paddrs = 0;
          count_sgx_paddrs = 0;
          modified_pages = 0;
          enclave_vcpuid = -1;
          return true;
        }

        bfdebug_info(0, "-------- ENCLAVE Starts -------");
        bfdebug_info(0, "ENCLS_HANDLER: EINIT called!");

#ifdef DEBUG
        // Checking the value of guest CR0.WP
        bfdebug_ndec(0, "Guest CR0.WP",
            ::intel_x64::vmcs::guest_cr0::write_protect::is_enabled());
#endif

        // find the address of CR3 (host CR3)
        uint64_t cr3 = ::intel_x64::cr3::get();

        // find the address of CR3 (guest CR3)
        cr3 = ::intel_x64::vmcs::guest_cr3::get();
        current_enclave_cr3 = cr3;

        // Taken from virt_to_phys_with_cr3(...)
        // if the ENCLS is ECREATE, set all the access bits of the program
        // and simply write-protect the pagetable by setting CR0.WP bit in
        // the guest CR0.

        modified_pages = 0;
      // access_all_pml4(cr3);
       add_epc_address_2m();
      // add_epc_address();
        bfdebug_ndec(0, "Enclave Pages: ", modified_pages);

        enclave_running = false;
       // find_enclave_channel(cr3);
        trap_enclave_entry_new_2m();
       // trap_enclave_entry_new();
       // trap_enclave_entry();
        remove_permission_channel(); 

        encls_exiting_set = 0x0;
        encls_exiting_set |= 1 << EREMOVE_EXIT;
        ::intel_x64::vmcs::encls_exiting_bitmap::set(encls_exiting_set);

        bfdebug_info(0, "[*] Finished ENCLS Setup\n\n");
        return true;
    }

    bool monitor_trap_handler(
        gsl::not_null<vmcs_t *> vmcs, monitor_trap_handler::info_t &info)
    {
        bfignored(vmcs);
        bfignored(info);

#if 0
        remove_permission_bars();
#endif

        return true;
    }

    bool ept_write_violation_handler(
        gsl::not_null<vmcs_t *> vmcs, ept_violation_handler::info_t &info)
    {

        bfignored(vmcs);
        bfignored(info);

        bfdebug_info(0, "[X] EPT Write Violation");
        bfdebug_nhex(0, "GPA: ", info.gpa);
        bfdebug_nhex(0, "GVA: ", info.gva);
         unsigned long rax = vmcs->save_state()->rax;
         unsigned long instr = vmcs->save_state()->rip;
         bfdebug_nhex(0, "RAX: ", rax);
         bfdebug_nhex(0, "INSTR: ", instr);
         bfdebug_nhex(0, "ENCLU: ", instr&0xffffff);
        if((ENCLU == (instr & 0xffffff)) && (rax == ERESUME_EXIT)){
          grant_permission_channel();
          bfdebug_info(0, "[X] grant_permission_channel");
        }
      
        grant_permission_channel();  
        eapis()->enable_monitor_trap_flag();
#if 0
        grant_permission_bars();
        grant_permission_gpu_buffers();
#endif
       //if ((info.gpa == gpu_buffers[count_gpu_buffers].paddr) && (info.gva == gpu_buffers[count_gpu_buffers].vaddr))
       //grant_permission_gpu_buffers();

        bfdebug_nhex(0, "[X] GVA", info.gva);
        uint64_t aligned = ALIGN(info.gpa, 4096);
        bfdebug_nhex(0, "[X] Aligned GVA", aligned);
        tmp_pte = g_guest_map.entry(aligned);
        ::intel_x64::ept::pt::entry::write_access::enable(tmp_pte);
        eapis()->set_eptp(g_guest_map);
        ::intel_x64::vmx::invept_global();

        return true;
    }

    bool ept_read_violation_handler(
        gsl::not_null<vmcs_t *> vmcs, ept_violation_handler::info_t &info)
    {
        bfignored(vmcs);
        bfignored(info);

        bfdebug_info(0, "[X] EPT Read Violation");
        // bfdebug_nhex(0, "[X] Virt Addr", info.gva);
        // bfdebug_nhex(0, "[X] Phy Addr", info.gpa);
        // bfdebug_info(0, "[X] \n");

        eapis()->enable_monitor_trap_flag();
#if 0
        grant_permission_gpu_buffers();
#endif
        bfdebug_nhex(0, "[X] GVA", info.gva);
        uint64_t aligned = ALIGN(info.gpa, 4096);
        bfdebug_nhex(0, "[X] Aligned GVA", aligned);
        tmp_pte = g_guest_map.entry(aligned);
        ::intel_x64::ept::pt::entry::read_access::enable(tmp_pte);
        eapis()->set_eptp(g_guest_map);
        ::intel_x64::vmx::invept_global();


        return true;
    }

    bool ept_execute_violation_handler(
        gsl::not_null<vmcs_t *> vmcs, ept_violation_handler::info_t &info)
    {
        bfignored(vmcs);
        bfignored(info);

     // bfdebug_info(0, "[X] EPT Execute Violation");

        if (enclave_running == false) {
          //bfdebug_info(0, "[*] Enclave Starts");
          enclave_running = true;
        }

        // set_enclave_l3_cos();
        enclave_vcpuid = vmcs->save_state()->vcpuid;
        enclave_aep = vmcs->save_state()->rip;
     /* unsigned long rax = vmcs->save_state()->rax;
        unsigned long instr = vmcs->save_state()->rip;
        if((ENCLU == (instr & 0xffffff)) && (rax == ERESUME_EXIT)){
          grant_permission_channel();
          bfdebug_info(0, "[X] grant_permission_channel");
        }*/
         



        for (int i = 0; i < modified_pages; i++) {
          tmp_pte = g_guest_map.entry(sgx_paddrs[i]);
          ::intel_x64::ept::pt::entry::execute_access::enable(tmp_pte);
          eapis()->set_eptp(g_guest_map);
        }

       // bfdebug_nhex(0, "[X] GVA", info.gva);
        uint64_t aligned = ALIGN(info.gpa, 4096);
       // bfdebug_nhex(0, "[X] Aligned GVA", aligned);
        tmp_pte = g_guest_map.entry(aligned);
        ::intel_x64::ept::pt::entry::execute_access::enable(tmp_pte);
        eapis()->set_eptp(g_guest_map);
        ::intel_x64::vmx::invept_global();

        return true;
    }

    bool wrcr3_handler( gsl::not_null<vmcs_t *> vmcs,
        control_register_handler::info_t &info)
    {

      bfignored(vmcs);
      bfignored(info);

      if (info.val == current_enclave_cr3) {
        return false;
      }

      if (enclave_running == true &&
            ::intel_x64::vmcs::guest_cr3::get() == current_enclave_cr3) {
        enclave_running = false;
      }

      return false;
    }

    bool
    preemption_timer_handler(gsl::not_null<vmcs_t *> vmcs)
    {
      bfignored(vmcs);
      return false;
    }
    void add_sgx_paddr(uint64_t paddr) {
      sgx_paddrs[count_sgx_paddrs] = paddr;
      count_sgx_paddrs++;
    }

    bool add_epc_address(void)
    {
          // Setting up for EPT.
          uint64_t cur_paddr = epc_base_address;
          uint64_t pages = (epc_size/4096);
          if (epc_size%4096 != 0) pages++;
          for (int i = 0; i < pages; i++) {
            if (g_guest_map.is_2m(
                bfn::upper(cur_paddr, ::intel_x64::ept::pd::from))) {
                ept::identity_map_convert_2m_to_4k(
                g_guest_map,
                bfn::upper(cur_paddr, ::intel_x64::ept::pd::from)
                );
            }
            add_sgx_paddr(cur_paddr);
            cur_paddr += 4096;
          }

          return true;

    }

    bool add_epc_address_2m(void)
    {
          // Setting up for EPT.
          uint64_t cur_paddr = epc_base_address;
          uint64_t pages = (epc_size/(4096*512));
          if (epc_size%(4096*512) != 0) pages++;
          for (int i = 0; i < pages; i++) {
              // bfdebug_info(0, "before the is_4k!");
            if (g_guest_map.is_4k(
              //  bfdebug_info(0, "within the is_4k!");      
                bfn::upper(cur_paddr, ::intel_x64::ept::pd::from))) {
                ept::identity_map_convert_4k_to_2m(
                g_guest_map,
                bfn::upper(cur_paddr, ::intel_x64::ept::pd::from)
                );
            }
            add_sgx_paddr(cur_paddr);
            cur_paddr += 4096*512;
          }

          return true;

    }
    void trap_enclave_entry_new_2m(void)
    {
        for (int k = 0; k < count_sgx_paddrs; k++) {
          if (g_guest_map.is_4k(
              bfn::upper(sgx_paddrs[k], ::intel_x64::ept::pd::from))) {
              ept::identity_map_convert_4k_to_2m(
              g_guest_map,
              bfn::upper(sgx_paddrs[k], ::intel_x64::ept::pd::from)
            );
          }
          tmp_pte = g_guest_map.entry(sgx_paddrs[k]);
          sgx_guest_pte_shadow[k] = g_guest_map.entry(sgx_paddrs[k]);
          ::intel_x64::ept::pt::entry::execute_access::disable(tmp_pte);
        }
        eapis()->set_eptp(g_guest_map);
        ::intel_x64::vmx::invept_global();
    }

    void trap_enclave_entry_new(void)
    {
        for (int k = 0; k < count_sgx_paddrs; k++) {
          if (g_guest_map.is_2m(
              bfn::upper(sgx_paddrs[k], ::intel_x64::ept::pd::from))) {
              ept::identity_map_convert_2m_to_4k(
              g_guest_map,
              bfn::upper(sgx_paddrs[k], ::intel_x64::ept::pd::from)
            );
          }
          tmp_pte = g_guest_map.entry(sgx_paddrs[k]);
          sgx_guest_pte_shadow[k] = g_guest_map.entry(sgx_paddrs[k]);
          ::intel_x64::ept::pt::entry::execute_access::disable(tmp_pte);
        }
        eapis()->set_eptp(g_guest_map);
        ::intel_x64::vmx::invept_global();
    }


    void trap_enclave_entry(void)
    {
        for (int k = 0; k < modified_pages; k++) {
          if (g_guest_map.is_2m(
              bfn::upper(sgx_paddrs[k], ::intel_x64::ept::pd::from))) {
              ept::identity_map_convert_2m_to_4k(
              g_guest_map,
              bfn::upper(sgx_paddrs[k], ::intel_x64::ept::pd::from)
            );
          }
          tmp_pte = g_guest_map.entry(sgx_paddrs[k]);
          sgx_guest_pte_shadow[k] = g_guest_map.entry(sgx_paddrs[k]);
          ::intel_x64::ept::pt::entry::execute_access::disable(tmp_pte);
        }
        eapis()->set_eptp(g_guest_map);
        ::intel_x64::vmx::invept_global();
    }

    bool find_enclave_channel(uint64_t guest_cr3)
    {
        uint64_t channel_vaddr = 0x100000;
        channel_paddr = custom_virt_to_phys_with_cr3(channel_vaddr, guest_cr3);
        if (channel_paddr == 0x0) {
          bfdebug_info(0, "Couldn't find channel!");
          return false;
        }
        channel_map =
          bfvmm::x64::make_unique_map<uintptr_t>(channel_paddr);
        char* t = (char*) channel_map.get();
        return true;
    }

    bool read_enclave_channel(uint64_t guest_cr3)
    {
      char* t = (char*) channel_map.get();
      bfdebug_ndec(0, "channel content: ", *t);
      if (*t == 'e') {
        // Enclave Exit
        remove_permission_gpu_buffers();
        *t = 'o';
      } else if (*t == 'm') {
        // This case signifies a memory region which has to be protected
        // by the hypervisor.
        // bfdebug_info(0, "within t == 'm'+++");
        unsigned long* buf = (unsigned long*) (channel_map.get() + 1);
        unsigned long* size = (unsigned long*) (channel_map.get() + sizeof(char) + sizeof(unsigned long));


/*      debug_dma(buf,size,guest_cr3);
*/

        if (check_buffer_exists(*buf) == false) {
          uint64_t paddr = custom_virt_to_phys_with_cr3(*buf, guest_cr3);
           // add_gpu_buffer(*buf, paddr, *size);
            add_gpu_buffer_2m(*buf, paddr, *size);
        }

        *t = 'o';
      } else if (*t == 'n') {
       // Enclave Resume
       // *t = 'o';
       // if ((info.gpa == gpu_buffers[count_gpu_buffers].paddr) && (info.gva == gpu_buffers[count_gpu_buffers].vaddr))
        write_ipi_init_all_not_self();
        grant_permission_gpu_buffers();
        *t = 'o';
      }else if (*t == 'i') {
       // channel id
       // *t = 'o';
        int* chanid= (int*) (channel_map.get() + sizeof(int));
        bfdebug_ndec(0, "channel id: ", *chanid);
        *t = 'o';
      }else if (*t == 'd') {
       // Enclave id
       // *t = 'o';
        bfdebug_info(0, "within enclave id!");
        uint64_t* encid= (uint64_t*) (channel_map.get() + sizeof(uint64_t));
        bfdebug_nhex(0, "enclave id: ", *encid);
        *t = 'o';
      }

      
      return true;
    }
    
    void encrypt(unsigned long* src, unsigned long* dst, unsigned long size)
    {
        for(int i=0; i< size; i++)
        {
            int j= i%10;
            ((char*)dst)[i]=((char*)src)[i]^key[j];
        }
    }
    void decrypt(unsigned long* src, unsigned long* dst, unsigned long size)
    {
         encrypt(src, dst, size);
    }

    void debug_dma(unsigned long* buf, unsigned long* size, uint64_t guest_cr3)
    {
        // This is all debugging to make sure that DMA has phyisically
        // contiguous pages.

        unsigned long num_pages = (*size/4096);
        if (*size%4096 != 0) num_pages++;
        unsigned long unalloc_pages = 0;
        uint64_t old_paddr = 0;
        for (int i = 0; i < num_pages; i++) {
          uint64_t paddr = custom_virt_to_phys_with_cr3(*buf+(i*4096),
            guest_cr3);
          add_gpu_paddr(paddr);
          if (paddr != 0x0 &&
              (paddr != old_paddr + 4096) && i > 0) {
            bfdebug_nhex(0, "NOT CONTIGUOUS: ", paddr);
          }
          if (paddr == 0x0) {
            unalloc_pages++;
          }
          old_paddr = paddr;
        }

        // Should be Zero!
        bfdebug_ndec(0, "Unallocated pages: ", unalloc_pages);
    }

    bool check_buffer_exists(uint64_t vaddr)
    {
      for (int i = 0; i < count_gpu_buffers; i++)
      {
        if (gpu_buffers[i].vaddr == vaddr) {
          return true;
        }
      }
      return false;
    }

    void add_gpu_paddr(uint64_t paddr) {
      gpu_paddrs[count_gpu_paddrs] = paddr;
      count_gpu_paddrs++;
    }

    bool add_gpu_buffer(uint64_t vaddr, uint64_t paddr, uint64_t size)
    {
        if (paddr != 0x0) {
          gpu_buffers[count_gpu_buffers].vaddr = vaddr;
          gpu_buffers[count_gpu_buffers].paddr = paddr;
          gpu_buffers[count_gpu_buffers].size = size;

          // Setting up for EPT.
          uint64_t cur_paddr = paddr;
          uint64_t pages = (size/4096);
          if (size%4096 != 0) pages++;
          for (int i = 0; i < pages; i++) {
            if (g_guest_map.is_2m(
                bfn::upper(cur_paddr, ::intel_x64::ept::pd::from))) {
                ept::identity_map_convert_2m_to_4k(
                g_guest_map,
                bfn::upper(cur_paddr, ::intel_x64::ept::pd::from)
                );
            }
            add_gpu_paddr(cur_paddr);
            cur_paddr += 4096;
          }

          gpu_buffers[count_gpu_buffers].num_pages = pages;

          bfdebug_info(0, "DMA BUFFER");
          bfdebug_nhex(0, "virtual address: ", vaddr);
          bfdebug_nhex(0, "physical address: ", paddr);
          bfdebug_ndec(0, "num pages: ", pages);
          bfdebug_info(0, "\n");

          count_gpu_buffers++;
          return true;
        }
        return false;
    }
    bool add_gpu_buffer_2m(uint64_t vaddr, uint64_t paddr, uint64_t size)
    {
        if (paddr != 0x0) {
          gpu_buffers[count_gpu_buffers].vaddr = vaddr;
          gpu_buffers[count_gpu_buffers].paddr = paddr;
          gpu_buffers[count_gpu_buffers].size = size;

          // Setting up for EPT.
          uint64_t cur_paddr = paddr;
          uint64_t pages = (size/4096*512);
          if (size%(4096*512) != 0) pages++;
          for (int i = 0; i < pages; i++) {
            if (g_guest_map.is_4k(
                bfn::upper(cur_paddr, ::intel_x64::ept::pd::from))) {
                ept::identity_map_convert_4k_to_2m(
                g_guest_map,
                bfn::upper(cur_paddr, ::intel_x64::ept::pd::from)
                );
            }
            add_gpu_paddr(cur_paddr);
            cur_paddr += 4096*512;
          }

          gpu_buffers[count_gpu_buffers].num_pages = pages;

          bfdebug_info(0, "DMA BUFFER");
          bfdebug_nhex(0, "virtual address: ", vaddr);
          bfdebug_nhex(0, "physical address: ", paddr);
          bfdebug_ndec(0, "num pages: ", pages);
          bfdebug_info(0, "\n");

          count_gpu_buffers++;
          return true;
        }
        return false;
    }

    bool remove_permission_gpu_buffers(void)
    {
        for (int k = 0; k < count_gpu_paddrs; k++) {
          tmp_pte = g_guest_map.entry(gpu_paddrs[k]);
          ::intel_x64::ept::pt::entry::read_access::disable(tmp_pte);
          ::intel_x64::ept::pt::entry::write_access::disable(tmp_pte);
        }
        eapis()->set_eptp(g_guest_map);
        ::intel_x64::vmx::invept_global();
         bfdebug_ndec(0, "Removed EPT permissions from pages ", count_gpu_paddrs);
        return true;
    }

    bool grant_permission_gpu_buffers(void)
    {
        for (int k = 0; k < count_gpu_paddrs; k++) {
          tmp_pte = g_guest_map.entry(gpu_paddrs[k]);
          ::intel_x64::ept::pt::entry::read_access::enable(tmp_pte);
          ::intel_x64::ept::pt::entry::write_access::enable(tmp_pte);
        }
        eapis()->set_eptp(g_guest_map);
        ::intel_x64::vmx::invept_global();
        bfdebug_ndec(0, "Granted EPT permissions from pages ", count_gpu_paddrs);
        return true;
    }

   bool grant_permission_channel(void)
    {
          tmp_pte = g_guest_map.entry(channel_paddr);
          ::intel_x64::ept::pt::entry::read_access::enable(tmp_pte);
          ::intel_x64::ept::pt::entry::write_access::enable(tmp_pte);
        eapis()->set_eptp(g_guest_map);
        ::intel_x64::vmx::invept_global();
        // bfdebug_ndec(0, "Granted EPT permissions from pages ", count_channel_paddrs);
        return true;
    }

   bool remove_permission_channel(void)
    {
          tmp_pte = g_guest_map.entry(channel_paddr);
     bfdebug_nhex(0, "channel_paddress: ", channel_paddr);
          ::intel_x64::ept::pt::entry::read_access::disable(tmp_pte);
          ::intel_x64::ept::pt::entry::write_access::disable(tmp_pte);
        
        eapis()->set_eptp(g_guest_map);
        ::intel_x64::vmx::invept_global();
        // bfdebug_ndec(0, "Removed EPT permissions from pages ", count_channel_paddrs);
        return true;
    }



    bool remove_permission_bars(void)
    {
       // unsigned long addr = 0xc03bc000;
        unsigned long addr = 0xa0000000;
        unsigned long size = 10;

        //unsigned long addr = act_bar2;
        //unsigned long size = act_bar2_size/4096;
        for (int k = 0; k < size; k++) {
          tmp_pte = g_guest_map.entry(addr);
          ::intel_x64::ept::pt::entry::read_access::disable(tmp_pte);
          ::intel_x64::ept::pt::entry::write_access::disable(tmp_pte);
          addr += 4096;
        }
        eapis()->set_eptp(g_guest_map);
        ::intel_x64::vmx::invept_global();
        //bfdebug_nhex(0, "Removed EPT permissions for pages starting from ", act_bar2);
        return true;
    }

    bool grant_permission_bars(void)
    {
       // unsigned long addr = 0xc03bc000;
        unsigned long addr = 0xa0000000;
        unsigned long size = 10;
        //unsigned long addr = act_bar2;
        //unsigned long size = act_bar2_size/4096;
        for (int k = 0; k < size; k++) {
          tmp_pte = g_guest_map.entry(addr);
          ::intel_x64::ept::pt::entry::read_access::enable(tmp_pte);
          ::intel_x64::ept::pt::entry::write_access::enable(tmp_pte);
          addr += 4096;
        }
        eapis()->set_eptp(g_guest_map);
        ::intel_x64::vmx::invept_global();
        //bfdebug_nhex(0, "Granted EPT permissions for pages starting from ", act_bar2);
        return true;
    }
};

}

namespace bfvmm
{

std::unique_ptr<vcpu>
vcpu_factory::make(vcpuid::type vcpuid, bfobject *obj)
{
    bfignored(obj);
    return std::make_unique<test::vcpu>(vcpuid);
}

}
