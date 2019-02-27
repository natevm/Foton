#!/bin/bash

cd `dirname "$0"`/..
# install brew
# /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"

# install swig, cmake, python3
# brew install swig cmake python3

# glfw patch
cd external/glfw
git apply ../glfw-patch.txt
cd -

git submodule update --init --recursive


mkdir build
mkdir install
cp ../files_for_pluto/Example.py ../files_for_pluto/HelperClasses.py ../files_for_pluto/AnimateMesh.py install/

cd build

cmake -G Xcode ..
# can pass -DBUILD_whatever=ON to CMake

#read  -n 1 -p "Project File Creation finished. Press any key to continue."
open Pluto.xcodeproj

