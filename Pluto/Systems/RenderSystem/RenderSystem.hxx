#pragma once

#include "Pluto/Tools/System.hxx"
#include "Pluto/Libraries/GLFW/GLFW.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"

#include "Pluto/Systems/RenderSystem/PushConstants.hxx"
#include "Pluto/Systems/RenderSystem/PipelineParameters.hxx"

#include <stack>

class Entity;
class Texture;
struct VisibleEntityInfo;

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

enum RenderSystemOptions : uint32_t {
    RASTERIZE_MULTIVIEW = 0,
    DISABLE_REVERSE_Z_PROJECTION = 1,
    ENABLE_TAA = 2,
    ENABLE_SVGF_ATROUS = 3,
    RASTERIZE_BOUNDING_BOX = 4,
    ENABLE_BLUE_NOISE = 5,
    ENABLE_PROGRESSIVE_REFINEMENT = 6,
    ENABLE_SVGF_TAA = 7,
    SHOW_DIRECT_ILLUMINATION = 8,
    SHOW_INDIRECT_ILLUMINATION = 9,
    SHOW_ALBEDO = 10,
};

namespace Systems 
{
    class RenderSystem : public System {
        public:
            static RenderSystem* Get();
            bool initialize();
            bool start();
            bool stop();

            void set_gamma(float gamma);
            void set_exposure(float exposure);

            void set_environment_map(int32_t id);
            void set_environment_map(Texture *texture);
            void set_environment_roughness(float roughness);
            void clear_environment_map();
            
            void set_irradiance_map(int32_t id);
            void set_irradiance_map(Texture *texture);
            void clear_irradiance_map();

            void set_diffuse_map(int32_t id);
            void set_diffuse_map(Texture *texture);
            void clear_diffuse_map();

            void set_top_sky_color(glm::vec3 color);
            void set_bottom_sky_color(glm::vec3 color);
            void set_top_sky_color(float r, float g, float b);
            void set_bottom_sky_color(float r, float g, float b);
            void set_sky_transition(float transition);

            void use_openvr(bool useOpenVR);
            void use_openvr_hidden_area_masks(bool use_masks);

            /* If RTX Raytracing is enabled, builds a top level BVH for all created meshes. (TODO, account for mesh transformations) */
            void build_top_level_bvh(bool submit_immediately = false);

            void enable_ray_tracing(bool enable);
            void enable_svgf_taa(bool enable);
            void enable_svgf_atrous(bool enable);
            void enable_taa(bool enable);
            void set_atrous_sigma(float sigma);
            void set_atrous_iterations(int iterations);
            void enable_progressive_refinement(bool enable);
            void enable_tone_mapping(bool enable);

            void set_max_bounces(uint32_t max_bounces);
            void enable_blue_noise(bool enable);

            void show_direct_illumination(bool enable);
            void show_indirect_illumination(bool enable);
            void show_albedo(bool enable);

            /* Initializes the vulkan resources required to render during the specified renderpass */
            void setup_graphics_pipelines(vk::RenderPass renderpass, uint32_t sampleCount, bool use_depth_prepass);

            float get_seconds_per_frame();

            void reset_progressive_refinement();

            void enqueue_bvh_rebuild();

        private:
            PushConsts push_constants;            
            bool using_openvr = false;
            bool using_vr_hidden_area_masks = true;
            bool ray_tracing_enabled = false;
            bool taa_enabled = false;
            bool svgf_taa_enabled = false;
            bool svgf_atrous_enabled = false;
            bool progressive_refinement_enabled = false;
            bool tone_mapping_enabled = true;
            double lastTime, currentTime;
            float atrous_sigma = 1.0;
            int atrous_iterations = 5;
            float ms_per_frame;
            int max_bounces = 1;
            bool top_level_acceleration_structure_built = false;
            
            /* A vector of vertex input binding descriptions, describing binding and stride of per vertex data. */
            std::vector<vk::VertexInputBindingDescription> vertexInputBindingDescriptions;

            /* A vector of vertex input attribute descriptions, describing location, format, and offset of each binding */
            std::vector<vk::VertexInputAttributeDescription> vertexInputAttributeDescriptions;
            
            /* A struct aggregating pipeline parameters, which configure each stage within a graphics pipeline 
                (rasterizer, input assembly, etc), with their corresponding graphics pipeline. */
            struct RasterPipelineResources {
                PipelineParameters pipelineParameters;
                std::unordered_map<PipelineType, vk::Pipeline> pipelines;
                vk::PipelineLayout pipelineLayout;
                bool ready = false;
            };

