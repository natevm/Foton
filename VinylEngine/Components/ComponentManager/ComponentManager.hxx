// ┌──────────────────────────────────────────────────────────────────┐
// │  ComponentManager: Contains global simulation data, like a list  | 
// |    of textures, meshes, a scene graph, a list of lights, etc     |
// └──────────────────────────────────────────────────────────────────┘
#pragma once
#include "VinylEngine/BaseClasses/Singleton.hxx"
#include "VinylEngine/Tools/HashCombiner.hxx"
#include <map>
#include <memory>

/* Forward Declarations */
// namespace Entities { class Entity; }
// namespace Components::Textures { class Texture; }
// namespace Components::Meshes { class Mesh; }
// namespace Components::Materials { class Material; }
// namespace Components { class Callbacks; }
namespace Components { class Transform; }
// namespace Components::Math { class Perspective; }
// namespace Components::Lights { class Light; }
// struct PipelineKey;
// struct PipelineParameters;

namespace Components 
{
    class ComponentManager : public Singleton {
        public:        
            static ComponentManager* Get();
            bool initialize();            
            std::map<std::string, std::shared_ptr<Components::Transform>> transforms;
        private:
            ComponentManager();
            ~ComponentManager();
    };   
}