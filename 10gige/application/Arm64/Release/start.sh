#!/bin/sh

# Add any custom startup commands you need in here
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/../../../libGigE/Arm64/Release
export LD_LIBRARY_PATH
./gvrd eth2
