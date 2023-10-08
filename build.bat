@echo off
if not exist build mkdir build
pushd build
gcc ..\source\*.c -I ..\include -o halo_assembler.exe
popd