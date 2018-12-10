%module Pluto

%{
#include "Pluto/Pluto.hxx"
#include "Pluto/Tools/StaticFactory.hxx"
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Texture/Texture.hxx"
#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Mesh/Mesh.hxx"
#include "Pluto/Light/Light.hxx"
#include "Pluto/Material/Material.hxx"
#include "Pluto/Entity/Entity.hxx"
%}

%feature("autodoc","2");

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

# %ignore threadFunction;
# %include "Pluto.hxx"

%import "Pluto/Libraries/GLM/GLM.i"
%include "Pluto/Pluto.hxx"
%include "Pluto/Tools/Singleton.hxx"
%include "Pluto/Tools/StaticFactory.hxx"
%include "Pluto/Transform/Transform.hxx"
%include "Pluto/Texture/Texture.hxx"
%include "Pluto/Mesh/Mesh.hxx"
%include "Pluto/Light/Light.hxx"
%include "Pluto/Material/Material.hxx"
%include "Pluto/Camera/Camera.hxx"
%include "Pluto/Entity/Entity.hxx"

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
