Foton [![Build Status](https://travis-ci.org/n8vm/Pluto.svg?branch=master)](https://travis-ci.org/n8vm/Pluto)
===========

``Foton`` is a Vulkan based graphics engine. It uses a Python 3 interpreter to create and edit 3D scenes at runtime, and has support for Jupyter notebook and Jupyter lab environments. Since it uses a python interpreter instead of a scripting language like LUA, users can import their own modules, like PyTorch or Tensorflow, and take advantage of popular python linear algebra libraries like numpy.

Since Foton uses Vulkan as it's back end, it can be built cross platform. It has support for VR headsets through OpenVR, and can optionally use Nvidia's RTX Raytracing extension to improve render quality.

At the moment, Foton is mainly used for rapid prototyping in both C++ and in python. Forking is highly encouraged, as well as GitHub issues!

<!-- PyPI Prebuilt Installation
------------
(This does not work right now on non-windows platforms)

To install ``Foton`` from PyPI::

    pip install Foton
    python -m Foton.install -->

Building from scratch
------------

# On all platforms, clone submodules!
`git clone --recurse-submodules https://github.com/n8vm/Pluto`

### On Linux
1. Install a version of CMake greater than or equal to 3.13 from here: [CMake Download Website](https://cmake.org/download/)
2. `sudo apt update`
4. `sudo apt install lunarg-vulkan-sdk swig3.0 xorg-dev python3.6-dev gcc-8 g++-8`
5. Optionally install Visual Studio Code here: [Visual Studio Code](https://code.visualstudio.com/docs/?dv=linux64_deb)
6. `mkdir build`
7. `mkdir install`
8. (If building from commandline)
    1. `cd build`
    2. `cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++-8 ../`
    3. `cmake --build . --target install --config Release`
9. (If building from visual studio code)
    1. `code .` 
    2. `CTL` + `Shift` + `P`
    3. Type and enter `CMake: configure`
    4. Type and enter `CMake: install`

### On Windows
1. Open command prompt in "Administrator Mode"
2. Install Chocolatey: `@"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command "iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"`
3. `choco install cmake python3 swig`
4. Install the LunarG SDK from here: [LunarG Download Website](https://vulkan.lunarg.com/sdk/home#windows)
5. Install Visual Studio Code and Visual Studio 2017 C++ Build Tools (Recommended) from here [Visual Studio Code](https://code.visualstudio.com/docs/?dv=linux64_deb) and here [Visual Studio Build Tools](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=BuildTools&rel=15), or just get Visual Studio 2017.
    1. If using Visual Studio Code, make sure to download the CMake Tools extension, the C/C++ extension from Microsoft, and optionally a GLSL syntax highlighting extension
6. mkdir build
7. mkdir install
8. (If using Visual Studio Code)
    1. `code .` 
    2. `CTL` + `Shift` + `P`
    3. Type and enter `CMake: configure`
    4. Type and enter `CMake: install`
9. (If using Visual Studio 2017)
    1. `cd build`
    2. `cmake -G "Visual Studio 15 2017 Win64` 
    3. Open the generated Visual Studio Solution File (the .sln)
    4. Right click the "INSTALL" target, and click build


### On MacOS
1. Install the LunarG SDK from here: [LunarG Download Website] (https://vulkan.lunarg.com/sdk/home#mac)
    1. Follow this guide: [LunarG MacOS Getting Started](https://vulkan.lunarg.com/doc/sdk/1.1.97.0/mac/getting_started.html )
    2. Add `vulkansdk/macOS/bin` to your `PATH`
    3. Optionally set `VK_LAYER_PATH` to `vulkansdk/macOS/etc/vulkan/explicit_layer.d` to support validation layers
    4. Set `VK_ICD_FILENAMES` to `vulkansdk/macOS/etc/vulkan/icd.d/MoltenVK_icd.json`
    5. Set `VULKAN_SDK` to `vulkansdk/macOS`
2. `brew install cmake`
3. `brew install swig`
4. `mkdir build`
5. `mkdir install`
6. (If building from commandline)
    1. `cd build`
    2. `cmake -DCMAKE_BUILD_TYPE=Release ../`
    3. `cmake --build . --target install --config Release`
7. (If building from visual studio code)
    1. `code .` 
    2. `CTL` + `Shift` + `P`
    3. Type and enter `CMake: configure`
    4. Type and enter `CMake: install`
