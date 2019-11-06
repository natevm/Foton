#pragma once

#include <set>
#include <string>
#include <vector>
#include <functional>


#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#define GLM_FORCE_RIGHT_HANDED

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

extern bool Initialized;

namespace Foton {

    void StartSystems(bool use_python = false, std::function<void()> callback = std::function<void()>());

    void Initialize(
        bool useGLFW = true, 
        bool useOpenVR = false,
        std::set<std::string> validation_layers = std::set<std::string>(), 
        std::set<std::string> instance_extensions = std::set<std::string>(), 
        std::set<std::string> device_extensions = std::set<std::string>(), 
        std::set<std::string> device_features = std::set<std::string>()
    );

    void WaitForStartupCallback();

    void StopSystems();

    void CleanUp();
}
