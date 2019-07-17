#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#define GLM_FORCE_RIGHT_HANDED

#include <glm/glm.hpp>
#include <glm/common.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <map>
#include <unordered_map>
#include <mutex>
#include <string>

#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Tools/StaticFactory.hxx"

#include "./MaterialStruct.hxx"
#include "./PushConstants.hxx"
#include "Pluto/Material/PipelineParameters.hxx"

class Entity;

class Texture;
class Camera;

/* An enumeration used to select a pipeline type for use when drawing a given entity. */
enum RenderMode : uint32_t { 
    RENDER_MODE_BLINN, 
    RENDER_MODE_PBR, 
    RENDER_MODE_NORMAL, 
    RENDER_MODE_TEXCOORD, 
    RENDER_MODE_SKYBOX, 
    RENDER_MODE_BASECOLOR, 
    RENDER_MODE_DEPTH, 
    RENDER_MODE_VOLUME, 
    RENDER_MODE_SHADOWMAP, 
    RENDER_MODE_FRAGMENTDEPTH, 
    RENDER_MODE_FRAGMENTPOSITION, 
    RENDER_MODE_VRMASK, 
    RENDER_MODE_HIDDEN, 
    RENDER_MODE_NONE 
};

enum PipelineType : uint32_t {
    PIPELINE_TYPE_NORMAL = 0,
    PIPELINE_TYPE_DEPTH_WRITE_DISABLED = 1,
    PIPELINE_TYPE_DEPTH_TEST_LESS = 2,
    PIPELINE_TYPE_DEPTH_TEST_GREATER = 3,
    PIPELINE_TYPE_WIREFRAME = 4
};

class Material : public StaticFactory
{
    friend class StaticFactory;
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
        static void Delete(std::string name);

        /* Deallocates a material with the given id */
        static void Delete(uint32_t id);

        /* Initializes the vulkan resources required to render during the specified renderpass */
        static void SetupGraphicsPipelines(vk::RenderPass renderpass, uint32_t sampleCount, bool use_depth_prepass);

        /* EXPLAIN THIS */
        static void SetupRaytracingShaderBindingTable(vk::RenderPass renderpass);

        /* Initializes all vulkan descriptor resources, as well as the Mesh factory. */
        static void Initialize();

        /* TODO: Explain this */
        static bool IsInitialized();

        /* Transfers all material components to an SSBO */
        static void UploadSSBO(vk::CommandBuffer command_buffer);

        /* Copies SSBO / texture handles into the texture and component descriptor sets. 
            Also, allocates the descriptor sets if not yet allocated. */
        static void UpdateRasterDescriptorSets();

        /* EXPLAIN THIS */
        static void UpdateRaytracingDescriptorSets(vk::AccelerationStructureNV &tlas, Entity &camera_entity);

        /* Returns the SSBO vulkan buffer handle */
        static vk::Buffer GetSSBO();

        /* Returns the size in bytes of the current material SSBO */
        static uint32_t GetSSBOSize();

        /* Releases vulkan resources */
        static void CleanUp();

        /* Records a bind of all descriptor sets to each possible pipeline to the given command buffer. Call this at the beginning of a renderpass. */
        static void BindDescriptorSets(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass);

        static void BindRayTracingDescriptorSets(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass);
        
        /* Records a draw of the supplied entity to the current command buffer. Call this during a renderpass. */
        static void DrawEntity(
            vk::CommandBuffer &command_buffer, 
            vk::RenderPass &render_pass, 
            Entity &entity, 
            PushConsts &push_constants, 
            RenderMode rendermode_override = RenderMode::RENDER_MODE_NONE, 
            PipelineType pipeline_type_override = PipelineType::PIPELINE_TYPE_NORMAL,
            bool render_bounding_box_override = false
        ); // int32_t camera_id, int32_t environment_id, int32_t diffuse_id, int32_t irradiance_id, float gamma, float exposure, std::vector<int32_t> &light_entity_ids, double time

        static void TraceRays(
            vk::CommandBuffer &command_buffer, 
            vk::RenderPass &render_pass, 
            PushConsts &push_constants,
            Texture &texture);

        /* TODO... */
        static void ResetBoundMaterial();

        /* Returns a json string summarizing the material */
        std::string to_string();

        /* These methods choose which pipeline a material should use when drawing an entity */
        void show_pbr();
        void show_normals();
        void show_base_color();
        void show_texcoords();
        void show_blinn();
        void show_fragment_depth();
        void show_vr_mask();
        void show_depth();
        void show_position();
        void show_environment();
        void show_volume();

