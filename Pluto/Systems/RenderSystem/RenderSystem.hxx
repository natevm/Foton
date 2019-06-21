#pragma once

#include "Pluto/Tools/System.hxx"
#include "Pluto/Libraries/GLFW/GLFW.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"

#include "Pluto/Material/PushConstants.hxx"

class Entity;
class Texture;
struct VisibleEntityInfo;

namespace Systems 
{
    class RenderSystem : public System {
        public:
            static RenderSystem* Get();
            bool initialize();
            bool start();
            bool stop();
            void *socket;
            std::string ip;
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
        private:
            PushConsts push_constants;            

            bool using_openvr = false;
            bool using_vr_hidden_area_masks = true;
            void *zmq_context;
            

            // glm::vec3 top_sky_color;
            // glm::vec3 bottom_sky_color;
            // float sky_transition;

            double lastTime, currentTime;

            struct ComputeNode
            {
                std::vector<std::shared_ptr<ComputeNode>> dependencies;
                std::vector<std::shared_ptr<ComputeNode>> children;
                std::vector<std::string> connected_windows;
                std::vector<vk::Semaphore> signal_semaphores;
                std::vector<vk::Semaphore> window_signal_semaphores;
                std::vector<vk::CommandBuffer> command_buffers;
                vk::Fence fence = vk::Fence();
                uint32_t level;
                uint32_t queue_idx;
            };

            std::vector<std::vector<std::shared_ptr<ComputeNode>>> compute_graph;

            struct Bucket
            {
                int x, y, width, height;
                float data[4 * 16 * 16];
            };
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

            vk::DeviceMemory accelerationStructureScratchMemory;
            vk::Buffer accelerationStructureScratchBuffer;

            /* A handle to an RTX buffer of geometry instances used to build the top level BVH */
            vk::Buffer instanceBuffer;
            vk::DeviceMemory instanceBufferMemory;

            // std::vector<vk::Semaphore> main_command_buffer_semaphores;
            // vk::Fence main_fence;
            bool main_command_buffer_recorded = false;
            bool main_command_buffer_presenting = false;
            // bool final_renderpass_semaphore_signalled = false;

            // std::vector<vk::Fence> maincmd_fences;

            std::vector<vk::Semaphore> final_renderpass_semaphores;
            std::vector<vk::Fence> final_fences;
            uint32_t max_frames_in_flight = 2;
            // uint32_t max_renderpass_semaphore_sets = 2;

            bool update_push_constants();
            void record_render_commands();
            void record_ray_trace_pass(Entity &camera_entity, std::vector<std::vector<VisibleEntityInfo>> &visible_entities);
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
