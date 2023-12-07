#ifndef __ENCLS_UTILS_H_
#define __ENCLS_UTILS_H_

typedef struct {
    uint32_t oeax;
    uint64_t orbx;
    uint64_t orcx;
    uint64_t ordx;
} out_regs_t;

typedef struct {
   uint64_t linaddr;
   uint64_t srcpge;
   uint64_t secinfo;
   uint64_t secs;
} pageinfo_t;

typedef enum {
   PT_SECS = 0x00,
   PT_TCS  = 0x01,
   PT_REG  = 0x02,
   PT_VA   = 0x03,
   PT_TRIM = 0x04
} page_type_t;

typedef enum {
    ENCLS_ECREATE      = 0x00,
    ENCLS_EADD         = 0x01,
    ENCLS_EINIT        = 0x02,
    ENCLS_EREMOVE      = 0x03,
    ENCLS_EDBGRD       = 0x04,
    ENCLS_EDBGWR       = 0x05,
    ENCLS_EEXTEND      = 0x06,
    ENCLS_ELDB         = 0x07,
    ENCLS_ELDU         = 0x08,
    ENCLS_EBLOCK       = 0x09,
    ENCLS_EPA          = 0x0A,
    ENCLS_EWB          = 0x0B,
    ENCLS_ETRACK       = 0x0C,
    ENCLS_EAUG         = 0x0D,
    ENCLS_EMODPR       = 0x0E,
    ENCLS_EMODT        = 0x0F,

    // custom hypercalls
    ENCLS_OSGX_INIT      = 0x10,          // XXX?
    ENCLS_OSGX_PUBKEY    = 0x11,          // XXX?
    ENCLS_OSGX_EPCM_CLR  = 0x12,          // XXX?
    ENCLS_OSGX_CPUSVN    = 0x13,          // XXX?
    ENCLS_OSGX_STAT      = 0x14,
    ENCLS_OSGX_SET_STACK = 0x15,
} encls_cmd_t;



// encls() : Execute an encls instruction
// out_regs store the output value returned from qemu
static
void encls(encls_cmd_t leaf, uint64_t rbx, uint64_t rcx,
           uint64_t rdx, out_regs_t* out)
{

   out_regs_t tmp;
   asm volatile(".byte 0x0F\n\t"
                ".byte 0x01\n\t"
                ".byte 0xcf\n\t"
                :"=a"(tmp.oeax),
                 "=b"(tmp.orbx),
                 "=c"(tmp.orcx),
                 "=d"(tmp.ordx)
                :"a"((uint32_t)leaf),
                 "b"(rbx),
                 "c"(rcx),
                 "d"(rdx)
                :"memory");

    if (out != NULL) {
        *out = tmp;
    }
}

static
int EINIT(uint64_t sigstruct, void* secs, uint64_t einittoken)
{
    // RBX: SIGSTRUCT(In, EA)
    // RCX: SECS(In, EA)
    // RDX: EINITTOKEN(In, EA)
    // RAX: ERRORCODE(Out)
    out_regs_t out;
    encls(ENCLS_EINIT, sigstruct, (uint64_t) secs, einittoken, &out);
    return -(int)(out.oeax);
}


void ENCLU(void)
{
  asm("ENCLU");
}

#endif