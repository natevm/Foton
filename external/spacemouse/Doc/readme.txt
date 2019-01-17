3DxWare SDK for Windows


INTRODUCTION

The 3DxWare SDK provides a single interface to the 3DxWare
driver software that gives application software access to
3D mouse-derived product data.  

In addition to this SDK, a current installation of 3DxWinCore 17.4 or 
newer is required.  Since the 3DxWare driver is released much more 
frequently than the SDK, a 3DxWare driver is NOT provided with this SDK.  
See http://www.3Dconnexion.com/ for the latest 3DxWare release.


IMPORTANT NOTES
Only release (non-debug) multi-threaded libraries are included.

Most of the software your application uses comes as DLLs with the 
3DxWare release.  The static libraries included in the SDK only 
load those DLLs.  While the DLLs change often, the static libs do not.


FILE LOCATIONS

The entire contents of this SDK are placed under the
directory that is provided during the Setup.

Two demonstration programs are provided with source and Microsoft
Visual Studio solutions.  They can be rebuilt with these solutions
(see notes below).

Applications must link with a static library called "siapp".  It 
provides stubs for driver routines that live in the DLL. 

In addition, a math library called "spwmath" is provided.  Some of the
demonstrations use the spwmath library.  Source code is provided 
for both of these libraries.


BUILD NOTES

Each demo and library can be rebuilt by opening the corresponding
Visual Studio solution in the main subdirectory for each demo or library.

Choose the correct target for your platform.
Release and Debug are win32.
Win64 x64 Release is for AMD64.

When building an application, the 3DxWare libraries must be linked in.
In Visual Studio this is done by selecting Project->Properties->
Linker->Input and General.  Add the libraries to Input and the paths to
General. 

The library paths are:
<3DxWare SDK>\lib\<plaftorm>\siapp.lib

and if you use spwmath lib functions:

<3DxWare SDK>\lib\<plaftorm>\spwmath.lib

You will also need to add the <3DxWare SDK>\inc path to the 
application's settings to pickup the .h files.


LICENSE

The 3DxWare SDK is licensed according to the terms of the 3Dconnexion
Software Development Kit License Agreement. The Agreement is documented
in the "LicenseAgreementSDK.rtf" file included with this SDK.

All materials included in this SDK are copyright 3Dconnexion:

Copyright (c) 2011-2016 3Dconnexion. All rights reserved.
