#pragma once

#include "Foton/Tools/System.hxx"
#include "Foton/Libraries/GLFW/GLFW.hxx"
#include "Foton/Libraries/Vulkan/Vulkan.hxx"

#include "Foton/Systems/RenderSystem/PushConstants.hxx"
#include "Foton/Systems/RenderSystem/PipelineParameters.hxx"

#include <stack>

class Entity;
class Texture;
struct VisibleEntityInfo;

/* An enumeration used to select a pipeline type for use when drawing a given entity. */
enum RenderMode : uint32_t { 
    RENDER_MODE_PRIMARY_VISIBILITY, 
    RENDER_MODE_SKYBOX, 
    RENDER_MODE_SHADOWMAP, 
    RENDER_MODE_FRAGMENTDEPTH, 
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
    ENABLE_ASVGF_GRADIENT = 3,
    ENABLE_ASVGF_GRADIENT_ATROUS = 15,
    ENABLE_ASVGF_TEMPORAL_ACCUMULATION = 11,
    ENABLE_ASVGF_VARIANCE_ESTIMATION = 12,
    ENABLE_ASVGF_ATROUS = 13,
    ENABLE_ASVGF = 14,
    RASTERIZE_BOUNDING_BOX = 4,
    ENABLE_BLUE_NOISE = 5,
    ENABLE_PROGRESSIVE_REFINEMENT = 6,
    ENABLE_SVGF_TAA = 7,
    SHOW_DIRECT_ILLUMINATION = 8,
    SHOW_INDIRECT_ILLUMINATION = 9,
    SHOW_ALBEDO = 10,
    ENABLE_ANALYTICAL_AREA_LIGHTS=16,
    ENABLE_BILATERAL_UPSAMPLING=17,
    FORCE_FLAT_REFLECTORS=18,
};

namespace Systems 
{
    class RenderSystem : public System {
        friend class Camera; // temporary. Required so camera can read some rendersystem parameters which are being migrated.

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
            void set_environment_intensity(float intensity);
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

            void enable_ray_tracing(bool enable);
            void rasterize_primary_visibility(bool enable);
            
            void enable_asvgf(bool enable);
            void enable_asvgf_gradient(bool enable);
            void set_asvgf_gradient_reconstruction_iterations(int iterations);
            void set_asvgf_gradient_reconstruction_sigma(float sigma);
            void enable_asvgf_temporal_accumulation(bool enable);
            void enable_asvgf_variance_estimation(bool enable);
            void enable_asvgf_atrous(bool enable);
            void set_asvgf_atrous_iterations(int iterations);
            void set_asvgf_atrous_sigma(float sigma);
            void set_asvgf_gradient_influence(float influence);
            void set_asvgf_diffuse_gradient_influence(float influence);
            void set_asvgf_specular_gradient_influence(float influence);

            void set_direct_diffuse_blur(float percent);
            void set_indirect_diffuse_blur(float percent);
            void set_direct_glossy_blur(float percent);
            void set_indirect_glossy_blur(float percent);

            
            void enable_taa(bool enable);
            void enable_progressive_refinement(bool enable);
            void enable_tone_mapping(bool enable);

            void set_max_bounces(uint32_t max_bounces);
            void enable_blue_noise(bool enable);

            void enable_analytical_arealights(bool enable);

            void enable_median(bool enable);
            void enable_bilateral_upsampling(bool enable);
            void set_upsampling_radius(float radius);
            void force_flat_reflectors(bool force);
            void force_nearest_temporal_sampling(bool force);

            float test_param = 1.0;
            void set_debug_parameter(float param) 
            {
                test_param = param;
            }

            void show_direct_illumination(bool enable);
            void show_indirect_illumination(bool enable);
            void show_albedo(bool enable);
            void show_gbuffer(uint32_t idx);

            /* Initializes the vulkan resources required to render during the specified renderpass */
            void setup_primary_visibility_raster_pipelines(vk::RenderPass renderpass, uint32_t sampleCount, bool use_depth_prepass);
            void setup_depth_prepass_raster_pipelines(vk::RenderPass renderpass, uint32_t sampleCount, bool use_depth_prepass, uint32_t num_g_buffers);
            void setup_shadow_map_raster_pipelines(vk::RenderPass renderpass, uint32_t sampleCount, bool use_depth_prepass);
            void setup_raytracing_pipelines();
            void setup_compute_pipelines();

            float get_milliseconds_per_frame();
            float get_milliseconds_per_acquire_swapchain_images();
            float get_milliseconds_per_record_commands();
            float get_milliseconds_per_record_raster_pass();
            float get_milliseconds_per_record_ray_trace_pass();
            float get_milliseconds_per_record_compute_pass();
            float get_milliseconds_per_enqueue_commands();
            float get_milliseconds_per_submit_commands();
            float get_milliseconds_per_present_frames();
            float get_milliseconds_per_download_visibility_queries();
            float get_milliseconds_per_upload_ssbo();
            float get_milliseconds_per_update_push_constants();
            float get_milliseconds_per_record_cameras();
            float get_milliseconds_per_record_blit_textures();

