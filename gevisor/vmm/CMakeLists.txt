#
# Bareflank Hypervisor
# Copyright (C) 2015 Assured Information Security, Inc.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

cmake_minimum_required(VERSION 3.6)
project(example_vmm C CXX)

include(${SOURCE_CMAKE_DIR}/project.cmake)
init_project()

eapis_add_vmm_executable(example_vmm
    SOURCES vcpu_factory_hook.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
#set(CMAKE_CXX_FLAGS_DEBUG "-O0")
#set(CMAKE_CXX_FLAGS_RELEASE "-O0")

## These are for all protections
add_definitions(-DPAGE -DCACHE_ATTACK -DLLC -DNON_LLC -DBRANCH)
