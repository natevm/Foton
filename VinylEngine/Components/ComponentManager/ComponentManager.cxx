#include "ComponentManager.hxx"
#include "VinylEngine/Components/Transform/Transform.hxx"

namespace Components 
{
    ComponentManager* ComponentManager::Get() {
        static ComponentManager instance;
        if (!instance.is_initialized()) instance.initialize();
        return &instance;
    }

    ComponentManager::ComponentManager() {}
    ComponentManager::~ComponentManager() {}

    bool ComponentManager::initialize() {
        return false;
    }
}