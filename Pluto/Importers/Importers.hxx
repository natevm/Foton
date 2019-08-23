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

class Entity;

namespace Pluto {
    std::vector<Entity*> ImportOBJ(std::string filepath, std::string mtl_base_dir, 
        glm::vec3 position = glm::vec3(0.0f), 
        glm::vec3 scale = glm::vec3(1.0f),
        glm::quat rotation = glm::angleAxis(0.0f, glm::vec3(1.0f, 0.0f, 0.0f)));

    std::vector<Entity*> ImportPBRT(std::string name, std::string filepath, std::string basePath,
        float camera_target = 1.0f,
        glm::vec3 position = glm::vec3(0.0f), 
        glm::vec3 scale = glm::vec3(1.0f),
        glm::quat rotation = glm::angleAxis(0.0f, glm::vec3(1.0f, 0.0f, 0.0f))
    );
}