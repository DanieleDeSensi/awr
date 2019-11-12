#!/bin/bash
export CRAYPE_LINK_TYPE="dynamic"
module load papi
module load ugni
module load dmapp

rm -rf build
mkdir build
cd build

cmake ../ -DCRAY=ON
make
cd ..
