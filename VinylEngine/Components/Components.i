%module Components

%{
#include "VinylEngine/Components/Component/Component.hxx"
#include "VinylEngine/Components/Transform/Transform.hxx"
#include "VinylEngine/Components/ComponentManager/ComponentManager.hxx"
using namespace Components;
%}

%include "./../Typemaps/Typemaps.i"

%feature("autodoc", "2");

%include <windows.i>

# %nodefaultctor Component;
# %nodefaultdtor Component;

%nodefaultctor ComponentManager;
%nodefaultdtor ComponentManager;

%template(TransformMap) std::map< std::string,std::shared_ptr< Components::Transform >,std::less< std::string >,std::allocator< std::pair< std::string const,std::shared_ptr< Components::Transform > > > >;

%include "VinylEngine/BaseClasses/Singleton.hxx"
%include "VinylEngine/Components/Component/Component.hxx"
/*%include "VinylEngine/Components/Transform/Transform.hxx"*/
%include "VinylEngine/Components/ComponentManager/ComponentManager.hxx"
