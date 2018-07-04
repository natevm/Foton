%module Systems

%{
#include "VinylEngine/Systems/Bullet/Bullet.hxx"
#include "VinylEngine/Systems/GLFW/GLFW.hxx"
#include "VinylEngine/Systems/Vulkan/Vulkan.hxx"
using namespace Systems;
%}

%include "./../Typemaps/Typemaps.i"
%include "./Bullet/Bullet.hxx";
%include "./GLFW/GLFW.hxx";
%include "./Vulkan/Vulkan.hxx";