        /* This method prevents an entity from rendering. */
        void hide();

        /* Accessors / Mutators */
        void set_base_color(glm::vec4 color);
        void set_base_color(float r, float g, float b, float a);
        glm::vec4 get_base_color();
        void set_roughness(float roughness);
        float get_roughness();
        void set_metallic(float metallic);
        float get_metallic();
        void set_transmission(float transmission);
        float get_transmission();
        void set_transmission_roughness(float transmission_roughness);
        float get_transmission_roughness();
        void set_ior(float ior);

        /* Certain constant material properties can be replaced with texture lookups. */
        void set_base_color_texture(uint32_t texture_id);
        void set_base_color_texture(Texture *texture);
        void clear_base_color_texture();

        void set_roughness_texture(uint32_t texture_id);
        void set_roughness_texture(Texture *texture);
        void clear_roughness_texture();

        void set_bump_texture(uint32_t texture_id);
        void set_bump_texture(Texture *texture);
        void clear_bump_texture();

        void set_alpha_texture(uint32_t texture_id);
        void set_alpha_texture(Texture *texture);
        void clear_alpha_texture();

        /* A uniform base color can be replaced with per-vertex colors as well. */
        void use_vertex_colors(bool use);

        /* The volume texture to be used by volume type materials */
        void set_volume_texture(uint32_t texture_id);
        void set_volume_texture(Texture *texture);

        void set_transfer_function_texture(uint32_t texture_id);
        void set_transfer_function_texture(Texture *texture);
        void clear_transfer_function_texture();

        bool contains_transparency();

    private:
        /* Creates an uninitialized material. Useful for preallocation. */
        Material();

        /* Creates a material with the given name and id. */
        Material(std::string name, uint32_t id);

        /* TODO */
		static std::shared_ptr<std::mutex> creation_mutex;

        /* TODO */
	    static bool Initialized;
    
        /*  A list of the material components, allocated statically */
        static Material materials[MAX_MATERIALS];

        /* A lookup table of name to material id */
        static std::map<std::string, uint32_t> lookupTable;
        
        /* A pointer to the mapped material SSBO. This memory is shared between the GPU and CPU. */
        static MaterialStruct* pinnedMemory;

        /* A vulkan buffer handle corresponding to the material SSBO  */
        static vk::Buffer SSBO;

        /* The corresponding material SSBO memory */
        static vk::DeviceMemory SSBOMemory;

        /* TODO  */
        static vk::Buffer stagingSSBO;

        /* TODO */
        static vk::DeviceMemory stagingSSBOMemory;

        /* A vector of vertex input binding descriptions, describing binding and stride of per vertex data. */
        static std::vector<vk::VertexInputBindingDescription> vertexInputBindingDescriptions;

        /* A vector of vertex input attribute descriptions, describing location, format, and offset of each binding */
        static std::vector<vk::VertexInputAttributeDescription> vertexInputAttributeDescriptions;
        
        /* A struct aggregating pipeline parameters, which configure each stage within a graphics pipeline 
            (rasterizer, input assembly, etc), with their corresponding graphics pipeline. */
        struct RasterPipelineResources {
            PipelineParameters pipelineParameters;
            std::unordered_map<PipelineType, vk::Pipeline> pipelines;
            vk::PipelineLayout pipelineLayout;
        };

        struct RaytracingPipelineResources {
            vk::Pipeline pipeline;
            vk::PipelineLayout pipelineLayout;
            vk::Buffer shaderBindingTable;
            vk::DeviceMemory shaderBindingTableMemory;
        };
        
        /* The descriptor set layout describing where component SSBOs are bound */
        static vk::DescriptorSetLayout componentDescriptorSetLayout;

        /* The descriptor set layout describing where the array of textures are bound */
        static vk::DescriptorSetLayout textureDescriptorSetLayout;

        /* The descriptor set layout describing per vertex information for all meshes */
        static vk::DescriptorSetLayout positionsDescriptorSetLayout;
        static vk::DescriptorSetLayout normalsDescriptorSetLayout;
        static vk::DescriptorSetLayout colorsDescriptorSetLayout;
        static vk::DescriptorSetLayout texcoordsDescriptorSetLayout;
        static vk::DescriptorSetLayout indexDescriptorSetLayout;

        /* The descriptor set layout describing where acceleration structures and 
            raytracing related data are bound */
        static vk::DescriptorSetLayout raytracingDescriptorSetLayout;

        /* The descriptor pool used to allocate the component descriptor set. */
        static vk::DescriptorPool componentDescriptorPool;

