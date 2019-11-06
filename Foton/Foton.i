%module Foton

%{
/* Common */
#include "Foton/Foton.hxx"
#include "Foton/Importers/Importers.hxx"
#include "Foton/Tools/StaticFactory.hxx"
using namespace Foton;

/* Libraries */
#include "Foton/Libraries/GLFW/GLFW.hxx"
#include "Foton/Libraries/Vulkan/Vulkan.hxx"
#include "Foton/Libraries/OpenVR/OpenVR.hxx"
#ifdef BUILD_SPACEMOUSE
#include "Foton/Libraries/SpaceMouse/SpaceMouse.hxx"
#endif
using namespace Libraries;

/* Systems */
#include "Foton/Systems/RenderSystem/RenderSystem.hxx"
#include "Foton/Systems/EventSystem/EventSystem.hxx"
#include "Foton/Systems/PhysicsSystem/PhysicsSystem.hxx"
using namespace Systems;

/* Components */
#include "Foton/Constraint/Constraint.hxx"
#include "Foton/Collider/Collider.hxx"
#include "Foton/RigidBody/RigidBody.hxx"
#include "Foton/Transform/Transform.hxx"
#include "Foton/Texture/Texture.hxx"
#include "Foton/Camera/Camera.hxx"
#include "Foton/Mesh/Mesh.hxx"
#include "Foton/Light/Light.hxx"
#include "Foton/Material/Material.hxx"
#include "Foton/Entity/Entity.hxx"

#include "Foton/Prefabs/Prefabs.hxx"
#include "Foton/Prefabs/CameraPrefab.hxx"
#include "Foton/Prefabs/VRRig.hxx"
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
%include <attribute.i>

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
%ignore Libraries::Vulkan::enqueue_compute_commands(std::vector<vk::CommandBuffer> commandBuffers, std::vector<vk::Semaphore> waitSemaphores, std::vector<vk::PipelineStageFlags> waitDstStageMasks, std::vector<vk::Semaphore> signalSemaphores, vk::Fence fence, std::string hint, uint32_t queue_idx);
%ignore Libraries::Vulkan::enqueue_compute_commands(CommandQueueItem item);
%ignore Libraries::Vulkan::enqueue_present_commands(std::vector<vk::SwapchainKHR> swapchains, std::vector<uint32_t> swapchain_indices, std::vector<vk::Semaphore> waitSemaphores);
%ignore Libraries::Vulkan::CommandQueueItem;
%ignore Libraries::Vulkan::CommandQueueItem;

%ignore CommandQueueItem;

%ignore Initialized;
%ignore Texture::Data;

/* Some properties to make entity creation and modification easier */
%attribute(Entity, RigidBody*, rigid_body, get_rigid_body, set_rigid_body);
%attribute(Entity, Collider*, collider, get_collider, set_collider);
%attribute(Entity, Transform*, transform, get_transform, set_transform);
%attribute(Entity, Mesh*, mesh, get_mesh, set_mesh);
%attribute(Entity, Material*, material, get_material, set_material);
%attribute(Entity, Light*, light, get_light, set_light);
%attribute(Entity, Camera*, camera, get_camera, set_camera);

%include "Foton/Foton.hxx"
%include "Foton/Importers/Importers.hxx"
%include "Foton/Tools/Singleton.hxx"
%include "Foton/Tools/StaticFactory.hxx"

// Libraries
%import "Foton/Libraries/GLM/GLM.i"
%include "Foton/Libraries/GLFW/GLFW.hxx";
%include "Foton/Libraries/Vulkan/Vulkan.hxx";
%include "Foton/Libraries/OpenVR/OpenVR.hxx";

#ifdef BUILD_SPACEMOUSE
%include "Foton/Libraries/SpaceMouse/SpaceMouse.hxx";
#endif

%include "Foton/Tools/System.hxx"
%include "Foton/Systems/RenderSystem/RenderSystem.hxx"
%include "Foton/Systems/EventSystem/EventSystem.hxx"
%include "Foton/Systems/PhysicsSystem/PhysicsSystem.hxx"

%include "Foton/Transform/Transform.hxx"
%include "Foton/Texture/Texture.hxx"
%include "Foton/Collider/Collider.hxx"
%include "Foton/RigidBody/RigidBody.hxx"
%include "Foton/Constraint/Constraint.hxx"
%include "Foton/Mesh/Mesh.hxx"
%include "Foton/Light/Light.hxx"
%include "Foton/Material/Material.hxx"
%include "Foton/Camera/Camera.hxx"
%include "Foton/Entity/Entity.hxx"

%include "Foton/Prefabs/Prefabs.hxx"
%include "Foton/Prefabs/CameraPrefab.hxx"
%include "Foton/Prefabs/VRRig.hxx"

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

/*
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
*/