## These are the ENCLS stuff

#define EINIT 0x2

#ifdef CONFIG_X86_64
#define XAX "%%rax"
#else
#define XAX "%%eax"
#endif

# define _ASM_EXTABLE_HANDLE(from, to, handler)     \
  .pushsection "__ex_table","a" ;       \
  .balign 4 ;           \
  .long (from) - . ;          \
  .long (to) - . ;          \
  .long (handler) - . ;         \
  .popsection

# define _ASM_EXTABLE(from, to)         \
  _ASM_EXTABLE_HANDLE(from, to, ex_handler_default)

#define __encls_ret(rax, rbx, rcx, rdx)     \
  ({            \
  int ret;          \
  asm volatile(         \
  "1: .byte 0x0f, 0x01, 0xcf;\n\t"    \
  "2:\n"            \
  ".section .fixup,\"ax\"\n"      \
  "3: mov $-14, " XAX "\n"        \
  "   jmp 2b\n"         \
  ".previous\n"         \
  _ASM_EXTABLE(1b, 3b)        \
  : "=a"(ret)         \
  : "a"(rax), "b"(rbx), "c"(rcx), "d"(rdx)  \
  : "memory");          \
  ret;            \
})

static inline int __einit(void *sigstruct, void *einittoken, void* secs)
{
  return __encls_ret(EINIT, sigstruct, secs, einittoken);
}

## These are from the hypervisor encls trap handler

        unsigned long rax = ::intel_x64::vmcs::save_state()->rax;
        unsigned long rbx = ::intel_x64::vmcs::save_state()->rbx;
        unsigned long rcx = ::intel_x64::vmcs::save_state()->rcx;
        unsigned long rdx = ::intel_x64::vmcs::save_state()->rdx;
        __einit(rax, rbx, rcx, rdx);

