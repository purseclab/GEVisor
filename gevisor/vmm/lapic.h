//
// Copyright (C) 2019 Assured Information Security, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef LAPIC_H
#define LAPIC_H

#include <bftypes.h>

    enum icr_delivery_mode : uint64_t {
        fixed = 0,
        lowest_priority = 1,
        smi = 2,
        nmi = 4,
        init = 5,
        sipi = 6
    };

    enum icr_destination_mode : uint64_t { physical = 0, logical = 1 };

    enum icr_level : uint64_t { deassert = 0, assert = 1 };

    enum icr_trigger_mode : uint64_t { edge = 0, level = 1 };

    enum icr_destination_shorthand : uint64_t {
        none = 0,
        self = 1,
        all_and_self = 2,
        all_not_self = 3
    };

struct access_ops {
        void (*write)(uintptr_t base, uint32_t reg, uint32_t val);
        void (*write_icr)(uintptr_t base, uint64_t val);
    };
struct access_ops m_ops;

uint64_t m_base_msr;

static void xapic_write(uintptr_t base, uint32_t reg, uint32_t val)
{
    *reinterpret_cast<volatile uint32_t *>(base | (reg << 4)) = val;
}

static void xapic_write_icr(uintptr_t base, uint64_t val)
{
    constexpr uintptr_t icr_hi = 0x310;
    constexpr uintptr_t icr_lo = 0x300;

    auto hi_addr = reinterpret_cast<volatile uint32_t *>(base | icr_hi);
    auto lo_addr = reinterpret_cast<volatile uint32_t *>(base | icr_lo);

    *hi_addr = (uint32_t)(val >> 32);
    ::intel_x64::wmb();
    *lo_addr = (uint32_t)val;
}


static void x2apic_write(uintptr_t base, uint32_t reg, uint32_t val)
{
    ::x64::msrs::set(base | reg, val);
}

static void x2apic_write_icr(uintptr_t base, uint64_t val)
{
    bfignored(base);
    ia32_x2apic_icr::set(val);
}

void init_xapic()
{   
    auto msr_hpa = ia32_apic_base::apic_base::get(m_base_msr);
    
    m_xapic_hpa = msr_hpa;
    m_xapic_hva = reinterpret_cast<uint32_t *>(g_mm->alloc_map(xapic_bytes));
    
    mmap->map_4k(m_xapic_hva,
                  m_xapic_hpa,
                  cr3::mmap::attr_type::read_write,
                  cr3::mmap::memory_type::uncacheable);
    
    m_base_addr = reinterpret_cast<uintptr_t>(m_xapic_hva);
    m_ops.write = xapic_write;
    m_ops.write_icr = xapic_write_icr;
}

void init_x2apic()
{
    m_base_addr = x2apic_base;
    m_ops.write = x2apic_write;
    m_ops.write_icr = x2apic_write_icr;
}

static void apic_init(){
    m_base_msr = ia32_apic_base::get();
    auto state = ia32_apic_base::state::get(m_base_msr);
    
    switch (state) {
    case ia32_apic_base::state::xapic:
        init_xapic();
        break;
    case ia32_apic_base::state::x2apic:
        init_x2apic();
        break;
    default:
        bferror_nhex(0, "Unsupported lapic state", state);
        throw std::runtime_error("Unsupported lapic state");
    }
}

void write_ipi_init_all_not_self()
{

   apic_init(); 
    uint64_t icr = 0U;

   
    icr |= (icr_delivery_mode::init << 8);
    icr |= (icr_level::assert << 14);
    icr |= (icr_trigger_mode::edge << 15);
    icr |= (icr_destination_shorthand::all_not_self << 18);

    m_ops.write_icr(m_base_addr, val);
}


#endif