            void reset_progressive_refinement();

            void enqueue_bvh_rebuild();

            // todo: make this private...
            void create_camera_resources(uint32_t camera_id);
        private:
            PushConsts push_constants;            
            bool using_openvr = false;
            bool using_vr_hidden_area_masks = true;
            bool ray_tracing_enabled = false;
            bool rasterize_primary_visibility_enabled = false;
            bool taa_enabled = false;
            
            bool asvgf_enabled = false;
            bool asvgf_gradient_enabled = true;
            float asvgf_gradient_reconstruction_sigma = .5f;
            int asvgf_gradient_reconstruction_iterations = 3;
            bool asvgf_temporal_accumulation_enabled = true;
            bool asvgf_variance_estimation_enabled = true;
            bool asvgf_atrous_enabled = true;
            float asvgf_direct_diffuse_blur = .1f;
            float asvgf_indirect_diffuse_blur = 1.f;
            float asvgf_direct_glossy_blur = .1f;
            float asvgf_indirect_glossy_blur = .6f;
            int asvgf_atrous_iterations = 3;
            float asvgf_diffuse_gradient_influence = 1.0f;
            float asvgf_specular_gradient_influence = 0.1f;

            bool tone_mapping_enabled = true;
            bool progressive_refinement_enabled = false;

            double last_frame_time, last_raster_time, last_compute_time, last_raytrace_time, current_time;
            float ms_per_frame;
            float ms_per_acquire_swapchain_images;
            float ms_per_record_commands;
            float ms_per_record_raster_pass;
            float ms_per_record_compute_pass;
            float ms_per_record_raytrace_pass;
            float ms_per_enqueue_commands;
            float ms_per_submit_commands;
            float ms_per_present_frames;
            float ms_per_download_visibility_queries;
            float ms_per_upload_ssbo;
            float ms_per_update_push_constants;
            float ms_per_record_cameras;
            float ms_per_record_blit_textures;
            int max_bounces = 1;
            bool top_level_acceleration_structure_built = false;

            int gbuffer_override_idx = 0;

            bool enable_median_filter = true;
            bool enable_bilateral_upsampling_filter = true;
            float upsampling_filter_radius = 1.0 / 4.0;
            bool force_flat_reflectors_on = false;
            bool force_nearest_temporal_sampling_on = false;

            
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
            std::map<vk::RenderPass, RasterPipelineResources> raster_primary_visibility;
            std::map<vk::RenderPass, RasterPipelineResources> skybox;
            std::map<vk::RenderPass, RasterPipelineResources> shadowmap;
            std::map<vk::RenderPass, RasterPipelineResources> raster_depth_prepass;
            std::map<vk::RenderPass, RasterPipelineResources> raster_vrmask;

            RaytracingPipelineResources raytrace_primary_visibility;
            RaytracingPipelineResources diffuse_path_tracer;
            RaytracingPipelineResources specular_path_tracer;

            /* Wrapper for shader module creation.  */
            vk::ShaderModule create_shader_module(std::string name, const std::vector<char>& code);

            /* Cached modules */
            std::unordered_map<std::string, vk::ShaderModule> shaderModuleCache;

            ComputePipelineResources deferred_final;
            ComputePipelineResources edgedetect;
            ComputePipelineResources median_3x3;
            ComputePipelineResources bilateral_upsample;
            ComputePipelineResources gaussian_x;
            ComputePipelineResources gaussian_y;
            ComputePipelineResources svgf_remodulate;
            ComputePipelineResources asvgf_reproject_specular_seeds;
            ComputePipelineResources asvgf_compute_gradient;
            ComputePipelineResources asvgf_reconstruct_gradient;
            ComputePipelineResources asvgf_diffuse_temporal_accumulation;
            ComputePipelineResources asvgf_specular_temporal_accumulation;
            ComputePipelineResources asvgf_estimate_variance;
            ComputePipelineResources reconstruct_diffuse;
            ComputePipelineResources reconstruct_glossy;
            ComputePipelineResources progressive_refinement;
            ComputePipelineResources compositing;
            ComputePipelineResources copy_history;
            ComputePipelineResources taa;

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
            void bind_depth_prepass_descriptor_sets(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass);
            void bind_raster_primary_visibility_descriptor_sets(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass);
            void bind_shadow_map_descriptor_sets(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass);

            void bind_compute_descriptor_sets(vk::CommandBuffer &command_buffer, Entity &camera_entity, uint32_t rp_idx);

            void bind_raytracing_descriptor_sets(vk::CommandBuffer &command_buffer, Entity &camera_entity, uint32_t rp_idx);

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

            /* TODO... */
            void reset_bound_material();