        /* The descriptor pool used to allocate the texture descriptor set. */
        static vk::DescriptorPool textureDescriptorPool;

        /* The descriptor pool used to allocate the vertex descriptor set. */
        static vk::DescriptorPool positionsDescriptorPool;
        static vk::DescriptorPool normalsDescriptorPool;
        static vk::DescriptorPool colorsDescriptorPool;
        static vk::DescriptorPool texcoordsDescriptorPool;
        static vk::DescriptorPool indexDescriptorPool;

        /* The descriptor pool used to allocate the raytracing descriptor set */
        static vk::DescriptorPool raytracingDescriptorPool;

        /* The descriptor set containing references to all component SSBO buffers to be used as uniforms. */
        static vk::DescriptorSet componentDescriptorSet;

        /* The descriptor set containing references to all array of textues to be used as uniforms. */
        static vk::DescriptorSet textureDescriptorSet;

        /* The descriptor set containing references to all vertices of all mesh components */
        static vk::DescriptorSet positionsDescriptorSet;
        static vk::DescriptorSet normalsDescriptorSet;
        static vk::DescriptorSet colorsDescriptorSet;
        static vk::DescriptorSet texcoordsDescriptorSet;
        static vk::DescriptorSet indexDescriptorSet;

        /* The descriptor set containing references to acceleration structures and textures to trace onto. */
        static vk::DescriptorSet raytracingDescriptorSet;
        
        /* The pipeline resources for each of the possible material types */
        // static std::map<vk::RenderPass, RasterPipelineResources> uniformColor;
        // static std::map<vk::RenderPass, RasterPipelineResources> blinn;
        static std::map<vk::RenderPass, RasterPipelineResources> pbr;
        // static std::map<vk::RenderPass, RasterPipelineResources> texcoordsurface;
        // static std::map<vk::RenderPass, RasterPipelineResources> normalsurface;
        static std::map<vk::RenderPass, RasterPipelineResources> skybox;
        // static std::map<vk::RenderPass, RasterPipelineResources> depth;
        static std::map<vk::RenderPass, RasterPipelineResources> volume;
        static std::map<vk::RenderPass, RasterPipelineResources> shadowmap;
        static std::map<vk::RenderPass, RasterPipelineResources> fragmentdepth;
        // static std::map<vk::RenderPass, RasterPipelineResources> fragmentposition;
        static std::map<vk::RenderPass, RasterPipelineResources> vrmask;
        
        static std::map<vk::RenderPass, RasterPipelineResources> gbuffers;

        static std::map<vk::RenderPass, RaytracingPipelineResources> rttest;

        /* Wrapper for shader module creation.  */
        static vk::ShaderModule CreateShaderModule(std::string name, const std::vector<char>& code);

        /* Cached modules */
        static std::unordered_map<std::string, vk::ShaderModule> shaderModuleCache;

        /* Wraps the vulkan boilerplate for creation of a graphics pipeline */
        static void CreateRasterPipeline(
            std::vector<vk::PipelineShaderStageCreateInfo> shaderStages,
            std::vector<vk::VertexInputBindingDescription> bindingDescriptions,
            std::vector<vk::VertexInputAttributeDescription> attributeDescriptions,
            std::vector<vk::DescriptorSetLayout> componentDescriptorSetLayouts,
            PipelineParameters parameters,
            vk::RenderPass renderpass,
            uint32 subpass,
            std::unordered_map<PipelineType, vk::Pipeline> &pipelines,
            vk::PipelineLayout &layout 
        );

        /* Creates all possible descriptor set layout combinations */
        static void CreateDescriptorSetLayouts();

        /* Creates the descriptor pool where the descriptor sets will be allocated from. */
        static void CreateDescriptorPools();

        /* Creates the vulkan vertex input binding descriptions required to create a pipeline. */
        static void CreateVertexInputBindingDescriptions();

        /* Creates the vulkan vertex attribute binding descriptions required to create a pipeline. */
        static void CreateVertexAttributeDescriptions();

        /* Creates the SSBO which will contain all material components, and maps that SSBO to a pinned memory pointer */
        static void CreateSSBO();
        
        /* The structure containing all shader material properties. This is what's coppied into the SSBO per instance */
        MaterialStruct material_struct;
        
        RenderMode renderMode = RENDER_MODE_PBR;

        static RenderMode currentlyBoundRenderMode;
        
        static PipelineType currentlyBoundPipelineType;

        static vk::RenderPass currentRenderpass;
};
