#!/bin/bash -e

sudo apt install nasm libelf-dev

./remove.sh || true

if [ "$1" == "clean" ];
then
rm -rf eapis_* bf* ack/ hook/ depends/ prefixes/ setup_git_dir.sh CMake* cmake_install.cmake
fi

mkdir -p prefixes/x86_64-vmm-elf/lib || true
pushd prefixes/x86_64-vmm-elf/lib
ln -s libcapstone.so libcapstone_shared.so || true
popd

cmake ../GEVisor/bareflank -DDEFAULT_VMM=example_vmm -DEXTENSION="../GEVisor/extended_apis;../GEVisor/gevisor"
make -j12

make driver_quick
make quick
