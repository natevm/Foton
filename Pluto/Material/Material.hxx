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
#include "./PushConstants.hxx"
#include "Pluto/Material/PipelineParameters.hxx"

class Entity;

class Texture;

class Material : public StaticFactory
{
    public:
        /* Creates a new material component */
        static Material* Create(std::string name);

        /* Retrieves a material component by name*/
        static Material* Get(std::string name);

        /* Retrieves a material component by id */
        static Material* Get(uint32_t id);

        /* Returns a pointer to the list of material components */
        static Material* GetFront();

        /* Returns the total number of reserved materials */
	    static uint32_t GetCount();

        /* Deallocates a material with the given name */
        static bool Delete(std::string name);

        /* Deallocates a material with the given id */
        static bool Delete(uint32_t id);

        /* Initializes the vulkan resources required to render during the specified renderpass */
        static void SetupGraphicsPipelines(vk::RenderPass renderpass, uint32_t sampleCount);
    
        /* Initializes all vulkan descriptor resources, as well as the Mesh factory. */
        static void Initialize();

        /* Transfers all material components to an SSBO */
        static void UploadSSBO();

        /* Copies SSBO / texture handles into the texture and component descriptor sets. 
            Also, allocates the descriptor sets if not yet allocated. */
        static void UpdateDescriptorSets();

        /* Returns the SSBO vulkan buffer handle */
        static vk::Buffer GetSSBO();

        /* Returns the size in bytes of the current material SSBO */
        static uint32_t GetSSBOSize();

        /* Releases vulkan resources */
        static void CleanUp();

        /* Records a bind of all descriptor sets to each possible pipeline to the given command buffer. Call this at the beginning of a renderpass. */
        static bool BindDescriptorSets(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass);

        /* Records a draw of the supplied entity to the current command buffer. Call this during a renderpass. */
        static bool DrawEntity(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass, Entity &entity, PushConsts &push_constants); // int32_t camera_id, int32_t environment_id, int32_t diffuse_id, int32_t irradiance_id, float gamma, float exposure, std::vector<int32_t> &light_entity_ids, double time

        /* Creates an uninitialized material. Useful for preallocation. */
        Material();

        /* Creates a material with the given name and id. */
        Material(std::string name, uint32_t id);

        /* Returns a json string summarizing the material */
        std::string to_string();

        /* These methods choose which pipeline a material should use when drawing an entity */
        void show_pbr();
        void show_normals();
        void show_base_color();
        void show_texcoords();
        void show_blinn();
        void show_depth();
        void show_environment();

        /* Accessors / Mutators */
        void set_base_color(glm::vec4 color);
        void set_base_color(float r, float g, float b, float a);
        void set_roughness(float roughness);
        void set_metallic(float metallic);
        void set_transmission(float transmission);
        void set_transmission_roughness(float transmission_roughness);
        void set_ior(float ior);

        /* Certain constant material properties can be replaced with texture lookups. */
        void use_base_color_texture(uint32_t texture_id);
        void use_base_color_texture(Texture *texture);
        void clear_base_color_texture();

        void use_roughness_texture(uint32_t texture_id);
        void use_roughness_texture(Texture *texture);
        void clear_roughness_texture();

        /* A uniform base color can be replaced with per-vertex colors as well. */
        void use_vertex_colors(bool use);


    private:
    
        /*  A list of the material components, allocated statically */
        static Material materials[MAX_MATERIALS];

        /* A lookup table of name to material id */
        static std::map<std::string, uint32_t> lookupTable;
        
        /* A pointer to the mapped material SSBO. This memory is shared between the GPU and CPU. */
        static MaterialStruct* pinnedMemory;

        /* A vulkan buffer handle corresponding to the material SSBO  */
        static vk::Buffer ssbo;

        /* The corresponding material SSBO memory */
        static vk::DeviceMemory ssboMemory;

        /* A vector of vertex input binding descriptions, describing binding and stride of per vertex data. */
        static std::vector<vk::VertexInputBindingDescription> vertexInputBindingDescriptions;

        /* A vector of vertex input attribute descriptions, describing location, format, and offset of each binding */
        static std::vector<vk::VertexInputAttributeDescription> vertexInputAttributeDescriptions;
        
        /* A struct aggregating pipeline parameters, which configure each stage within a graphics pipeline 
            (rasterizer, input assembly, etc), with their corresponding graphics pipeline. */
        struct PipelineResources {
            PipelineParameters pipelineParameters;
            vk::Pipeline pipeline;
            vk::PipelineLayout pipelineLayout;
        };
        
        
        /* The descriptor set layout describing where component SSBOs are bound */
        static vk::DescriptorSetLayout componentDescriptorSetLayout;

        /* The descriptor set layout describing where the array of textures are bound */
        static vk::DescriptorSetLayout textureDescriptorSetLayout;

        /* The descriptor pool used to allocate the component descriptor set. */
        static vk::DescriptorPool componentDescriptorPool;

        /* The descriptor pool used to allocate the texture descriptor set. */
        static vk::DescriptorPool textureDescriptorPool;

        /* The descriptor set containing references to all component SSBO buffers to be used as uniforms. */
        static vk::DescriptorSet componentDescriptorSet;

        /* The descriptor set containing references to all array of textues to be used as uniforms. */
        static vk::DescriptorSet textureDescriptorSet;

        /* The pipeline resources for each of the possible material types */
        static std::map<vk::RenderPass, PipelineResources> uniformColor;
        static std::map<vk::RenderPass, PipelineResources> blinn;
        static std::map<vk::RenderPass, PipelineResources> pbr;
        static std::map<vk::RenderPass, PipelineResources> texcoordsurface;
        static std::map<vk::RenderPass, PipelineResources> normalsurface;
        static std::map<vk::RenderPass, PipelineResources> skybox;
        static std::map<vk::RenderPass, PipelineResources> depth;

        /* Wrapper for shader module creation.  */
        static vk::ShaderModule CreateShaderModule(const std::vector<char>& code);

        /* Wraps the vulkan boilerplate for creation of a graphics pipeline */
        static void CreatePipeline(
            std::vector<vk::PipelineShaderStageCreateInfo> shaderStages,
            std::vector<vk::VertexInputBindingDescription> bindingDescriptions,
            std::vector<vk::VertexInputAttributeDescription> attributeDescriptions,
            std::vector<vk::DescriptorSetLayout> componentDescriptorSetLayouts,
            PipelineParameters parameters,
            vk::RenderPass renderpass,
            uint32 subpass,
            vk::Pipeline &pipeline,
            vk::PipelineLayout &layout 
        );

        /* Creates all possible descriptor set layout combinations */
        static void CreateDescriptorSetLayouts();

        /* Creates the descriptor pool where the descriptor sets will be allocated from. */
        static void CreateDescriptorPool();

        /* Creates the vulkan vertex input binding descriptions required to create a pipeline. */
        static void CreateVertexInputBindingDescriptions();

        /* Creates the vulkan vertex attribute binding descriptions required to create a pipeline. */
        static void CreateVertexAttributeDescriptions();

        /* Creates the SSBO which will contain all material components, and maps that SSBO to a pinned memory pointer */
        static void CreateSSBO();
        
        /* The structure containing all shader material properties. This is what's coppied into the SSBO per instance */
        MaterialStruct material_struct;

        /* An enumeration used to select a pipeline type for use when drawing a given entity. */
        enum RenderMode { BLINN, PBR, NORMAL, TEXCOORD, SKYBOX, BASECOLOR, DEPTH };
        RenderMode renderMode = PBR;
};