            struct RaytracingPipelineResources {
                vk::Pipeline pipeline;
                vk::PipelineLayout pipelineLayout;
                vk::Buffer shaderBindingTable;
                vk::DeviceMemory shaderBindingTableMemory;
                bool ready = false;
            };

            struct ComputePipelineResources {
                vk::Pipeline pipeline;
                vk::PipelineLayout pipelineLayout;
                bool ready = false;
            };
            
            /* The descriptor set layout describing where component SSBOs are bound */
            vk::DescriptorSetLayout componentDescriptorSetLayout;

            /* The descriptor set layout describing where the array of textures are bound */
            vk::DescriptorSetLayout textureDescriptorSetLayout;

            /* TODO */
            vk::DescriptorSetLayout gbufferDescriptorSetLayout;
            
            /* The descriptor set layout describing per vertex information for all meshes */
            vk::DescriptorSetLayout positionsDescriptorSetLayout;
            vk::DescriptorSetLayout normalsDescriptorSetLayout;
            vk::DescriptorSetLayout colorsDescriptorSetLayout;
            vk::DescriptorSetLayout texcoordsDescriptorSetLayout;
            vk::DescriptorSetLayout indexDescriptorSetLayout;

            /* The descriptor set layout describing where acceleration structures and 
                raytracing related data are bound */
            vk::DescriptorSetLayout raytracingDescriptorSetLayout;

            /* The descriptor pool used to allocate the component descriptor set. */
            vk::DescriptorPool componentDescriptorPool;

            /* The descriptor pool used to allocate the texture descriptor set. */
            vk::DescriptorPool textureDescriptorPool;

            /* Descriptor pool for g buffer descriptor sets */
            /* I have no idea why, but on Intel, I cannot allocate more than one descriptor set from a pool... */
            std::vector<vk::DescriptorPool> gbufferDescriptorPools;
            int gbufferPoolIdx = 0;

            /* The descriptor pool used to allocate the vertex descriptor set. */
            vk::DescriptorPool positionsDescriptorPool;
            vk::DescriptorPool normalsDescriptorPool;
            vk::DescriptorPool colorsDescriptorPool;
            vk::DescriptorPool texcoordsDescriptorPool;
            vk::DescriptorPool indexDescriptorPool;

            /* The descriptor pool used to allocate the raytracing descriptor set */
            vk::DescriptorPool raytracingDescriptorPool;

            /* The descriptor set containing references to all component SSBO buffers to be used as uniforms. */
            vk::DescriptorSet componentDescriptorSet;

            /* The descriptor set containing references to all array of textues to be used as uniforms. */
            vk::DescriptorSet textureDescriptorSet;

            /* TODO */
            std::map<std::pair<uint32_t, uint32_t>, vk::DescriptorSet> gbufferDescriptorSets;

            /* The descriptor set containing references to all vertices of all mesh components */
            vk::DescriptorSet positionsDescriptorSet;
            vk::DescriptorSet normalsDescriptorSet;
            vk::DescriptorSet colorsDescriptorSet;
            vk::DescriptorSet texcoordsDescriptorSet;
            vk::DescriptorSet indexDescriptorSet;

            /* The descriptor set containing references to acceleration structures and textures to trace onto. */
            vk::DescriptorSet raytracingDescriptorSet;
            
            /* The pipeline resources for each of the possible material types */
            // std::map<vk::RenderPass, RasterPipelineResources> uniformColor;
            // std::map<vk::RenderPass, RasterPipelineResources> blinn;
            std::map<vk::RenderPass, RasterPipelineResources> pbr;
            // std::map<vk::RenderPass, RasterPipelineResources> texcoordsurface;
            // std::map<vk::RenderPass, RasterPipelineResources> normalsurface;
            std::map<vk::RenderPass, RasterPipelineResources> skybox;
            // std::map<vk::RenderPass, RasterPipelineResources> depth;
            std::map<vk::RenderPass, RasterPipelineResources> volume;
            std::map<vk::RenderPass, RasterPipelineResources> shadowmap;
            std::map<vk::RenderPass, RasterPipelineResources> fragmentdepth;
            // std::map<vk::RenderPass, RasterPipelineResources> fragmentposition;
            std::map<vk::RenderPass, RasterPipelineResources> vrmask;
            
            std::map<vk::RenderPass, RasterPipelineResources> gbuffers;

            std::map<vk::RenderPass, RaytracingPipelineResources> path_tracer;

            /* Wrapper for shader module creation.  */
            vk::ShaderModule create_shader_module(std::string name, const std::vector<char>& code);

