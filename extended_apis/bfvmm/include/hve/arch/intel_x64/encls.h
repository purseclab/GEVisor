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

#ifndef ENCLS_INTEL_X64_EAPIS_H
#define ENCLS_INTEL_X64_EAPIS_H

#include "base.h"

// -----------------------------------------------------------------------------
// Definitions
// -----------------------------------------------------------------------------

namespace eapis
{
namespace intel_x64
{

class apis;
class eapis_vcpu_global_state_t;

/// ENCLS
///
/// Provides an interface for enabling ENCLS_EXITS
///
class EXPORT_EAPIS_HVE encls_handler
{
public:

    /// Constructor
    ///
    /// @expects
    /// @ensures
    ///
    /// @param apis the apis object for this rdmsr handler
    /// @param eapis_vcpu_global_state a pointer to the vCPUs global state
    ///
    encls_handler(
        gsl::not_null<apis *> apis,
        gsl::not_null<eapis_vcpu_global_state_t *> eapis_vcpu_global_state);

    /// Destructor
    ///
    /// @expects
    /// @ensures
    ///
    ~encls_handler() = default;


    /// Enable
    ///
    /// @expects
    /// @ensures
    ///
    void enable();

    /// Disable
    ///
    /// @expects
    /// @ensures
    ///
    void disable();

    /// @cond

    encls_handler(encls_handler &&) = default;
    encls_handler(const encls_handler &) = delete;
    encls_handler &operator=(const encls_handler &) = delete;

    /// @endcond
};

}
}

#endif
