%module(package="Pluto") Libraries

%{
#include "Pluto/Libraries/GLFW/GLFW.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Libraries/OpenVR/OpenVR.hxx"
#include "Pluto/Libraries/SpaceMouse/SpaceMouse.hxx"
using namespace Libraries;
%}

%include "./../Tools/Typemaps.i"

%feature("autodoc","2");

%nodefaultctor GLFW;
%nodefaultdtor GLFW;

%nodefaultctor Vulkan;
%nodefaultdtor Vulkan;

%nodefaultctor OpenVR;
%nodefaultdtor OpenVR;

%nodefaultctor SpaceMouse;
%nodefaultdtor SpaceMouse;

// Issue where swig tries to construct a std future from non-existant copy constructor...
%ignore Libraries::Vulkan::enqueue_graphics_commands(vk::SubmitInfo submit_info, vk::Fence fence);
%ignore Libraries::Vulkan::enqueue_present_commands(vk::PresentInfoKHR presentInfo);

%include "./../Tools/Singleton.hxx";
%include "./GLFW/GLFW.hxx";
%include "./Vulkan/Vulkan.hxx";
%include "./OpenVR/OpenVR.hxx";
%include "./SpaceMouse/SpaceMouse.hxx";
