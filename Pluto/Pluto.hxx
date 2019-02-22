#pragma once

#include <set>
#include <string>
#include <vector>

extern bool Initialized;

class Entity;

void Initialize(
    bool useGLFW = true, 
    bool useOpenVR = false,
    std::set<std::string> validation_layers = {}, 
    std::set<std::string> instance_extensions = {}, 
    std::set<std::string> device_extensions = {}, 
    std::set<std::string> device_features = {
        "shaderUniformBufferArrayDynamicIndexing",
        "shaderSampledImageArrayDynamicIndexing",
        "shaderStorageBufferArrayDynamicIndexing",
        "shaderStorageImageArrayDynamicIndexing",
        "vertexPipelineStoresAndAtomics", 
        "fragmentStoresAndAtomics"}
);

void CleanUp();

std::vector<Entity*> ImportOBJ(std::string filepath, std::string mtl_base_dir);