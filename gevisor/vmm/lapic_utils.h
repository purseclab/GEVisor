#ifndef __LAPIC_UTILS_H__
#define __LAPIC_UTILS_H__

// ACPI: Local APIC address 0xfee00000

#define LAPIC_BASE_ADDRESS 0xfee00000
#define LAPIC_LVT_TMR      (0x0320 + LAPIC_BASE_ADDRESS)
#define LAPIC_DISABLE      0x10000
#define TMR_PERIODIC       0x20000

void disable_lapic_timer()
{
  //asm ("mov dword [LAPIC_BASE_ADDRESS+LAPIC_LVT_TMR], LAPIC_DISABLE");
  //asm ("mov dword 0xfee00320, LAPIC_DISABLE");
  //int* tmp = (int*) 0xfee00320;
  //*tmp = LAPIC_DISABLE;
}

void enable_lapic_timer()
{
  bfdebug_nhex(0, "LAPIC_LVT_TMR: ", LAPIC_LVT_TMR);
  // asm ("movb 32, (LAPIC_LVT_TMR)");
  // int* tmp = (int*) 0xfee00320;
  // *tmp = 32 | TMR_PERIODIC;
}

#endif