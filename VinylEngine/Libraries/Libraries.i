%module Libraries

%{
#include "VinylEngine/Libraries/GLFW/GLFW.hxx"
#include "VinylEngine/Libraries/Vulkan/Vulkan.hxx"
using namespace Libraries;
%}

%include "./../Typemaps/Typemaps.i"

%feature("autodoc","2");
%feature("kwargs");

%nodefaultctor GLFW;
%nodefaultdtor GLFW;

%nodefaultctor Vulkan;
%nodefaultdtor Vulkan;

%include "./../BaseClasses/Singleton.hxx";
%include "./GLFW/GLFW.hxx";
%include "./Vulkan/Vulkan.hxx";
