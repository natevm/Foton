#!/bin/bash

git submodule update --init --recursive

cd `dirname "$0"`

mkdir build
mkdir install

cd build

cmake -G Xcode ..
# can pass -DBUILD_whatever=ON to CMake

#read  -n 1 -p "Project File Creation finished. Press any key to continue."
open Pluto.xcodeproj

