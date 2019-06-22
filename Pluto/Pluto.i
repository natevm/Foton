%module Pluto

%{
/* Common */
#include "Pluto/Pluto.hxx"
#include "Pluto/Importers/Importers.hxx"
#include "Pluto/Tools/StaticFactory.hxx"
using namespace Pluto;

/* Libraries */
#include "Pluto/Libraries/GLFW/GLFW.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Libraries/OpenVR/OpenVR.hxx"
#ifdef BUILD_SPACEMOUSE
#include "Pluto/Libraries/SpaceMouse/SpaceMouse.hxx"
#endif
using namespace Libraries;

/* Systems */
#include "Pluto/Systems/RenderSystem/RenderSystem.hxx"
#include "Pluto/Systems/EventSystem/EventSystem.hxx"
#include "Pluto/Systems/PhysicsSystem/PhysicsSystem.hxx"
using namespace Systems;

/* Components */
#include "Pluto/Constraint/Constraint.hxx"
#include "Pluto/Collider/Collider.hxx"
#include "Pluto/RigidBody/RigidBody.hxx"
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

%feature("kwargs") Initialize;
%feature("kwargs") Prefabs;
%feature("kwargs") Camera;
%feature("kwargs") Collider;
%feature("kwargs") Entity;
%feature("kwargs") Light;
%feature("kwargs") Material;
%feature("kwargs") Mesh;
%feature("kwargs") Texture;
%feature("kwargs") Transform;
%feature("kwargs") RigidBody;
%feature("kwargs") Constraint;


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
%shared_ptr(RigidBody)
%shared_ptr(Collider)
%shared_ptr(Constraint)
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
%nodefaultctor PhysicsSystem;
%nodefaultdtor PhysicsSystem;

%nodefaultctor Prefabs;
%nodefaultdtor Prefabs;

%nodefaultctor Prefabs;
%nodefaultctor Camera;
%nodefaultctor Collider;
%nodefaultctor Entity;
%nodefaultctor Light;
%nodefaultctor Material;
%nodefaultctor Mesh;
%nodefaultctor Texture;
%nodefaultctor Transform;
%nodefaultctor RigidBody;
%nodefaultctor Constraint;

%nodefaultdtor Prefabs;
%nodefaultdtor Camera;
%nodefaultdtor Collider;
%nodefaultdtor Entity;
%nodefaultdtor Light;
%nodefaultdtor Material;
%nodefaultdtor Mesh;
%nodefaultdtor Texture;
%nodefaultdtor Transform;
%nodefaultdtor RigidBody;
%nodefaultdtor Constraint;

// Issue where swig tries to construct a std future from non-existant copy constructor...
%ignore Libraries::Vulkan::enqueue_graphics_commands(std::vector<vk::CommandBuffer> commandBuffers, std::vector<vk::Semaphore> waitSemaphores, std::vector<vk::PipelineStageFlags> waitDstStageMasks, std::vector<vk::Semaphore> signalSemaphores, vk::Fence fence, std::string hint, uint32_t queue_idx);
%ignore Libraries::Vulkan::enqueue_graphics_commands(CommandQueueItem item);
%ignore Libraries::Vulkan::enqueue_present_commands(std::vector<vk::SwapchainKHR> swapchains, std::vector<uint32_t> swapchain_indices, std::vector<vk::Semaphore> waitSemaphores);
%ignore Libraries::Vulkan::CommandQueueItem;
%ignore Libraries::Vulkan::CommandQueueItem;

%ignore CommandQueueItem;

%ignore Initialized;
%ignore Texture::Data;

%include "Pluto/Pluto.hxx"
%include "Pluto/Importers/Importers.hxx"
%include "Pluto/Tools/Singleton.hxx"
%include "Pluto/Tools/StaticFactory.hxx"

// Libraries
%import "Pluto/Libraries/GLM/GLM.i"
%include "Pluto/Libraries/GLFW/GLFW.hxx";
%include "Pluto/Libraries/Vulkan/Vulkan.hxx";
%include "Pluto/Libraries/OpenVR/OpenVR.hxx";

#ifdef BUILD_SPACEMOUSE
%include "Pluto/Libraries/SpaceMouse/SpaceMouse.hxx";
#endif

%include "Pluto/Tools/System.hxx"
%include "Pluto/Systems/RenderSystem/RenderSystem.hxx"
%include "Pluto/Systems/EventSystem/EventSystem.hxx"
%include "Pluto/Systems/PhysicsSystem/PhysicsSystem.hxx"

%include "Pluto/Transform/Transform.hxx"
%include "Pluto/Texture/Texture.hxx"
%include "Pluto/Collider/Collider.hxx"
%include "Pluto/RigidBody/RigidBody.hxx"
%include "Pluto/Constraint/Constraint.hxx"
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

%extend RigidBody {
	%feature("python:slot", "tp_repr", functype="reprfunc") __repr__;
	std::string __repr__() { return $self->to_string(); }
}

%extend Collider {
	%feature("python:slot", "tp_repr", functype="reprfunc") __repr__;
	std::string __repr__() { return $self->to_string(); }
}

%extend Constraint {
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
%template(ColliderVector) vector<Collider*>;
%template(ConstraintVector) vector<Constraint*>;
%template(RigidBodyVector) vector<RigidBody*>;
%template(LightVector) vector<Light*>;
%template(MaterialVector) vector<Material*>;

/* Some properties to make entity creation and modification easier */
%extend Entity{
	%pythoncode %{
		__swig_getmethods__["rigid_body"] = get_rigid_body
		__swig_setmethods__["rigid_body"] = set_rigid_body
		if _newclass: rigid_body = property(get_rigid_body, set_rigid_body)

		__swig_getmethods__["collider"] = get_collider
		__swig_setmethods__["collider"] = set_collider
		if _newclass: collider = property(get_collider, set_collider)
		
		__swig_getmethods__["transform"] = get_transform
		__swig_setmethods__["transform"] = set_transform
		if _newclass: transform = property(get_transform, set_transform)

		__swig_getmethods__["mesh"] = get_mesh
		__swig_setmethods__["mesh"] = set_mesh
		if _newclass: mesh = property(get_mesh, set_mesh)

		__swig_getmethods__["material"] = get_material
		__swig_setmethods__["material"] = set_material
		if _newclass: material = property(get_material, set_material)

		__swig_getmethods__["light"] = get_light
		__swig_setmethods__["light"] = set_light
		if _newclass: light = property(get_light, set_light)

		__swig_getmethods__["camera"] = get_camera
		__swig_setmethods__["camera"] = set_camera
		if _newclass: camera = property(get_camera, set_camera)
	%}
};
