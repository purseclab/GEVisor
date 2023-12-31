//
// Bareflank Extended APIs
// Copyright (C) 2018 Assured Information Security, Inc.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

#include <bfdebug.h>
#include <hve/arch/intel_x64/vcpu.h>
#include <hve/arch/intel_x64/encls.h>

namespace eapis
{
namespace intel_x64
{

encls_handler::encls_handler(
    gsl::not_null<apis *> apis,
    gsl::not_null<eapis_vcpu_global_state_t *> eapis_vcpu_global_state)
{
    bfignored(apis);
    bfignored(eapis_vcpu_global_state);
}

void encls_handler::enable()
{ vmcs_n::secondary_processor_based_vm_execution_controls::enable_encls_exiting::enable(); }

void encls_handler::disable()
{ vmcs_n::secondary_processor_based_vm_execution_controls::enable_encls_exiting::disable(); }

}
}
