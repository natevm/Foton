%module Systems

%{
#include "VinylEngine/Systems/RenderSystem/RenderSystem.hxx"
#include "VinylEngine/Systems/EventSystem/EventSystem.hxx"
using namespace Systems;
%}

%include "./../Typemaps/Typemaps.i"

%feature("autodoc", "2");
%feature("kwargs");

%include <windows.i>

%nodefaultctor System;
%nodefaultdtor System;

%nodefaultctor RenderSystem;
%nodefaultdtor RenderSystem;

%nodefaultctor EventSystem;
%nodefaultdtor EventSystem;

%include "VinylEngine/BaseClasses/Singleton.hxx"
%include "VinylEngine/BaseClasses/System.hxx"
%include "VinylEngine/Systems/RenderSystem/RenderSystem.hxx"
%include "VinylEngine/Systems/EventSystem/EventSystem.hxx"