            RenderMode currentlyBoundRenderMode = RenderMode::RENDER_MODE_NONE;
            
            PipelineType currentlyBoundPipelineType = PipelineType::PIPELINE_TYPE_NORMAL;

            /* Camera data */
            struct CameraResources 
            {
                vk::QueryPool queryPool;
                std::vector<bool> index_queried;
                std::vector<bool> next_index_queried;
                std::vector<uint64_t> previousQueryResults;
                std::vector<uint64_t> queryResults;
                std::vector<vk::RenderPass> primary_visibility_depth_prepasses;
                std::vector<vk::Framebuffer> primary_visibility_depth_prepass_framebuffers;
                std::vector<vk::RenderPass> primary_visibility_renderpasses;
                std::vector<vk::Framebuffer> primary_visibility_renderpass_framebuffers;
                
                // It seems a bit goofy to have a depth prepass on a shadow map,
                // but the prepass also allows for occlusion testing
                std::vector<vk::RenderPass> shadow_map_depth_prepasses;
                std::vector<vk::Framebuffer> shadow_map_depth_prepass_framebuffers;
                std::vector<vk::RenderPass> shadow_map_renderpasses;
                std::vector<vk::Framebuffer> shadow_map_renderpass_framebuffers;
                vk::CommandBuffer command_buffer;
                vk::CommandPool command_buffer_pool;
            };

            std::map<uint32_t, CameraResources> camera_resources;

            vk::RenderPass currentRenderpass = vk::RenderPass();

            void create_camera_render_passes(uint32_t camera_id, uint32_t layers = 1, uint32_t sample_count = 1);
	        void create_camera_frame_buffers(uint32_t camera_id, uint32_t layers);
            void create_camera_query_pool(uint32_t camera_id, uint32_t max_views);

            void reset_query_pool(vk::CommandBuffer command_buffer, uint32_t camera_id);
            std::vector<uint64_t> & get_query_pool_results(uint32_t camera_id);
            void download_query_pool_results(uint32_t camera_id);
            void begin_visibility_query(vk::CommandBuffer command_buffer, uint32_t camera_id, uint32_t renderpass_idx, uint32_t entity_id);
            void end_visibility_query(vk::CommandBuffer command_buffer, uint32_t camera_id, uint32_t renderpass_idx, uint32_t entity_id);
            bool is_entity_visible(uint32_t camera_id, uint32_t renderpass_idx, uint32_t entity_id);
            std::vector<std::vector<VisibleEntityInfo>> get_visible_entities(uint32_t camera_id, uint32_t camera_entity_id);

            /* Records vulkan commands to the given command buffer required to 
            	start a renderpass for the current camera setup. This should only be called by the render 
            	system, and only after the command buffer has begun recording commands. */
            /* Records vulkan commands to the given command buffer required to 
                end a renderpass for the current camera setup. */
            void begin_renderpass(vk::CommandBuffer command_buffer, vk::RenderPass renderpass, vk::Framebuffer framebuffer, uint32_t camera_id, uint32_t num_g_buffers);
            void end_renderpass(vk::CommandBuffer command_buffer, uint32_t camera_id);
            void begin_depth_prepass(vk::CommandBuffer command_buffer, vk::RenderPass renderpass, vk::Framebuffer framebuffer, uint32_t camera_id, uint32_t num_g_buffers);
            void end_depth_prepass(vk::CommandBuffer command_buffer, uint32_t camera_id);

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
            
            vk::CommandBuffer bvh_command_buffer;
            vk::CommandBuffer blit_command_buffer;

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

            bool bvh_command_buffer_recorded = false;
            bool blit_command_buffer_recorded = false;
            bool blit_command_buffer_presenting = false;

            std::vector<vk::Semaphore> final_renderpass_semaphores;
            std::vector<vk::Fence> final_fences;
            uint32_t max_frames_in_flight = 2;

            bool update_push_constants();
            void record_render_commands();
            void record_build_top_level_bvh(bool submit_immediately = false);
            void record_ray_trace_pass(Entity &camera_entity);
            void record_pre_compute_pass(Entity &camera_entity);
            void record_post_compute_pass(Entity &camera_entity);
            void record_raster_primary_visibility_renderpass(Entity &camera_entity, std::vector<std::vector<VisibleEntityInfo>> &visible_entities);
            void record_shadow_map_renderpass(Entity &camera_entity, std::vector<std::vector<VisibleEntityInfo>> &visible_entities);
            void record_blit_camera(Entity &camera_entity, std::map<std::string, std::pair<Camera *, uint32_t>> &window_to_cam);
            void record_cameras();
            void record_blit_textures();
            void enqueue_render_commands();
            void download_visibility_queries();
            void mark_cameras_as_render_complete();

            void update_openvr_transforms();
            void present_openvr_frames();
            void allocate_vulkan_resources();
            void release_vulkan_resources();
            RenderSystem();
            ~RenderSystem();            
    };
}
