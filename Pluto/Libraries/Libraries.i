%module(package="Pluto") Libraries

%{
#include "Pluto/Libraries/GLFW/GLFW.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Libraries/OpenVR/OpenVR.hxx"
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

// Issue where swig tries to construct a std future from non-existant copy constructor...
%ignore Libraries::Vulkan::enqueue_graphics_commands(std::vector<vk::CommandBuffer> commandBuffers, std::vector<vk::Semaphore> waitSemaphores, std::vector<vk::PipelineStageFlags> waitDstStageMasks, std::vector<vk::Semaphore> signalSemaphores, vk::Fence fence);
%ignore Libraries::Vulkan::enqueue_present_commands(std::vector<vk::SwapchainKHR> swapchains, std::vector<uint32_t> swapchain_indices, std::vector<vk::Semaphore> waitSemaphores);

%ignore CommandQueueItem;

%include "./../Tools/Singleton.hxx";
%include "./GLFW/GLFW.hxx";
%include "./Vulkan/Vulkan.hxx";
%include "./OpenVR/OpenVR.hxx";