            /* Cached modules */
            std::unordered_map<std::string, vk::ShaderModule> shaderModuleCache;

            /* EXPLAIN THIS */
            void setup_raytracing_shader_binding_table(vk::RenderPass renderpass);

            ComputePipelineResources edgedetect;
            ComputePipelineResources gaussian_x;
            ComputePipelineResources gaussian_y;
            ComputePipelineResources svgf_taa;
            ComputePipelineResources svgf_atrous_filter;
            ComputePipelineResources svgf_remodulate;
            ComputePipelineResources progressive_refinement;
            ComputePipelineResources tone_mapping;
            ComputePipelineResources taa;

            void setup_compute_pipelines();

            /* Wraps the vulkan boilerplate for creation of a graphics pipeline */
            void create_raster_pipeline(
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
            void create_descriptor_set_layouts();

            /* Creates the descriptor pool where the descriptor sets will be allocated from. */
            void create_descriptor_pools();

            /* Creates the vulkan vertex input binding descriptions required to create a pipeline. */
            void create_vertex_input_binding_descriptions();

            /* Creates the vulkan vertex attribute binding descriptions required to create a pipeline. */
            void create_vertex_attribute_descriptions();
            
            /* Copies SSBO / texture handles into the texture and component descriptor sets. 
                Also, allocates the descriptor sets if not yet allocated. */
            void update_raster_descriptor_sets();

            /* EXPLAIN THIS */
            void update_gbuffer_descriptor_sets(Entity &camera_entity, uint32_t rp_idx);

            /* EXPLAIN THIS */            
            void update_raytracing_descriptor_sets(vk::AccelerationStructureNV &tlas);

            /* Records a bind of all descriptor sets to each possible pipeline to the given command buffer. Call this at the beginning of a renderpass. */
            void bind_raster_descriptor_sets(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass);

            void bind_compute_descriptor_sets(vk::CommandBuffer &command_buffer, Entity &camera_entity, uint32_t rp_idx);

            void bind_raytracing_descriptor_sets(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass, Entity &camera_entity, uint32_t rp_idx);

            /* Records a draw of the supplied entity to the current command buffer. Call this during a renderpass. */
            void draw_entity(
                vk::CommandBuffer &command_buffer, 
                vk::RenderPass &render_pass, 
                Entity &entity, 
                PushConsts &push_constants, 
                RenderMode rendermode_override = RenderMode::RENDER_MODE_NONE, 
                PipelineType pipeline_type_override = PipelineType::PIPELINE_TYPE_NORMAL,
                bool render_bounding_box_override = false
            ); // int32_t camera_id, int32_t environment_id, int32_t diffuse_id, int32_t irradiance_id, float gamma, float exposure, std::vector<int32_t> &light_entity_ids, double time

            void trace_rays(
                vk::CommandBuffer &command_buffer, 
                vk::RenderPass &render_pass, 
                PushConsts &push_constants,
                Texture &texture);

            /* TODO... */
            void reset_bound_material();

            RenderMode currentlyBoundRenderMode = RenderMode::RENDER_MODE_NONE;
            
            PipelineType currentlyBoundPipelineType = PipelineType::PIPELINE_TYPE_NORMAL;

            vk::RenderPass currentRenderpass = vk::RenderPass();

            struct ComputeNode
            {
                vk::CommandBuffer command_buffer;
                std::vector<std::shared_ptr<ComputeNode>> dependencies;
                std::vector<std::shared_ptr<ComputeNode>> children;
                std::vector<std::string> connected_windows;
                std::vector<vk::Semaphore> signal_semaphores;
                std::vector<vk::Semaphore> window_signal_semaphores;
                uint32_t level;
                uint32_t queue_idx;
            };

            std::vector<std::stack<vk::Semaphore>> availableSemaphores;
            std::vector<std::stack<vk::Semaphore>> usedSemaphores;
            std::vector<std::stack<vk::Fence>> availableFences;
            std::vector<std::stack<vk::Fence>> usedFences;

            void initialize_semaphore_pools() 
            {
                while (availableSemaphores.size() <= currentFrame) {
                    availableSemaphores.push_back(std::stack<vk::Semaphore>());
                }
                while (usedSemaphores.size() <= currentFrame) {
                    usedSemaphores.push_back(std::stack<vk::Semaphore>());
                }
            }

            void initialize_fence_pools() 
            {
                while (availableFences.size() <= currentFrame) {
                    availableFences.push_back(std::stack<vk::Fence>());
                }
                while (usedFences.size() <= currentFrame) {
                    usedFences.push_back(std::stack<vk::Fence>());
                }
            }

            void semaphores_submitted()
            {
                initialize_semaphore_pools();
                while (!usedSemaphores[currentFrame].empty()) {
                    vk::Semaphore semaphore = usedSemaphores[currentFrame].top();
                    usedSemaphores[currentFrame].pop();
                    availableSemaphores[currentFrame].push(semaphore);
                }
            }

            void fences_submitted()
            {
                initialize_fence_pools();
                while (!usedFences[currentFrame].empty()) {
                    vk::Fence fence = usedFences[currentFrame].top();
                    usedFences[currentFrame].pop();
                    availableFences[currentFrame].push(fence);
                }
            }

            vk::Semaphore get_semaphore() {
                initialize_semaphore_pools();
                if (!availableSemaphores[currentFrame].empty()) {
                    vk::Semaphore semaphore = availableSemaphores[currentFrame].top();
                    availableSemaphores[currentFrame].pop();
                    usedSemaphores[currentFrame].push(semaphore);
                    return semaphore;
                }

                vk::SemaphoreCreateInfo semaphoreInfo;
                auto vulkan = Libraries::Vulkan::Get(); auto device = vulkan->get_device();
                vk::Semaphore semaphore = device.createSemaphore(semaphoreInfo);
                usedSemaphores[currentFrame].push(semaphore);
                return semaphore;
            }

            vk::Fence get_fence() {
                auto vulkan = Libraries::Vulkan::Get(); auto device = vulkan->get_device();
                initialize_fence_pools();
                if (!availableFences[currentFrame].empty()) {
                    vk::Fence fence = availableFences[currentFrame].top();
                    availableFences[currentFrame].pop();
                    usedFences[currentFrame].push(fence);
                    device.resetFences({fence});
                    return fence;
                }

                vk::FenceCreateInfo fenceInfo;
                vk::Fence fence = device.createFence(fenceInfo);
                usedFences[currentFrame].push(fence);
                return fence;
            }

            bool vulkan_resources_created = false;

            uint32_t currentFrame = 0;
            
            vk::CommandBuffer main_command_buffer;

            /* Declaration of an RTX geometry instance. This struct is described in the Khronos 
                specification to be exactly this, so don't modify! */
            struct VkGeometryInstance
            {
                float transform[12];
                uint32_t instanceId : 24;
                uint32_t mask : 8;
                uint32_t instanceOffset : 24;
                uint32_t flags : 8;
                uint64_t accelerationStructureHandle;
            };

            static_assert(sizeof(VkGeometryInstance) == 64, "VkGeometryInstance structure compiles to incorrect size");

            /* A handle to an RTX top level BVH */
            vk::AccelerationStructureNV topAS;
            vk::DeviceMemory topASMemory;
            uint64_t topASHandle;

            vk::DeviceMemory accelerationStructureBuildScratchMemory;
            vk::Buffer accelerationStructureBuildScratchBuffer;

            vk::DeviceMemory accelerationStructureUpdateScratchMemory;
            vk::Buffer accelerationStructureUpdateScratchBuffer;

            /* A handle to an RTX buffer of geometry instances used to build the top level BVH */
            vk::Buffer instanceBuffer;
            vk::DeviceMemory instanceBufferMemory;

            // std::vector<vk::Semaphore> main_command_buffer_semaphores;
            // vk::Fence main_fence;

            bool main_command_buffer_recorded = false;
            bool main_command_buffer_presenting = false;

            std::vector<vk::Semaphore> final_renderpass_semaphores;
            std::vector<vk::Fence> final_fences;
            uint32_t max_frames_in_flight = 2;

            bool update_push_constants();
            void record_render_commands();
            void record_ray_trace_pass(Entity &camera_entity);
            void record_compute_pass(Entity &camera_entity);
            void record_raster_renderpass(Entity &camera_entity, std::vector<std::vector<VisibleEntityInfo>> &visible_entities);
            void record_depth_prepass(Entity &camera_entity, std::vector<std::vector<VisibleEntityInfo>> &visible_entities);
            void record_blit_camera(Entity &camera_entity, std::map<std::string, std::pair<Camera *, uint32_t>> &window_to_cam);
            void record_cameras();
            void record_blit_textures();
            void enqueue_render_commands();
            void download_visibility_queries();
            void mark_cameras_as_render_complete();

            void stream_frames();
            void update_openvr_transforms();
            void present_openvr_frames();
            void allocate_vulkan_resources();
            void release_vulkan_resources();
            RenderSystem();
            ~RenderSystem();            
    };
}
