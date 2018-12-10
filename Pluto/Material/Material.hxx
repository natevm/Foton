#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#include <glm/glm.hpp>
#include <glm/common.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <map>

#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Tools/StaticFactory.hxx"

#include "./MaterialStruct.hxx"
#include "Pluto/Material/PipelineParameters.hxx"

class Entity;

class Material : public StaticFactory
{
    private:
        /* Factory fields */
        static Material materials[MAX_MATERIALS];
        static MaterialStruct* pinnedMemory;
        static std::map<std::string, uint32_t> lookupTable;
        static vk::Buffer ssbo;
        static vk::DeviceMemory ssboMemory;

        struct MaterialResources {
            vk::DescriptorSetLayout descriptorSetLayout;
            vk::DescriptorPool descriptorPool;
            vk::DescriptorSet descriptorSet;
            std::vector<vk::VertexInputBindingDescription> vertexInputBindingDescriptions;
            std::vector<vk::VertexInputAttributeDescription> vertexInputAttributeDescriptions;
            PipelineParameters pipelineParameters;
            vk::Pipeline pipeline;
            vk::PipelineLayout pipelineLayout;
        };
        
        /* Uniform Color Pipeline Fields */
        static MaterialResources uniformColor;
        static MaterialResources blinn;

        static vk::DescriptorSetLayout uniformColorLayout;
        static vk::DescriptorPool uniformColorDescriptorPool;
        static vk::DescriptorSet uniformColorDescriptorSet;
        static std::vector<vk::VertexInputBindingDescription> uniformColorVertexInputBindingDescriptions;
        static std::vector<vk::VertexInputAttributeDescription> uniformColorAttributeDescriptions;
        static PipelineParameters uniformColorPipelineParameters;
        static vk::Pipeline uniformColorPipeline;
        static vk::PipelineLayout uniformColorPipelineLayout;

        static vk::ShaderModule CreateShaderModule(const std::vector<char>& code);
        static void CreatePipeline(
            std::vector<vk::PipelineShaderStageCreateInfo> shaderStages, // yes
            std::vector<vk::VertexInputBindingDescription> bindingDescriptions, // yes
            std::vector<vk::VertexInputAttributeDescription> attributeDescriptions, // yes
            std::vector<vk::DescriptorSetLayout> descriptorSetLayouts, // yes
            PipelineParameters parameters,
            vk::RenderPass renderpass,
            uint32 subpass,
            vk::Pipeline &pipeline,
            vk::PipelineLayout &layout 
        );

        static void CreateDescriptorSetLayouts();
        static void CreateDescriptorPools();
        static void CreateDescriptorSets();
        static void CreateVertexInputBindingDescriptions();
        static void CreateVertexAttributeDescriptions();
        static void CreateSSBO();
        
        /* Instance fields*/
        MaterialStruct material_struct;

    public:
        /* Factory functions */
        static Material* Create(std::string name);
        static Material* Get(std::string name);
        static Material* Get(uint32_t id);
        static Material* GetFront();
	    static uint32_t GetCount();
        static bool Delete(std::string name);
        static bool Delete(uint32_t id);

        static void SetupGraphicsPipelines(uint32_t id, vk::RenderPass renderpass);
    
        static void Initialize();
        static void UploadSSBO();
        static vk::Buffer GetSSBO();
        static uint32_t GetSSBOSize();
        static void CleanUp();

        static bool DrawEntity(vk::CommandBuffer &command_buffer, Entity &entity);

        /* Instance functions */
        Material() {
            this->initialized = false;
        }

        Material(std::string name, uint32_t id)
        {
            this->initialized = true;
            this->name = name;
            this->id = id;

            /* Working off blender's principled BSDF */
            material_struct.base_color = vec4(.8, .8, .8, 1.0);
            material_struct.subsurface = 0.0;
            material_struct.subsurface_radius = vec4(1.0, .2, .1, 1.0);
            material_struct.subsurface_color = vec4(.8, .8, .8, 1.0);
            material_struct.metallic = 0.0;
            material_struct.specular = .5;
            material_struct.specular_tint = 0.0;
            material_struct.roughness = .5;
            material_struct.anisotropic = 0.0;
            material_struct.anisotropic_rotation = 0.0;
            material_struct.sheen = 0.0;
            material_struct.sheen_tint = 0.5;
            material_struct.clearcoat = 0.0;
            material_struct.clearcoat_roughness = .03f;
            material_struct.ior = 1.45f;
            material_struct.transmission = 0.0;
            material_struct.transmission_roughness = 0.0;
        }

 

    
        std::string to_string() {
            std::string output;
            output += "{\n";
            output += "\ttype: \"Material\",\n";
            output += "\tname: \"" + name + "\"\n";
            output += "\tbase_color: \"" + glm::to_string(material_struct.base_color) + "\"\n";
            output += "\tsubsurface: \"" + std::to_string(material_struct.subsurface) + "\"\n";
            output += "\tsubsurface_radius: \"" + glm::to_string(material_struct.subsurface_radius) + "\"\n";
            output += "\tsubsurface_color: \"" + glm::to_string(material_struct.subsurface_color) + "\"\n";
            output += "\tmetallic: \"" + std::to_string(material_struct.metallic) + "\"\n";
            output += "\tspecular: \"" + std::to_string(material_struct.specular) + "\"\n";
            output += "\tspecular_tint: \"" + std::to_string(material_struct.specular_tint) + "\"\n";
            output += "\troughness: \"" + std::to_string(material_struct.roughness) + "\"\n";
            output += "\tanisotropic: \"" + std::to_string(material_struct.anisotropic) + "\"\n";
            output += "\tanisotropic_rotation: \"" + std::to_string(material_struct.anisotropic_rotation) + "\"\n";
            output += "\tsheen: \"" + std::to_string(material_struct.sheen) + "\"\n";
            output += "\tsheen_tint: \"" + std::to_string(material_struct.sheen_tint) + "\"\n";
            output += "\tclearcoat: \"" + std::to_string(material_struct.clearcoat) + "\"\n";
            output += "\tclearcoat_roughness: \"" + std::to_string(material_struct.clearcoat_roughness) + "\"\n";
            output += "\tior: \"" + std::to_string(material_struct.ior) + "\"\n";
            output += "\ttransmission: \"" + std::to_string(material_struct.transmission) + "\"\n";
            output += "\ttransmission_roughness: \"" + std::to_string(material_struct.transmission_roughness) + "\"\n";
            output += "}";
            return output;
        }

        void set_base_color(glm::vec4 color) {
            this->material_struct.base_color = color;
        }

        void set_base_color(float r, float g, float b, float a) {
            this->material_struct.base_color = glm::vec4(r, g, b, a);
        }
};
