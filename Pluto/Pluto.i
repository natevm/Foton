%module Pluto

%{
/* Common */
#include "Pluto/Pluto.hxx"
#include "Pluto/Tools/StaticFactory.hxx"
using namespace Pluto;

/* Libraries */
#include "Pluto/Libraries/GLFW/GLFW.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#ifdef BUILD_OPENVR
#include "Pluto/Libraries/OpenVR/OpenVR.hxx"
#endif
#ifdef BUILD_SPACEMOUSE
#include "Pluto/Libraries/SpaceMouse/SpaceMouse.hxx"
#endif
using namespace Libraries;

/* Systems */
#include "Pluto/Systems/RenderSystem/RenderSystem.hxx"
#include "Pluto/Systems/EventSystem/EventSystem.hxx"
using namespace Systems;

/* Components */
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Texture/Texture.hxx"
#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Mesh/Mesh.hxx"
#include "Pluto/Light/Light.hxx"
#include "Pluto/Material/Material.hxx"
#include "Pluto/Entity/Entity.hxx"

#include "Pluto/Prefabs/Prefabs.hxx"
#include "Pluto/Prefabs/CameraPrefab.hxx"
#include "Pluto/Prefabs/VRRig.hxx"
%}

/* Note: kwargs does not work with c++ standard types, like vector. 

    For now, we can use kwargs with only specific declarations
*/

%feature("autodoc","4");
%feature("kwargs") Prefabs::CreateArcBallCamera;
%feature("kwargs") Camera::Create;

/* Required on windows... */
%include <windows.i>

%include "exception.i"

%exception {
  try {
    $action
  } catch (const std::exception& e) {
    SWIG_exception(SWIG_RuntimeError, e.what());
  }
}

%include "Tools/Typemaps.i"

/* NOTE: using namespace doesnt work for this template. */
%shared_ptr(StaticFactory)
%shared_ptr(Entity)
%shared_ptr(Transform)
%shared_ptr(Texture)
%shared_ptr(Camera)
%shared_ptr(Mesh)
%shared_ptr(Material)
%shared_ptr(Light)

// Ignores
%nodefaultctor GLFW;
%nodefaultdtor GLFW;
%nodefaultctor Vulkan;
%nodefaultdtor Vulkan;
%nodefaultctor OpenVR;
%nodefaultdtor OpenVR;
%nodefaultctor SpaceMouse;
%nodefaultdtor SpaceMouse;

%nodefaultctor System;
%nodefaultdtor System;
%nodefaultctor RenderSystem;
%nodefaultdtor RenderSystem;
%nodefaultctor EventSystem;
%nodefaultdtor EventSystem;

%nodefaultctor Prefabs;
%nodefaultdtor Prefabs;

// Issue where swig tries to construct a std future from non-existant copy constructor...
%ignore Libraries::Vulkan::enqueue_graphics_commands(std::vector<vk::CommandBuffer> commandBuffers, std::vector<vk::Semaphore> waitSemaphores, std::vector<vk::PipelineStageFlags> waitDstStageMasks, std::vector<vk::Semaphore> signalSemaphores, vk::Fence fence, std::string hint, uint32_t queue_idx);
%ignore Libraries::Vulkan::enqueue_present_commands(std::vector<vk::SwapchainKHR> swapchains, std::vector<uint32_t> swapchain_indices, std::vector<vk::Semaphore> waitSemaphores);

%ignore CommandQueueItem;

%ignore Initialized;
%ignore Texture::Data;

%include "Pluto/Pluto.hxx"
%include "Pluto/Tools/Singleton.hxx"
%include "Pluto/Tools/StaticFactory.hxx"

// Libraries
%import "Pluto/Libraries/GLM/GLM.i"
%include "Pluto/Libraries/GLFW/GLFW.hxx";
%include "Pluto/Libraries/Vulkan/Vulkan.hxx";

#ifdef BUILD_OPENVR
%include "Pluto/Libraries/OpenVR/OpenVR.hxx";
#endif

#ifdef BUILD_SPACEMOUSE
%include "Pluto/Libraries/SpaceMouse/SpaceMouse.hxx";
#endif

%include "Pluto/Tools/System.hxx"
%include "Pluto/Systems/RenderSystem/RenderSystem.hxx"
%include "Pluto/Systems/EventSystem/EventSystem.hxx"

%include "Pluto/Transform/Transform.hxx"
%include "Pluto/Texture/Texture.hxx"
%include "Pluto/Mesh/Mesh.hxx"
%include "Pluto/Light/Light.hxx"
%include "Pluto/Material/Material.hxx"
%include "Pluto/Camera/Camera.hxx"
%include "Pluto/Entity/Entity.hxx"

%include "Pluto/Prefabs/Prefabs.hxx"
%include "Pluto/Prefabs/CameraPrefab.hxx"
%include "Pluto/Prefabs/VRRig.hxx"

/* Representations */
%extend Transform {
    %feature("python:slot", "tp_repr", functype="reprfunc") __repr__;
    std::string __repr__() { return $self->to_string(); }
}

%extend Texture {
    %feature("python:slot", "tp_repr", functype="reprfunc") __repr__;
    std::string __repr__() { return $self->to_string(); }
}

%extend Camera {
    %feature("python:slot", "tp_repr", functype="reprfunc") __repr__;
    std::string __repr__() { return $self->to_string(); }
}

%extend Mesh {
    %feature("python:slot", "tp_repr", functype="reprfunc") __repr__;
    std::string __repr__() { return $self->to_string(); }
}

%extend Light {
    %feature("python:slot", "tp_repr", functype="reprfunc") __repr__;
    std::string __repr__() { return $self->to_string(); }
}

%extend Material {
    %feature("python:slot", "tp_repr", functype="reprfunc") __repr__;
    std::string __repr__() { return $self->to_string(); }
}

/* Representations */
%extend Entity {
    %feature("python:slot", "tp_repr", functype="reprfunc") __repr__;
    std::string __repr__() { return $self->to_string(); }
}

%template(EntityVector) vector<Entity*>;
%template(TransformVector) vector<Transform*>;
%template(MeshVector) vector<Mesh*>;
%template(CameraVector) vector<Camera*>;
%template(TextureVector) vector<Texture*>;
%template(LightVector) vector<Light*>;
%template(MaterialVector) vector<Material*>;

