Pluto [![Build Status](https://travis-ci.org/n8vm/Pluto.svg?branch=master)](https://travis-ci.org/n8vm/Pluto)
===========

``Pluto`` is a Vulkan based graphics engine. It uses a Python 3 interpreter to create and edit 3D scenes at runtime, and has support for Jupyter notebook and Jupyter lab environments. Since it uses a python interpreter instead of a scripting language like LUA, users can import their own modules, like PyTorch or Tensorflow, and take advantage of popular python linear algebra libraries like numpy.

Since Pluto uses Vulkan as it's back end, it can be built cross platform. It has support for VR headsets through OpenVR, and can optionally use Nvidia's RTX Raytracing extension to improve render quality.

At the moment, Pluto is mainly used for rapid prototyping in both C++ and in python. Forking is highly encouraged, as well as GitHub issues!

PyPI Prebuilt Installation
------------
(This does not work right now on non-windows platforms)

To install ``Pluto`` from PyPI::

    pip install Pluto
    python -m Pluto.install

Building from scratch
------------

This section is a work in progress. It will be updated once the project supports all platforms.
