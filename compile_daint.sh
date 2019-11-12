#!/bin/bash
export CRAYPE_LINK_TYPE="dynamic"
module switch PrgEnv-cray PrgEnv-gnu
module unload perftools-base
module load papi
module load ugni
module load dmapp

rm -rf build
mkdir build
cd build

cmake ../ -DCRAY=ON -DCMAKE_SYSTEM_NAME=CrayLinuxEnvironment
make
cd ..
