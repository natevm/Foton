#pragma once

#include <set>
#include <string>

extern bool Initialized;

void Initialize(
    bool useGLFW = true, 
    bool useOpenVR = false,
    std::set<std::string> validation_layers = {"VK_LAYER_LUNARG_standard_validation"}, 
    std::set<std::string> instance_extensions = {}, 
    std::set<std::string> device_extensions = {}, 
    std::set<std::string> device_features = {"vertexPipelineStoresAndAtomics", "fragmentStoresAndAtomics"}
);

void CleanUp();
