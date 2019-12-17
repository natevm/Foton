// #pragma optimize("", off)
#include "Foton/Prefabs/Prefabs.hxx"

#include "Foton/Tools/Options.hxx"
#include "Foton/Tools/FileReader.hxx"

#define NOMINMAX
//#include <zmq.h>

#include <string>
#include <iostream>
#include <assert.h>

#include "./RenderSystem.hxx"
#include "Foton/Tools/Colors.hxx"
#include "Foton/Camera/Camera.hxx"
#include "Foton/Texture/Texture.hxx"
#include "Foton/Entity/Entity.hxx"
#include "Foton/Tools/Options.hxx"
#include "Foton/Material/Material.hxx"
#include "Foton/Light/Light.hxx"
#include "Foton/Mesh/Mesh.hxx"
#include "Foton/Transform/Transform.hxx"

#include "Foton/Libraries/OpenVR/OpenVR.hxx"

#include "Foton/Resources/Shaders/Common/GBufferLocations.hxx"
using namespace Libraries;

namespace Systems
{

RenderSystem::RenderSystem() {}
RenderSystem::~RenderSystem() {}

RenderSystem *RenderSystem::Get()
{
    static RenderSystem instance;
    return &instance;
}

bool RenderSystem::initialize()
{
    using namespace Libraries;
    if (initialized)
        return false;

    #if ZMQ_BUILD_DRAFT_API == 1
    /* Setup Server/Client */
    if (Options::IsServer() || Options::IsClient())
    {
        zmq_context = zmq_ctx_new();

        ip = "udp://";
        ip += Options::GetIP();
        ip += ":5555";
        if (Options::IsServer())
        {
            socket = zmq_socket(zmq_context, ZMQ_RADIO);
            int rc = zmq_connect(socket, ip.c_str());
            assert(rc == 0);
        }

        else if (Options::IsClient())
        {
            std::cout << "Connecting to hello world server..." << std::endl;
            socket = zmq_socket(zmq_context, ZMQ_DISH);
            int rc = zmq_bind(socket, ip.c_str());
            assert(rc == 0);

            /* Join the message group */
            zmq_join(socket, "Foton");
        }

        int64_t rate = 100000;
        zmq_setsockopt(socket, ZMQ_RATE, &rate, sizeof(int64_t));
    }
    #endif

    push_constants.top_sky_color = vec4(0.0f);
    push_constants.bottom_sky_color = vec4(0.0f);
    push_constants.sky_transition = 0.0f;
    push_constants.gamma = 2.2f;
    push_constants.exposure = 2.0f;
    push_constants.time = 0.0f;
    push_constants.frame = 0;
    push_constants.environment_roughness = 0.0f;
    push_constants.environment_intensity = 1.0f;
    push_constants.target_id = -1;
    push_constants.camera_id = -1;
    push_constants.viewIndex = -1;

    push_constants.flags = 0;
    
    #ifdef DISABLE_MULTIVIEW
    push_constants.flags |= (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW);
    #endif

    #ifdef DISABLE_REVERSE_Z
    push_constants.flags |= ( 1 << RenderSystemOptions::DISABLE_REVERSE_Z_PROJECTION );
    #endif

	// If temporal antialiasing is enabled
	push_constants.flags &= ~( 1 << RenderSystemOptions::ENABLE_TAA );

	enable_blue_noise(true);
	enable_median(true);
	enable_bilateral_upsampling(true);
	force_flat_reflectors(false);

    initialized = true;
    return true;
}

bool RenderSystem::update_push_constants()
{
    // /* Find lookup tables */
    // Texture* brdf = nullptr;
    // Texture* ltc_mat = nullptr;
    // Texture* ltc_amp = nullptr;

	// // todo: remove
    // Texture* sobel = nullptr;
    // Texture* ranking = nullptr;
    // Texture* scramble = nullptr;

    // Texture* ddgi_irradiance = nullptr;
    // Texture* ddgi_visibility = nullptr;

    // try {
    //     brdf = Texture::Get("BRDF");
    //     ltc_mat = Texture::Get("LTCMAT");
    //     ltc_amp = Texture::Get("LTCAMP");
    //     ranking = Texture::Get("RANKINGTILE");
    //     scramble = Texture::Get("SCRAMBLETILE");
    //     sobel = Texture::Get("SOBELTILE");

	// 	ddgi_irradiance = Texture::Get("DDGI_IRRADIANCE");
	// 	ddgi_visibility = Texture::Get("DDGI_VISIBILITY");
    // } catch (...) {}
    // if ((!brdf) || (!ltc_mat) || (!ltc_amp) || (!sobel) || (!ranking) || (!scramble)) return false;

    /* Update some push constants */
    // auto brdf_id = brdf->get_id();
    // auto ltc_mat_id = ltc_mat->get_id();
    // auto ltc_amp_id = ltc_amp->get_id();
    // push_constants.brdf_lut_id = brdf_id;
    // push_constants.ltc_mat_lut_id = ltc_mat_id;
    // push_constants.ltc_amp_lut_id = ltc_amp_id;
    // push_constants.sobel_tile_id = sobel->get_id();
    // push_constants.ranking_tile_id = ranking->get_id();
    // push_constants.scramble_tile_id = scramble->get_id();
    push_constants.time = (float) glfwGetTime();
	push_constants.num_lights = Light::GetNumActiveLights();
    push_constants.frame++;

	if (environment_id == -1) {
		push_constants.flags |= (1 << RenderSystemOptions::USE_PROCEDURAL_ENVIRONMENT);
	} else {
		push_constants.flags &= ~(1 << RenderSystemOptions::USE_PROCEDURAL_ENVIRONMENT);
	}

	// if ((!progressive_refinement_enabled) && (push_constants.frame > 1024))
		// push_constants.frame = 0;
    return true;
}

void RenderSystem::download_visibility_queries()
{
    auto cameras = Camera::GetFront();
    for (uint32_t i = 0; i < Camera::GetCount(); ++i ) {
        if (!cameras[i].is_initialized()) continue;
		download_query_pool_results(i);
    }
}

uint32_t shadow_idx = 0;

void RenderSystem::record_raster_primary_visibility_renderpass(Entity &camera_entity, std::vector<std::vector<VisibleEntityInfo>> &visible_entities)
{
	auto camera = camera_entity.get_camera();
    if (!camera) throw std::runtime_error("Error, camera was null during recording of raster primary visibility renderpass");
	auto &cam_res = camera_resources[camera->get_id()];
	Texture * texture = camera->get_texture();
    vk::CommandBuffer command_buffer = cam_res.command_buffer;

	uint32_t num_renderpasses = (uint32_t)cam_res.primary_visibility_depth_prepasses.size();

    /* Record Z prepasses / Occlusion query  */
	reset_query_pool(command_buffer, camera->get_id());
    if (camera->should_record_depth_prepass()) {
        for(uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
            reset_bound_material();
            
            /* Get the renderpass for the current camera */
            vk::RenderPass rp = cam_res.primary_visibility_depth_prepasses[rp_idx];
			vk::Framebuffer fb = cam_res.primary_visibility_depth_prepass_framebuffers[rp_idx];

            /* Bind all descriptor sets to that renderpass.
                Note that we're using a single bind. The same descriptors are shared across pipelines. */
            bind_depth_prepass_descriptor_sets(command_buffer, rp);
			begin_depth_prepass(command_buffer, rp, fb, camera->get_id(), USED_PRIMARY_VISIBILITY_G_BUFFERS);

            if (using_openvr && using_vr_hidden_area_masks)
            {
                auto ovr = Libraries::OpenVR::Get();

                /* Render visibility masks */
                if (camera == ovr->get_connected_camera()) {
                    Entity* mask_entity;
                    if (rp_idx == 0) mask_entity = ovr->get_left_eye_hidden_area_entity();
                    else mask_entity = ovr->get_right_eye_hidden_area_entity();
                    // Push constants
                    push_constants.target_id = mask_entity->get_id();
                    push_constants.camera_id = camera_entity.get_id();
                    push_constants.viewIndex = rp_idx;
                    push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
                    draw_entity(command_buffer, rp, *mask_entity, push_constants, RenderMode::RENDER_MODE_VRMASK);
                }
            }

            for (uint32_t i = 0; i < visible_entities[rp_idx].size(); ++i) {
                if (visible_entities[rp_idx][i].visible && visible_entities[rp_idx][i].distance < camera->get_max_visible_distance()) {
                	auto target_id = visible_entities[rp_idx][i].entity->get_id();
					begin_visibility_query(command_buffer, camera->get_id(), rp_idx, visible_entities[rp_idx][i].entity_index);
                    // Push constants
                    push_constants.target_id = target_id;
                    push_constants.camera_id = camera_entity.get_id();
                    push_constants.viewIndex = rp_idx;
                    push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
                    
                    bool was_visible_last_frame = is_entity_visible(camera->get_id(), rp_idx, target_id);
                    bool contains_transparency = visible_entities[rp_idx][i].entity->get_material()->contains_transparency();
                    bool disable_depth_write = (was_visible_last_frame) ? contains_transparency : true;
                    
                    bool show_bounding_box = !was_visible_last_frame;

                    PipelineType pipeline_type = PipelineType::PIPELINE_TYPE_NORMAL;
                    if (show_bounding_box || contains_transparency)
                        pipeline_type = PipelineType::PIPELINE_TYPE_DEPTH_WRITE_DISABLED;

                    draw_entity(command_buffer, rp, *visible_entities[rp_idx][i].entity, push_constants, 
                        RenderMode::RENDER_MODE_FRAGMENTDEPTH, pipeline_type, show_bounding_box);
					end_visibility_query(command_buffer, camera->get_id(), rp_idx, visible_entities[rp_idx][i].entity_index);
                }
            }
			end_depth_prepass(command_buffer, camera->get_id());
        }
    }

	/* Record GBuffer passes */
    for(uint32_t rp_idx = 0; rp_idx < cam_res.primary_visibility_renderpasses.size(); rp_idx++) {
        reset_bound_material();

        /* Get the renderpass for the current camera */
        vk::RenderPass rp = cam_res.primary_visibility_renderpasses[rp_idx];
        vk::Framebuffer fb = cam_res.primary_visibility_renderpass_framebuffers[rp_idx];

        /* Bind all descriptor sets to that renderpass.
            Note that we're using a single bind. The same descriptors are shared across pipelines. */
        bind_raster_primary_visibility_descriptor_sets(command_buffer, rp);
        
        begin_renderpass(command_buffer, rp, fb, camera->get_id(), USED_PRIMARY_VISIBILITY_G_BUFFERS);
		push_constants.width = texture->get_width();
        push_constants.height = texture->get_height();
		push_constants.camera_id = camera_entity.get_id();
		push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 

        /* Render visibility masks */
        if (using_openvr && using_vr_hidden_area_masks && !(camera->should_record_depth_prepass()))
        {
            auto ovr = Libraries::OpenVR::Get();

            if (camera == ovr->get_connected_camera()) {
                Entity* mask_entity;
                if (rp_idx == 0) mask_entity = ovr->get_left_eye_hidden_area_entity();
                else mask_entity = ovr->get_right_eye_hidden_area_entity();
                // Push constants
                push_constants.target_id = mask_entity->get_id();
                push_constants.viewIndex = rp_idx;
                draw_entity(command_buffer, rp, *mask_entity, push_constants, RenderMode::RENDER_MODE_VRMASK);
            }
        }

        /* Render all opaque objects */
        for (uint32_t i = 0; i < visible_entities[rp_idx].size(); ++i) {
            /* An object must be opaque, have a transform, a mesh, and a material to be rendered. */
            if (!visible_entities[rp_idx][i].visible || visible_entities[rp_idx][i].distance > camera->get_max_visible_distance()) continue;
            if (visible_entities[rp_idx][i].entity->get_material()->contains_transparency()) continue;

            auto target_id = visible_entities[rp_idx][i].entity_index;
            if (is_entity_visible(camera->get_id(), rp_idx, target_id) || (!camera->should_record_depth_prepass())) {
                push_constants.target_id = target_id;
                push_constants.viewIndex = rp_idx;
				draw_entity(command_buffer, rp, *visible_entities[rp_idx][i].entity, push_constants, camera->get_rendermode_override());
            }
        }
        
        /* Draw transparent objects last */
        for (int32_t i = (int32_t)visible_entities[rp_idx].size() - 1; i >= 0; --i)
        {
            if (!visible_entities[rp_idx][i].visible || visible_entities[rp_idx][i].distance > camera->get_max_visible_distance()) continue;
            if (!visible_entities[rp_idx][i].entity->get_material()->contains_transparency()) continue;

            auto target_id = visible_entities[rp_idx][i].entity_index;
            if (is_entity_visible(camera->get_id(), rp_idx, target_id) || (!camera->should_record_depth_prepass())) {
                push_constants.target_id = target_id;
                push_constants.viewIndex = rp_idx;
                draw_entity(command_buffer, rp, *visible_entities[rp_idx][i].entity, push_constants, 
                    camera->get_rendermode_override(), PipelineType::PIPELINE_TYPE_DEPTH_TEST_GREATER);
            }
        }
		end_renderpass(command_buffer, camera->get_id());
    }
}

void RenderSystem::record_shadow_map_renderpass(Entity &camera_entity, std::vector<std::vector<VisibleEntityInfo>> &visible_entities)
{
	auto camera = camera_entity.get_camera();
    if (!camera) throw std::runtime_error("Error, camera was null during recording of raster primary visibility renderpass");
	auto &cam_res = camera_resources[camera->get_id()];
	Texture * texture = camera->get_texture();
	uint32_t num_renderpasses = (uint32_t)cam_res.primary_visibility_depth_prepasses.size();
    vk::CommandBuffer command_buffer = cam_res.command_buffer;

    /* Record Z prepasses / Occlusion query  */
	reset_query_pool(command_buffer, camera->get_id());

    if (camera->should_record_depth_prepass()) {
        for(uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
            reset_bound_material();
            
            /* Get the renderpass for the current camera */
            vk::RenderPass rp = cam_res.shadow_map_depth_prepasses[rp_idx];
            vk::Framebuffer fb = cam_res.shadow_map_depth_prepass_framebuffers[rp_idx];

            /* Bind all descriptor sets to that renderpass.
                Note that we're using a single bind. The same descriptors are shared across pipelines. */
            bind_depth_prepass_descriptor_sets(command_buffer, rp);

			begin_depth_prepass(command_buffer, rp, fb, camera->get_id(), USED_SHADOW_MAP_G_BUFFERS);

            if (using_openvr && using_vr_hidden_area_masks)
            {
                auto ovr = Libraries::OpenVR::Get();

                /* Render visibility masks */
                if (camera == ovr->get_connected_camera()) {
                    Entity* mask_entity;
                    if (rp_idx == 0) mask_entity = ovr->get_left_eye_hidden_area_entity();
                    else mask_entity = ovr->get_right_eye_hidden_area_entity();
                    // Push constants
                    push_constants.target_id = mask_entity->get_id();
                    push_constants.camera_id = camera_entity.get_id();
                    push_constants.viewIndex = rp_idx;
                    push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
                    draw_entity(command_buffer, rp, *mask_entity, push_constants, RenderMode::RENDER_MODE_VRMASK);
                }
            }

            for (uint32_t i = 0; i < visible_entities[rp_idx].size(); ++i) {
                if (visible_entities[rp_idx][i].visible && visible_entities[rp_idx][i].distance < camera->get_max_visible_distance()) {
                	auto target_id = visible_entities[rp_idx][i].entity->get_id();
					begin_visibility_query(command_buffer, camera->get_id(), rp_idx, visible_entities[rp_idx][i].entity_index);
                    // Push constants
                    push_constants.target_id = target_id;
                    push_constants.camera_id = camera_entity.get_id();
                    push_constants.viewIndex = rp_idx;
                    push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
                    
                    bool was_visible_last_frame = is_entity_visible(camera->get_id(), rp_idx, target_id);
                    bool contains_transparency = visible_entities[rp_idx][i].entity->get_material()->contains_transparency();
                    bool disable_depth_write = (was_visible_last_frame) ? contains_transparency : true;
                    
                    bool show_bounding_box = !was_visible_last_frame;

                    PipelineType pipeline_type = PipelineType::PIPELINE_TYPE_NORMAL;
                    if (show_bounding_box || contains_transparency)
                        pipeline_type = PipelineType::PIPELINE_TYPE_DEPTH_WRITE_DISABLED;

                    draw_entity(command_buffer, rp, *visible_entities[rp_idx][i].entity, push_constants, 
                        RenderMode::RENDER_MODE_FRAGMENTDEPTH, pipeline_type, show_bounding_box);
					end_visibility_query(command_buffer, camera->get_id(), rp_idx, visible_entities[rp_idx][i].entity_index);
				}
            }
			end_depth_prepass(command_buffer, camera->get_id());
        }
    }

	/* Record GBuffer passes */
    for(uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
        reset_bound_material();

        /* Get the renderpass for the current camera */
        vk::RenderPass rp = cam_res.shadow_map_renderpasses[rp_idx];
        vk::Framebuffer fb = cam_res.shadow_map_renderpass_framebuffers[rp_idx];

        /* Bind all descriptor sets to that renderpass.
            Note that we're using a single bind. The same descriptors are shared across pipelines. */
        bind_shadow_map_descriptor_sets(command_buffer, rp);
        
        begin_renderpass(command_buffer, rp, fb, camera->get_id(), USED_SHADOW_MAP_G_BUFFERS);
		push_constants.width = texture->get_width();
        push_constants.height = texture->get_height();
		push_constants.camera_id = camera_entity.get_id();
		push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 

        /* Render visibility masks */
        if (using_openvr && using_vr_hidden_area_masks && !(camera->should_record_depth_prepass()))
        {
            auto ovr = Libraries::OpenVR::Get();

            if (camera == ovr->get_connected_camera()) {
                Entity* mask_entity;
                if (rp_idx == 0) mask_entity = ovr->get_left_eye_hidden_area_entity();
                else mask_entity = ovr->get_right_eye_hidden_area_entity();
                // Push constants
                push_constants.target_id = mask_entity->get_id();
                push_constants.viewIndex = rp_idx;
                draw_entity(command_buffer, rp, *mask_entity, push_constants, RenderMode::RENDER_MODE_VRMASK);
            }
        }

        /* Render all opaque objects */
        for (uint32_t i = 0; i < visible_entities[rp_idx].size(); ++i) {
            /* An object must be opaque, have a transform, a mesh, and a material to be rendered. */
            if (!visible_entities[rp_idx][i].visible || visible_entities[rp_idx][i].distance > camera->get_max_visible_distance()) continue;
            if (visible_entities[rp_idx][i].entity->get_material()->contains_transparency()) continue;

            auto target_id = visible_entities[rp_idx][i].entity_index;
            if (is_entity_visible(camera->get_id(), rp_idx, target_id) || (!camera->should_record_depth_prepass())) {
                push_constants.target_id = target_id;
                push_constants.viewIndex = rp_idx;
				draw_entity(command_buffer, rp, *visible_entities[rp_idx][i].entity, push_constants, camera->get_rendermode_override());
            }
        }
        
        /* Draw transparent objects last */
        for (int32_t i = (int32_t)visible_entities[rp_idx].size() - 1; i >= 0; --i)
        {
            if (!visible_entities[rp_idx][i].visible || visible_entities[rp_idx][i].distance > camera->get_max_visible_distance()) continue;
            if (!visible_entities[rp_idx][i].entity->get_material()->contains_transparency()) continue;

            auto target_id = visible_entities[rp_idx][i].entity_index;
            if (is_entity_visible(camera->get_id(), rp_idx, target_id) || (!camera->should_record_depth_prepass())) {
                push_constants.target_id = target_id;
                push_constants.viewIndex = rp_idx;
                draw_entity(command_buffer, rp, *visible_entities[rp_idx][i].entity, push_constants, 
                    camera->get_rendermode_override(), PipelineType::PIPELINE_TYPE_DEPTH_TEST_GREATER);
            }
        }
		end_renderpass(command_buffer, camera->get_id());
    }
}

void RenderSystem::record_ray_trace_pass(Entity &camera_entity)
{
    auto vulkan = Vulkan::Get();
    auto device = vulkan->get_device();
    auto dldi = vulkan->get_dldi();

    if (!vulkan->is_ray_tracing_enabled()) return;

	auto rayTracingProps = vulkan->get_physical_device_ray_tracing_properties();

    auto camera = camera_entity.get_camera();
    if (!camera) throw std::runtime_error("Error, camera was null in recording ray trace");
	auto &cam_res = camera_resources[camera->get_id()];
    vk::CommandBuffer command_buffer = cam_res.command_buffer;
    Texture * texture = camera->get_texture();

    /* Camera needs a texture */
    if (!texture) return;
    
    if (!Material::IsInitialized()) return;

    texture->make_general(command_buffer);

	/* Primary visibility */
	if (!rasterize_primary_visibility_enabled) {
		for(uint32_t v_idx = 0; v_idx < camera->maxViews; v_idx++) {
			reset_bound_material();

			/* Get the renderpass for the current camera */
			// vk::RenderPass rp = cam_res.renderpasses[v_idx];
			
			bind_raytracing_descriptor_sets(command_buffer, camera_entity, v_idx);

			push_constants.target_id = -1;
			push_constants.camera_id = camera_entity.get_id();
			push_constants.viewIndex = v_idx;
			push_constants.width = texture->get_width();
			push_constants.height = texture->get_height();
			push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
			push_constants.parameter1 = (float)this->max_bounces;
			
			command_buffer.pushConstants(raytrace_primary_visibility.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
			command_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingNV, raytrace_primary_visibility.pipeline);

			command_buffer.traceRaysNV(
				raytrace_primary_visibility.shaderBindingTable, 0,
				raytrace_primary_visibility.shaderBindingTable, 1 * rayTracingProps.shaderGroupHandleSize, rayTracingProps.shaderGroupHandleSize,
				raytrace_primary_visibility.shaderBindingTable, 2 * rayTracingProps.shaderGroupHandleSize, rayTracingProps.shaderGroupHandleSize,
				vk::Buffer(), 0, 0,
				texture->get_width(), texture->get_height(), 1,
				dldi
			);	
		}
	}
}

void RenderSystem::record_pre_compute_pass(Entity &camera_entity)
{
	/* Could use this in the future, although would require transitioning
	each gbuffer from general to attachment optimal. */
	// if (!taa_enabled) return;
	auto vulkan = Vulkan::Get();
    auto device = vulkan->get_device();
    auto dldi = vulkan->get_dldi();

    auto camera = camera_entity.get_camera();
    if (!camera) throw std::runtime_error("Error, camera was null in recording compute pass");
	auto &cam_res = camera_resources[camera->get_id()];
    vk::CommandBuffer command_buffer = cam_res.command_buffer;
	uint32_t num_renderpasses = camera->maxViews;
    Texture * texture = camera->get_texture();

    /* Camera needs a texture */
    if (!texture) return;
	uint32_t width = texture->get_width();
	uint32_t height = texture->get_height();
	push_constants.width = texture->get_width();
	push_constants.height = texture->get_height();

	bool shadow_caster = false;
	if (camera_entity.get_light() && camera_entity.get_light()->should_cast_shadows())
		shadow_caster = true;

    texture->make_general(command_buffer);

	if (!shadow_caster) 
	{
		/* SVGF DENOISER PATH */
		if (ray_tracing_enabled) 
		{
		
		}
	}

	if (shadow_caster)
	{
		
	}
}

void RenderSystem::record_post_compute_pass(Entity &camera_entity)
{
	// if (!taa_enabled) return;
	auto vulkan = Vulkan::Get();
    auto device = vulkan->get_device();
    auto dldi = vulkan->get_dldi();

    auto camera = camera_entity.get_camera();
    if (!camera) throw std::runtime_error("Error, camera was null in recording compute pass");
	auto &cam_res = camera_resources[camera->get_id()];
    vk::CommandBuffer command_buffer = cam_res.command_buffer;
	uint32_t num_renderpasses = camera->maxViews;
    Texture * texture = camera->get_texture();

    /* Camera needs a texture */
    if (!texture) return;
	uint32_t width = texture->get_width();
	uint32_t height = texture->get_height();
	push_constants.width = texture->get_width();
	push_constants.height = texture->get_height();

	bool shadow_caster = false;
	if (camera_entity.get_light() && camera_entity.get_light()->should_cast_shadows())
		shadow_caster = true;

    texture->make_general(command_buffer);

	if (!shadow_caster) 
	{
		/* SVGF DENOISER PATH */
		if (ray_tracing_enabled) 
		{
			auto rayTracingProps = vulkan->get_physical_device_ray_tracing_properties();
			
			/* Diffuse path tracer */
			for(uint32_t v_idx = 0; v_idx < camera->maxViews; v_idx++) {
				reset_bound_material();        
				bind_raytracing_descriptor_sets(command_buffer, camera_entity, v_idx);

				push_constants.target_id = -1;
				push_constants.camera_id = camera_entity.get_id();
				push_constants.viewIndex = v_idx;
				push_constants.width = texture->get_width();
				push_constants.height = texture->get_height();
				push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
				push_constants.parameter1 = (float)this->max_bounces;
				push_constants.parameter2 = this->test_param;
				
				command_buffer.pushConstants(diffuse_path_tracer.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
				command_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingNV, diffuse_path_tracer.pipeline);

				command_buffer.traceRaysNV(
					diffuse_path_tracer.shaderBindingTable, 0,
					diffuse_path_tracer.shaderBindingTable, 1 * rayTracingProps.shaderGroupHandleSize, rayTracingProps.shaderGroupHandleSize,
					diffuse_path_tracer.shaderBindingTable, 2 * rayTracingProps.shaderGroupHandleSize, rayTracingProps.shaderGroupHandleSize,
					vk::Buffer(), 0, 0,
					texture->get_width(), texture->get_height(), 1,
					dldi
				);	
			}

			/* Specular path tracer */
			for(uint32_t v_idx = 0; v_idx < camera->maxViews; v_idx++) {
				reset_bound_material();        
				bind_raytracing_descriptor_sets(command_buffer, camera_entity, v_idx);

				push_constants.target_id = -1;
				push_constants.camera_id = camera_entity.get_id();
				push_constants.viewIndex = v_idx;
				push_constants.width = texture->get_width();
				push_constants.height = texture->get_height();
				push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
				push_constants.parameter1 = (float)this->max_bounces;
				push_constants.parameter2 = this->test_param;
				
				command_buffer.pushConstants(specular_path_tracer.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
				command_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingNV, specular_path_tracer.pipeline);

				command_buffer.traceRaysNV(
					specular_path_tracer.shaderBindingTable, 0,
					specular_path_tracer.shaderBindingTable, 1 * rayTracingProps.shaderGroupHandleSize, rayTracingProps.shaderGroupHandleSize,
					specular_path_tracer.shaderBindingTable, 2 * rayTracingProps.shaderGroupHandleSize, rayTracingProps.shaderGroupHandleSize,
					vk::Buffer(), 0, 0,
					texture->get_width(), texture->get_height(), 1,
					dldi
				);	
			}

			/* Compute Sparse Gradient */
			if (asvgf_enabled && asvgf_gradient_enabled)
			{
				for(uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
					push_constants.target_id = -1;
					push_constants.camera_id = camera_entity.get_id();
					push_constants.viewIndex = rp_idx;
					push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
					bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);
					
					command_buffer.pushConstants(asvgf_compute_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, asvgf_compute_gradient.pipeline);

					uint32_t local_size_x = 16;
					uint32_t local_size_y = 16;

					uint32_t groupX = (width + local_size_x - 1) / local_size_x;
					uint32_t groupY = (height + local_size_y - 1) / local_size_y;
					uint32_t groupZ = 1;
					command_buffer.dispatch(groupX, groupY, groupZ);
				}
			}

			// if (enable_median_filter) {
			/* median filter the temporal gradient */
			// if (asvgf_enabled && asvgf_gradient_enabled)
			// {
			// 	for(uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
			// 		push_constants.target_id = -1;
			// 		push_constants.camera_id = camera_entity.get_id();
			// 		push_constants.viewIndex = rp_idx;
			// 		push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
			// 		bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);

			// 		uint32_t local_size_x = 16;
			// 		uint32_t local_size_y = 16;

			// 		uint32_t groupX = (width + local_size_x - 1) / local_size_x;
			// 		uint32_t groupY = (height + local_size_y - 1) / local_size_y;
			// 		uint32_t groupZ = 1;

			// 		command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, median_3x3.pipeline);
					
			// 		push_constants.parameter1 = TEMPORAL_GRADIENT_ADDR;
			// 		push_constants.parameter2 = ATROUS_HISTORY_1_ADDR;
			// 		push_constants.iteration = 0;
			// 		command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
			// 		command_buffer.dispatch(groupX, groupY, groupZ);
			// 		push_constants.iteration = 1;
			// 		command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
			// 		command_buffer.dispatch(groupX, groupY, groupZ);

			// 		push_constants.parameter1 = LUMINANCE_MAX_ADDR;
			// 		push_constants.parameter2 = ATROUS_HISTORY_1_ADDR;
			// 		push_constants.iteration = 0;
			// 		command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
			// 		command_buffer.dispatch(groupX, groupY, groupZ);
			// 		push_constants.iteration = 1;
			// 		command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
			// 		command_buffer.dispatch(groupX, groupY, groupZ);
			// 	}
			// }
			// }

			/* A-SVGF reconstruct gradient pass */
			if (asvgf_enabled && asvgf_gradient_enabled)
			{
				for(uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
					push_constants.target_id = -1;
					push_constants.camera_id = camera_entity.get_id();
					push_constants.viewIndex = rp_idx;
					push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
					bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);

					uint32_t local_size_x = 16;
					uint32_t local_size_y = 16;

					uint32_t groupX = (width + local_size_x - 1) / local_size_x;
					uint32_t groupY = (height + local_size_y - 1) / local_size_y;
					uint32_t groupZ = 1;

					command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, asvgf_reconstruct_gradient.pipeline);
					
					push_constants.parameter1 = asvgf_gradient_reconstruction_sigma;
					for (int i = (2 * asvgf_gradient_reconstruction_iterations) - 1; i >= 0; i--) {
						push_constants.iteration = i;
						command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
						command_buffer.dispatch(groupX, groupY, groupZ);
					}
				}
			}

			/* A-SVGF Temporal Accumulation Pass */
			if (asvgf_enabled && asvgf_temporal_accumulation_enabled)
			{
				for(uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
					push_constants.target_id = -1;
					push_constants.camera_id = camera_entity.get_id();
					push_constants.viewIndex = rp_idx;
					push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
					bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);
					
					uint32_t local_size_x = 16;
					uint32_t local_size_y = 16;

					uint32_t groupX = (width + local_size_x - 1) / local_size_x;
					uint32_t groupY = (height + local_size_y - 1) / local_size_y;
					uint32_t groupZ = 1;
					
					command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, asvgf_diffuse_temporal_accumulation.pipeline);
					push_constants.parameter1 = this->asvgf_diffuse_gradient_influence;
					push_constants.parameter2 = this->force_nearest_temporal_sampling_on;
					push_constants.iteration = 0;
					command_buffer.pushConstants(asvgf_diffuse_temporal_accumulation.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);
					push_constants.iteration = 1;
					command_buffer.pushConstants(asvgf_diffuse_temporal_accumulation.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);
				}

				for(uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
					push_constants.target_id = -1;
					push_constants.camera_id = camera_entity.get_id();
					push_constants.viewIndex = rp_idx;
					push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
					push_constants.parameter1 = this->asvgf_specular_gradient_influence;
					push_constants.parameter2 = this->force_nearest_temporal_sampling_on;
					bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);
					
					uint32_t local_size_x = 16;
					uint32_t local_size_y = 16;

					uint32_t groupX = (width + local_size_x - 1) / local_size_x;
					uint32_t groupY = (height + local_size_y - 1) / local_size_y;
					uint32_t groupZ = 1;

					command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, asvgf_specular_temporal_accumulation.pipeline);
					push_constants.iteration = 0;
					command_buffer.pushConstants(asvgf_specular_temporal_accumulation.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);
					push_constants.iteration = 1;
					command_buffer.pushConstants(asvgf_specular_temporal_accumulation.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);
				}
			}

			/* Fill disocclusions */
			if (asvgf_enabled && asvgf_variance_estimation_enabled)
			{
				for(uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
					push_constants.target_id = -1;
					push_constants.camera_id = camera_entity.get_id();
					push_constants.viewIndex = rp_idx;
					push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
					bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);
					
					uint32_t local_size_x = 16;
					uint32_t local_size_y = 16;

					uint32_t groupX = (width + local_size_x - 1) / local_size_x;
					uint32_t groupY = (height + local_size_y - 1) / local_size_y;
					uint32_t groupZ = 1;

					command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, asvgf_fill_diffuse_holes.pipeline);
					push_constants.iteration = 0;
					command_buffer.pushConstants(asvgf_fill_diffuse_holes.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);
					push_constants.iteration = 1;
					command_buffer.pushConstants(asvgf_fill_diffuse_holes.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);

					command_buffer.pushConstants(asvgf_fill_specular_holes.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, asvgf_fill_specular_holes.pipeline);
					command_buffer.dispatch(groupX, groupY, groupZ);
				}
			}

			/* Firefly supression */
			if (enable_median_filter) {
				for(uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
					push_constants.target_id = -1;
					push_constants.camera_id = camera_entity.get_id();
					push_constants.viewIndex = rp_idx;
					push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
					bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);

					uint32_t local_size_x = 16;
					uint32_t local_size_y = 16;

					uint32_t groupX = (width + local_size_x - 1) / local_size_x;
					uint32_t groupY = (height + local_size_y - 1) / local_size_y;
					uint32_t groupZ = 1;

					command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, median_3x3.pipeline);
					
					push_constants.parameter1 = DIFFUSE_INDIRECT_ADDR;
					push_constants.parameter2 = ATROUS_HISTORY_1_ADDR;
					push_constants.iteration = 0;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);
					push_constants.iteration = 1;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);

					command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, median_3x3.pipeline);

					push_constants.parameter1 = GLOSSY_INDIRECT_ADDR;
					push_constants.parameter2 = ATROUS_HISTORY_1_ADDR;
					push_constants.iteration = 0;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);
					push_constants.iteration = 1;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);
				}
			}

			/* Reconstruct Diffuse and Glossy */
			if (asvgf_enabled && asvgf_atrous_enabled)
			{
				for(uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
					push_constants.target_id = -1;
					push_constants.camera_id = camera_entity.get_id();
					push_constants.viewIndex = rp_idx;
					push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
					bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);

					uint32_t local_size_x = 16;
					uint32_t local_size_y = 16;

					uint32_t groupX = (width + local_size_x - 1) / local_size_x;
					uint32_t groupY = (height + local_size_y - 1) / local_size_y;
					uint32_t groupZ = 1;

					command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, reconstruct_diffuse.pipeline);
					
					push_constants.parameter1 = asvgf_direct_diffuse_blur;
					push_constants.parameter2 = asvgf_indirect_diffuse_blur;
					for (int i = (2 * asvgf_atrous_iterations) - 1; i >= 0; i--) {
					// for (int i = 0; i < (2 * asvgf_atrous_iterations); i++) {
						push_constants.iteration = i;
						command_buffer.pushConstants(reconstruct_diffuse.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
						command_buffer.dispatch(groupX, groupY, groupZ);
					}
				}

				for(uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
					push_constants.target_id = -1;
					push_constants.camera_id = camera_entity.get_id();
					push_constants.viewIndex = rp_idx;
					push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
					bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);

					uint32_t local_size_x = 16;
					uint32_t local_size_y = 16;

					uint32_t groupX = (width + local_size_x - 1) / local_size_x;
					uint32_t groupY = (height + local_size_y - 1) / local_size_y;
					uint32_t groupZ = 1;

					command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, reconstruct_glossy.pipeline);
					
					push_constants.parameter1 = asvgf_direct_glossy_blur;
					push_constants.parameter2 = asvgf_indirect_glossy_blur;
					for (int i = (2 * asvgf_atrous_iterations) - 1; i >= 0; i--) {
					// for (int i = 0; i < (2 * asvgf_atrous_iterations); i++) {
						push_constants.iteration = i;
						command_buffer.pushConstants(reconstruct_glossy.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
						command_buffer.dispatch(groupX, groupY, groupZ);
					}
				}
			}

			/* Bilateral Upsampling */
			if (enable_bilateral_upsampling_filter) {
				for(uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
					push_constants.target_id = -1;
					push_constants.camera_id = camera_entity.get_id();
					push_constants.viewIndex = rp_idx;
					push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
					bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);

					uint32_t local_size_x = 16;
					uint32_t local_size_y = 16;

					uint32_t groupX = (width + local_size_x - 1) / local_size_x;
					uint32_t groupY = (height + local_size_y - 1) / local_size_y;
					uint32_t groupZ = 1;

					command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, bilateral_upsample.pipeline);
					
					push_constants.parameter3 = 3;
					
					push_constants.parameter1 = DIFFUSE_DIRECT_ADDR;
					push_constants.parameter2 = ATROUS_HISTORY_1_ADDR;
					push_constants.iteration = 0;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);
					push_constants.iteration = 1;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);

					push_constants.parameter1 = DIFFUSE_INDIRECT_ADDR;
					push_constants.parameter2 = ATROUS_HISTORY_1_ADDR;
					push_constants.iteration = 0;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);
					push_constants.iteration = 1;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);

					// push_constants.parameter3 = 3;
					push_constants.parameter1 = GLOSSY_DIRECT_ADDR;
					push_constants.parameter2 = ATROUS_HISTORY_1_ADDR;
					push_constants.iteration = 0;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);
					push_constants.iteration = 1;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);

					push_constants.parameter1 = GLOSSY_INDIRECT_ADDR;
					push_constants.parameter2 = ATROUS_HISTORY_1_ADDR;
					push_constants.iteration = 0;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);
					push_constants.iteration = 1;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);

					// push_constants.parameter3 = 3;

					push_constants.parameter1 = EMISSION_ADDR;
					push_constants.parameter2 = ATROUS_HISTORY_1_ADDR;
					push_constants.iteration = 0;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);
					push_constants.iteration = 1;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);

					push_constants.parameter1 = ENVIRONMENT_ADDR;
					push_constants.parameter2 = ATROUS_HISTORY_1_ADDR;
					push_constants.iteration = 0;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);
					push_constants.iteration = 1;
					command_buffer.pushConstants(asvgf_reconstruct_gradient.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.dispatch(groupX, groupY, groupZ);
				}
			}

			/* SVGF Remodulate */
			for (uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
				push_constants.target_id = -1;
				push_constants.camera_id = camera_entity.get_id();
				push_constants.viewIndex = rp_idx;
				push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
				bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);


				command_buffer.pushConstants(svgf_remodulate.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
				command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, svgf_remodulate.pipeline);

				uint32_t local_size_x = 16;
				uint32_t local_size_y = 16;

				uint32_t groupX = (width + local_size_x - 1) / local_size_x;
				uint32_t groupY = (height + local_size_y - 1) / local_size_y;
				uint32_t groupZ = 1;
				command_buffer.dispatch(groupX, groupY, groupZ);
			}
		}

		/* RASTER DEFERRED PATH */
		else 
		{
			/* Final Deferred Program */
			for (uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
				push_constants.target_id = -1;
				push_constants.camera_id = camera_entity.get_id();
				push_constants.viewIndex = rp_idx;
				push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
				bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);

				command_buffer.pushConstants(deferred_final.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
				command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, deferred_final.pipeline);

				uint32_t local_size_x = 16;
				uint32_t local_size_y = 16;

				uint32_t groupX = (width + local_size_x - 1) / local_size_x;
				uint32_t groupY = (height + local_size_y - 1) / local_size_y;
				uint32_t groupZ = 1;
				command_buffer.dispatch(groupX, groupY, groupZ);
			}
		}

		/* Compositing pass */
		for (uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
			push_constants.target_id = -1;
			push_constants.camera_id = camera_entity.get_id();
			push_constants.viewIndex = rp_idx;
			push_constants.flags = (!camera->should_use_multiview()) ? 
				(push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)): 
				(push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
			push_constants.parameter1 = (float)gbuffer_override_idx;
			bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);


			command_buffer.pushConstants(compositing.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, compositing.pipeline);

			uint32_t local_size_x = 16;
			uint32_t local_size_y = 16;

			uint32_t groupX = (width + local_size_x - 1) / local_size_x;
			uint32_t groupY = (height + local_size_y - 1) / local_size_y;
			uint32_t groupZ = 1;
			command_buffer.dispatch(groupX, groupY, groupZ);
		}

		/* Copy History pass */
		for (uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
			push_constants.target_id = -1;
			push_constants.camera_id = camera_entity.get_id();
			push_constants.viewIndex = rp_idx;
			push_constants.flags = (!camera->should_use_multiview()) ? 
				(push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)): 
				(push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
			push_constants.parameter1 = (float)gbuffer_override_idx;
			bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);


			command_buffer.pushConstants(copy_history.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, copy_history.pipeline);

			uint32_t local_size_x = 16;
			uint32_t local_size_y = 16;

			uint32_t groupX = (width + local_size_x - 1) / local_size_x;
			uint32_t groupY = (height + local_size_y - 1) / local_size_y;
			uint32_t groupZ = 1;
			command_buffer.dispatch(groupX, groupY, groupZ);
		}
	
		/* TAA/Progresssive Refinement */
		if ((progressive_refinement_enabled || taa_enabled) )
		{
			for(uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
				push_constants.target_id = -1;
				push_constants.camera_id = camera_entity.get_id();
				push_constants.viewIndex = rp_idx;
				push_constants.flags = (!camera->should_use_multiview()) ? (push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)) : (push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_MULTIVIEW)); 
				bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);

				if (progressive_refinement_enabled) {
					command_buffer.pushConstants(progressive_refinement.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, progressive_refinement.pipeline);
				}
				else if (taa_enabled) {
					command_buffer.pushConstants(taa.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
					command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, taa.pipeline);
				}
				uint32_t local_size_x = 16;
				uint32_t local_size_y = 16;

				uint32_t groupX = (width + local_size_x - 1) / local_size_x;
				uint32_t groupY = (height + local_size_y - 1) / local_size_y;
				uint32_t groupZ = 1;
				command_buffer.dispatch(groupX, groupY, groupZ);
			}
		}
	}

	if (shadow_caster)
	{
		/* gaussian blur (for VSM) */
		if (camera_entity.get_light() && camera_entity.get_light()->should_cast_shadows() && camera_entity.get_light()->should_use_vsm()) {
			for (uint32_t rp_idx = 0; rp_idx < num_renderpasses; rp_idx++) {
				push_constants.target_id = -1;
				push_constants.camera_id = camera_entity.get_id();
				push_constants.viewIndex = rp_idx;
				push_constants.flags = (!camera->should_use_multiview()) ? 
					(push_constants.flags | RenderSystemOptions::RASTERIZE_MULTIVIEW): 
					(push_constants.flags & ~RenderSystemOptions::RASTERIZE_MULTIVIEW); 
				bind_camera_compute_descriptor_sets(command_buffer, camera_entity, rp_idx);

				uint32_t local_size_x = 16;
				uint32_t local_size_y = 16;

				uint32_t groupX = (width + local_size_x - 1) / local_size_x;
				uint32_t groupY = (height + local_size_y - 1) / local_size_y;
				uint32_t groupZ = 1;

				command_buffer.pushConstants(gaussian_x.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
				command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, gaussian_x.pipeline);
				command_buffer.dispatch(groupX, groupY, groupZ);

				command_buffer.pushConstants(gaussian_y.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
				command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, gaussian_y.pipeline);
				command_buffer.dispatch(groupX, groupY, groupZ);
			}
		}
	}
}

void RenderSystem::record_blit_camera(Entity &camera_entity, std::map<std::string, std::pair<Camera *, uint32_t>> &window_to_cam)
{
    auto glfw = GLFW::Get();
    auto camera = camera_entity.get_camera();

    if (!camera) throw std::runtime_error("Error, camera was null in recording blits");
	auto &cam_res = camera_resources[camera->get_id()];

    vk::CommandBuffer command_buffer = cam_res.command_buffer;
    Texture * texture = camera->get_texture();

    /* There might be a better way to handle this... */
    if (!texture) throw std::runtime_error("Error, camera texture was null in recording blits");

    /* Blit to GLFW windows */
    for (auto w2c : window_to_cam) {
        if ((w2c.second.first->get_id() == camera_entity.get_camera_id())) {
            /* Window needs a swapchain */
            auto swapchain = glfw->get_swapchain(w2c.first);
            if (!swapchain) {
                continue;
            }

            /* Need to be able to get swapchain/texture by key...*/
            auto swapchain_texture = glfw->get_texture(w2c.first);

            /* Record blit to swapchain */
            if (swapchain_texture && swapchain_texture->is_initialized()) {
                texture->record_blit_to(command_buffer, swapchain_texture, w2c.second.second);
            }
        }
    }

    /* Blit to OpenVR eyes. */
    if (using_openvr) {
        auto ovr = OpenVR::Get();
        if (camera == ovr->get_connected_camera()) {
            auto left_eye_texture = ovr->get_left_eye_texture();
            auto right_eye_texture = ovr->get_right_eye_texture();
            if (left_eye_texture)  texture->record_blit_to(command_buffer, left_eye_texture, 0);
            if (right_eye_texture) texture->record_blit_to(command_buffer, right_eye_texture, 1);
        }
    }
}

void RenderSystem::record_cameras()
{
    auto vulkan = Vulkan::Get();
    auto device = vulkan->get_device();

    /* Get window to camera mapping */
    auto glfw = GLFW::Get();
    auto window_to_cam = glfw->get_window_to_camera_map();
    auto entities = Entity::GetFront();

	/* Some profiling information */
	float ms_per_raytrace = 0.0f;
	float ms_per_raster = 0.0f;
	float ms_per_compute = 0.0f;

	/* Render all cameras */
    auto cameras = Camera::GetFront();
    for (uint32_t entity_id = 0; entity_id < Entity::GetCount(); ++entity_id) {
        /* Entity must be initialized */
        if (!entities[entity_id].is_initialized()) continue;

        /* Entity needs a camera */
        auto camera = entities[entity_id].get_camera();
        if (!camera) continue;

		auto &cam_resources = camera_resources[camera->get_id()];

        /* Camera needs a texture */
        Texture * texture = camera->get_texture();
        if (!texture) continue;

		/* Update camera specific descriptor sets */
		for(uint32_t rp_idx = 0; rp_idx < camera->maxViews; rp_idx++) {
			update_gbuffer_descriptor_sets(entities[entity_id], rp_idx);
		}

        /* Begin recording */
        vk::CommandBufferBeginInfo beginInfo;
        if (cam_resources.command_buffer == vk::CommandBuffer())
		{
			vk::CommandBufferAllocateInfo cmdAllocInfo;
			cmdAllocInfo.commandPool = vulkan->get_graphics_command_pool();
			cmdAllocInfo.level = vk::CommandBufferLevel::ePrimary;
			cmdAllocInfo.commandBufferCount = 1;
			cam_resources.command_buffer = device.allocateCommandBuffers(cmdAllocInfo)[0];
			cam_resources.command_buffer_pool = cmdAllocInfo.commandPool;
		}
        vk::CommandBuffer command_buffer = cam_resources.command_buffer;
        
		/* If entity is a shadow camera (TODO: REFACTOR THIS...) */
        if (entities[entity_id].get_light() != nullptr) {
            /* Skip shadowmaps which shouldn't cast shadows */
            auto light = entities[entity_id].get_light();
            if (!light->should_cast_shadows()) continue;
            if (!light->should_cast_dynamic_shadows()) continue;
        }

		beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        command_buffer.begin(beginInfo);

		record_pre_compute_pass(entities[entity_id]);

		/* Ray trace (only non-shadow cameras) */
        if (ray_tracing_enabled && vulkan->is_ray_tracing_enabled() && (entities[entity_id].get_light() == nullptr))
        {
            float last_time = (float) glfwGetTime();

			if (rasterize_primary_visibility_enabled){
				auto visible_entities = get_visible_entities(camera->get_id(), entity_id);
				record_raster_primary_visibility_renderpass(entities[entity_id], visible_entities);
			}
            record_ray_trace_pass(entities[entity_id]);

			float current_time = (float) glfwGetTime();
			ms_per_raytrace += (float) (current_time - last_time) * 1000.f;
        }
		/* Rasterize (only non-shadow cameras) */
        else if (!ray_tracing_enabled && (entities[entity_id].get_light() == nullptr))
        {
			float last_time = (float) glfwGetTime();

            auto visible_entities = get_visible_entities(camera->get_id(), entity_id);
            record_raster_primary_visibility_renderpass(entities[entity_id], visible_entities);

			float current_time = (float) glfwGetTime();
			ms_per_raster += (float) (current_time - last_time) * 1000.f;
        }
		// Rasterize (only shadow cameras)
		else if (!ray_tracing_enabled && (entities[entity_id].get_light() != nullptr)) {
			float last_time = (float) glfwGetTime();

			auto visible_entities = get_visible_entities(camera->get_id(), entity_id);
            record_shadow_map_renderpass(entities[entity_id], visible_entities);

			float current_time = (float) glfwGetTime();
			ms_per_raster += (float) (current_time - last_time) * 1000.f;
		}

		record_post_compute_pass(entities[entity_id]);
        record_blit_camera(entities[entity_id], window_to_cam);

		/* Need to keep track of largest render order number... */
		if (entities[entity_id].get_camera()->get_render_order() < 0) {
			texture->make_samplable(command_buffer);
		}

        /* End this recording. */
        command_buffer.end();
    }

	ms_per_record_raytrace_pass = ms_per_raytrace;
	ms_per_record_raster_pass = ms_per_raster;
	ms_per_record_compute_pass = ms_per_compute;
}

void RenderSystem::record_compute(bool submit_immediately)
{
	auto vulkan = Libraries::Vulkan::Get();
	if (!vulkan->is_initialized()) throw std::runtime_error("Error: vulkan is not initialized");

	auto dldi = vulkan->get_dldi();
	auto device = vulkan->get_device();
	if (!device) 
		throw std::runtime_error("Error: vulkan device not initialized");

	Texture* ddgiIrradiance = nullptr;
	Texture* ddgiVisibility = nullptr;
	try {
		ddgiIrradiance = Texture::Get("DDGI_IRRADIANCE");
        ddgiVisibility = Texture::Get("DDGI_VISIBILITY");
	} catch (...) {}

	if (ddgiIrradiance == nullptr) return;
	if (ddgiVisibility == nullptr) return;

	vk::CommandBufferBeginInfo beginInfo;
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	compute_command_buffer.begin(beginInfo);

	bool descriptors_bound = bind_compute_descriptor_sets(compute_command_buffer);
	if (descriptors_bound) {		
		// Note, irradiance lut has 6x6 texels per probe, 
		// visibility lut has 16x16 texels per probe

		// Blend DDGI Irradiance
		{
			// Note, irradiance lut has 6x6 texels per probe, 
			// visibility lut has 16x16 texels per probe

			int width = ddgiIrradiance->get_width();
			int height = ddgiIrradiance->get_height();

			push_constants.target_id = -1;
			push_constants.camera_id = -1;
			push_constants.viewIndex = 0;
			bind_compute_descriptor_sets(compute_command_buffer);
			
			compute_command_buffer.pushConstants(ddgi_blend_irradiance.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
			compute_command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, ddgi_blend_irradiance.pipeline);

			uint32_t local_size_x = 16;
			uint32_t local_size_y = 16;

			uint32_t groupX = (width + local_size_x - 1) / local_size_x;
			uint32_t groupY = (height + local_size_y - 1) / local_size_y;
			uint32_t groupZ = 1;
			compute_command_buffer.dispatch(groupX, groupY, groupZ);
		}

		// Blend DDGI Visibility
		{

			int width = ddgiVisibility->get_width();
			int height = ddgiVisibility->get_height();

			push_constants.target_id = -1;
			push_constants.camera_id = -1;
			push_constants.viewIndex = 0;
			bind_compute_descriptor_sets(compute_command_buffer);
			
			compute_command_buffer.pushConstants(ddgi_blend_visibility.pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
			compute_command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, ddgi_blend_visibility.pipeline);

			uint32_t local_size_x = 16;
			uint32_t local_size_y = 16;

			uint32_t groupX = (width + local_size_x - 1) / local_size_x;
			uint32_t groupY = (height + local_size_y - 1) / local_size_y;
			uint32_t groupZ = 1;
			compute_command_buffer.dispatch(groupX, groupY, groupZ);
		}

	}

	if (submit_immediately)
		vulkan->end_one_time_graphics_command_immediately(compute_command_buffer, "general compute commands", false);
	else
	{
		compute_command_buffer.end();
		compute_command_buffer_recorded = true;
	}
}

void RenderSystem::record_build_top_level_bvh(bool submit_immediately)
{
	auto vulkan = Libraries::Vulkan::Get();
	if (!vulkan->is_initialized()) throw std::runtime_error("Error: vulkan is not initialized");

	if (!vulkan->is_ray_tracing_enabled()) 
		throw std::runtime_error("Error: Vulkan device extension VK_NVX_raytracing is not currently enabled.");
	
	auto dldi = vulkan->get_dldi();
	auto device = vulkan->get_device();
	if (!device) 
		throw std::runtime_error("Error: vulkan device not initialized");

    /* Get geometry count */
    std::vector<vk::GeometryNV> geometries;
    auto meshes = Mesh::GetFront();
    for (uint32_t i = 0; i < Mesh::GetCount(); ++i) {
        if (!meshes[i].is_initialized()) continue;
        if (meshes[i].get_nv_geometry() != vk::GeometryNV()) geometries.push_back(meshes[i].get_nv_geometry());
    }

	vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    bvh_command_buffer.begin(beginInfo);

	{
		vk::MemoryBarrier memoryBarrier;
		memoryBarrier.srcAccessMask  = vk::AccessFlagBits::eAccelerationStructureReadNV;
		memoryBarrier.srcAccessMask  |= vk::AccessFlagBits::eAccelerationStructureWriteNV;
		memoryBarrier.dstAccessMask  = vk::AccessFlagBits::eAccelerationStructureReadNV;
		memoryBarrier.dstAccessMask  |= vk::AccessFlagBits::eAccelerationStructureWriteNV;
		bvh_command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, 
			vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, 
			vk::DependencyFlags(),
			{memoryBarrier},
			{},
			{}
		);
	}

	{
		/* Now we can build our acceleration structure */
		bool update = top_level_acceleration_structure_built;

		/* TODO: MOVE INSTANCE STUFF INTO HERE */
		{
			vk::AccelerationStructureInfoNV asInfo;
			asInfo.type = vk::AccelerationStructureTypeNV::eTopLevel;
			asInfo.instanceCount = (uint32_t) MAX_ENTITIES;
			asInfo.geometryCount = 0;//(uint32_t) geometries.size();
			asInfo.flags = vk::BuildAccelerationStructureFlagBitsNV::eAllowUpdate;//VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV;
			// asInfo.pGeometries = geometries.data();

            // TODO: change this to update instead of fresh build
			// UPDATE: Should always rebuild top level BVH...
			if (!top_level_acceleration_structure_built)
			{
				bvh_command_buffer.buildAccelerationStructureNV(&asInfo, 
					instanceBuffer, 0, VK_FALSE, 
					topAS, vk::AccelerationStructureNV(),
					accelerationStructureBuildScratchBuffer, 0, dldi);
			}
			else {
				bvh_command_buffer.buildAccelerationStructureNV(&asInfo, 
					instanceBuffer, 0, VK_TRUE, 
					topAS, topAS,
					accelerationStructureUpdateScratchBuffer, 0, dldi);
			}
		}
	}

	{
		vk::MemoryBarrier memoryBarrier;
		memoryBarrier.srcAccessMask  = vk::AccessFlagBits::eAccelerationStructureReadNV;
		memoryBarrier.srcAccessMask  |= vk::AccessFlagBits::eAccelerationStructureWriteNV;
		memoryBarrier.dstAccessMask  = vk::AccessFlagBits::eAccelerationStructureReadNV;
		bvh_command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, 
			vk::PipelineStageFlagBits::eRayTracingShaderNV, 
			vk::DependencyFlags(),
			{memoryBarrier},
			{},
			{}
		);
	}

	if (submit_immediately)
		vulkan->end_one_time_graphics_command_immediately(bvh_command_buffer, "build acceleration structure", false);
	else
	{
		bvh_command_buffer.end();
		bvh_command_buffer_recorded = true;
	}
}

void RenderSystem::record_blit_textures()
{
    /* Blit any textures to windows which request them. */
    auto glfw = GLFW::Get();
    auto window_to_tex = glfw->get_window_to_texture_map();
    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    blit_command_buffer.begin(beginInfo);
    blit_command_buffer_recorded = true;
    blit_command_buffer_presenting = false;
    for (auto &w2t : window_to_tex) {
        /* Window needs a swapchain */
        auto swapchain = glfw->get_swapchain(w2t.first);
        if (!swapchain) {
            continue;
        }

        /* Need to be able to get swapchain/texture by key...*/
        auto swapchain_texture = glfw->get_texture(w2t.first);

        /* Record blit to swapchain */
        if (swapchain_texture && swapchain_texture->is_initialized()) {
            w2t.second.first->record_blit_to(blit_command_buffer, swapchain_texture, w2t.second.second);
            blit_command_buffer_presenting = true;
        }
    }

    blit_command_buffer.end();
}

void RenderSystem::record_render_commands()
{
    auto vulkan = Vulkan::Get();
    auto device = vulkan->get_device();

    update_openvr_transforms();
    
	{
		float last_time = (float) glfwGetTime();
		auto upload_command = vulkan->begin_one_time_graphics_command();
		/* Upload SSBO data */
		Material::UploadSSBO(upload_command);
		Transform::UploadSSBO(upload_command);
		Light::UploadSSBO(upload_command);
		Camera::UploadSSBO(upload_command);
		Entity::UploadSSBO(upload_command);
		Texture::UploadSSBO(upload_command);
		Mesh::UploadSSBO(upload_command);
		vulkan->end_one_time_graphics_command_immediately(upload_command, "Upload SSBO Data", true);
		float current_time = (float) glfwGetTime();
		ms_per_upload_ssbo = (float) (current_time - last_time) * 1000.f;
	}

	if (ray_tracing_enabled)
	{
		auto vulkan = Libraries::Vulkan::Get();
		if (!vulkan->is_initialized()) throw std::runtime_error("Error: vulkan is not initialized");

		if (!vulkan->is_ray_tracing_enabled()) 
			throw std::runtime_error("Error: Vulkan device extension VK_NVX_raytracing is not currently enabled.");
		
		auto dldi = vulkan->get_dldi();
		auto device = vulkan->get_device();
		if (!device) 
			throw std::runtime_error("Error: vulkan device not initialized");


		/* Gather Instances */
		auto entities = Entity::GetFront();
		std::vector<VkGeometryInstance> instances(MAX_ENTITIES);
		for (uint32_t i = 0; i < MAX_ENTITIES; ++i)
		{
			VkGeometryInstance instance;

			if ((!entities[i].is_initialized())
				|| (!entities[i].get_mesh())
				|| (!entities[i].get_transform())
				|| (!entities[i].get_material())
				|| (entities[i].get_material()->get_name().compare("SkyboxMaterial") == 0)
				|| (!entities[i].get_mesh()->get_low_level_bvh()))
			{

				instance.instanceId = i;
				instance.mask = 0; // means this instance can't be hit.
				instance.instanceOffset = 0;
				instance.accelerationStructureHandle = 0; // not sure if this is allowed...
			}
			else {
				auto llas = entities[i].get_mesh()->get_low_level_bvh();
				if (!llas) continue;

				uint64_t handle = entities[i].get_mesh()->get_low_level_bvh_handle();
				/* Create Instance */
				
				auto local_to_world = entities[i].get_transform()->get_local_to_world_matrix();
				// Matches a working example
				float transform[12] = {
					local_to_world[0][0], local_to_world[1][0], local_to_world[2][0], local_to_world[3][0],
					local_to_world[0][1], local_to_world[1][1], local_to_world[2][1], local_to_world[3][1],
					local_to_world[0][2], local_to_world[1][2], local_to_world[2][2], local_to_world[3][2],
				};

				memcpy(instance.transform, transform, sizeof(instance.transform));
				instance.instanceId = i;
				instance.mask = 0xff;
				instance.instanceOffset = 0;
				// instance.flags = (uint32_t) (vk::GeometryInstanceFlagBitsNV::e);
				instance.accelerationStructureHandle = handle;
			}
		
			instances[i] = instance;
		}

		/* ----- Upload Instances ----- */
		{
			uint32_t instanceBufferSize = (uint32_t)(sizeof(VkGeometryInstance) * MAX_ENTITIES);
			void* ptr = device.mapMemory(instanceBufferMemory, 0, instanceBufferSize);
			memcpy(ptr, instances.data(), instanceBufferSize);
			device.unmapMemory(instanceBufferMemory);
		}
	}

	if (ray_tracing_enabled) {
		record_build_top_level_bvh();
	}


    if (Material::IsInitialized()) {
		bool result = false;
		{
			float last_time = (float) glfwGetTime();
			result = update_push_constants();
			float current_time = (float) glfwGetTime();
			ms_per_update_push_constants = (float) (current_time - last_time) * 1000.f;
		}


        if (result == true) {
			float last_time = (float) glfwGetTime();
			/* Update global descriptor sets */
			update_global_descriptor_sets();
			if (vulkan->is_ray_tracing_enabled())
				update_raytracing_descriptor_sets(topAS);
			
			record_compute();
            
			record_cameras();
			
			float current_time = (float) glfwGetTime();
			ms_per_record_cameras = (float) (current_time - last_time) * 1000.f;
        }
    }
    
	{
		float last_time = (float) glfwGetTime();
    	record_blit_textures();
		float current_time = (float) glfwGetTime();
		ms_per_record_blit_textures = (float) (current_time - last_time) * 1000.f;
	}
}

void RenderSystem::update_openvr_transforms()
{
    /* Set OpenVR transform data right before rendering */
    if (using_openvr) {
        auto ovr = OpenVR::Get();
        ovr->wait_get_poses();

        /* Set camera information */
        auto camera = ovr->get_connected_camera();
        
        if (camera) {
            /* Move the camera to where the headset is. */
            camera->set_view(ovr->get_left_view_matrix(), 0);
            camera->set_custom_projection(ovr->get_left_projection_matrix(.1f), .1f, 0);
            camera->set_view(ovr->get_right_view_matrix(), 1);
            camera->set_custom_projection(ovr->get_right_projection_matrix(.1f), .1f, 1);
        }

        /* Set transform information */
        auto left_hand_transform = ovr->get_connected_left_hand_transform();
        auto right_hand_transform = ovr->get_connected_right_hand_transform();
        auto camera_transform = ovr->get_connected_camera_transform();

        if (left_hand_transform)
        {
            left_hand_transform->set_transform(ovr->get_left_controller_transform());
        }

        if (right_hand_transform)
        {
            right_hand_transform->set_transform(ovr->get_right_controller_transform());
        }

        /* TODO... camera transform currently set via view... */
        if (camera_transform)
        {
            
        }
    }
}

void RenderSystem::present_openvr_frames()
{
    /* Ignore if openvr isn't in use. */
    if (!using_openvr) return;

    auto ovr = OpenVR::Get();
    auto camera = ovr->get_connected_camera();

    /* Dont submit anything if no camera is connected to VR. */
    if (!camera) return;

    /* Don't render anything if the camera component doesn't have a color texture. */
    Texture * texture = camera->get_texture();
    if (texture == nullptr) return;
    
    /* Submit the left and right eye textures to OpenVR */
    ovr->submit_textures();
}

/* THIS FUNCTION IS OVERDUE FOR REFACTORING... */
void RenderSystem::enqueue_render_commands() {			
    auto vulkan = Vulkan::Get(); auto device = vulkan->get_device();
    auto glfw = GLFW::Get();

    auto window_to_cam = glfw->get_window_to_camera_map();
    auto window_to_tex = glfw->get_window_to_texture_map();

    auto entities = Entity::GetFront();
    auto cameras = Camera::GetFront();
    
    std::vector<Vulkan::CommandQueueItem> command_queue;
	std::vector<std::vector<std::shared_ptr<ComputeNode>>> compute_graph;
	std::vector<vk::Fence> level_fences;

    final_renderpass_semaphores.clear();

    /* Aggregate command sets */
    std::vector<std::shared_ptr<ComputeNode>> last_level;
    std::vector<std::shared_ptr<ComputeNode>> current_level;


    uint32_t level_idx = 0;

	/* Before anything else, enqueue the BVH build (if using RTX) */
    if (bvh_command_buffer_recorded) {
        auto node = std::make_shared<ComputeNode>();
        node->level = level_idx;
        node->queue_idx = 0;
        node->command_buffer = bvh_command_buffer;
		current_level.push_back(node);
		compute_graph.push_back(current_level);
		last_level = current_level;
        current_level = std::vector<std::shared_ptr<ComputeNode>>();
		level_idx++;
    }

	/* Next, record global compute commands (if any) */
	if (compute_command_buffer_recorded) {
        auto node = std::make_shared<ComputeNode>();
        node->level = level_idx;
        node->queue_idx = 0;
        node->command_buffer = compute_command_buffer;
		current_level.push_back(node);
		compute_graph.push_back(current_level);
		last_level = current_level;
        current_level = std::vector<std::shared_ptr<ComputeNode>>();
		level_idx++;
		compute_command_buffer_recorded = false;
    }

	/* Now, enqueue camera passes */
    for (int32_t rp_idx = Camera::GetMinRenderOrder(); rp_idx <= Camera::GetMaxRenderOrder(); ++rp_idx)
    {
        uint32_t queue_idx = 0;
        bool command_found = false;
        for (uint32_t e_id = 0; e_id < Entity::GetCount(); ++e_id) {
            if ((!entities[e_id].is_initialized()) ||
                (!entities[e_id].get_camera()) ||
                (!entities[e_id].get_camera()->get_texture()) ||
                (entities[e_id].get_camera()->get_render_order() != rp_idx)
            ) continue;

            /* If the entity is a shadow camera and shouldn't cast shadows, skip it. (TODO: REFACTOR THIS...) */
            if (entities[e_id].get_light() && 
                ((!entities[e_id].get_light()->should_cast_shadows())
                    || (!entities[e_id].get_light()->should_cast_dynamic_shadows()))) continue; 

            /* Make a compute node for this command buffer */
            auto node = std::make_shared<ComputeNode>();
            node->level = level_idx;
            node->queue_idx = queue_idx;
			auto &cam_res = camera_resources[entities[e_id].get_camera()->get_id()];
            node->command_buffer = cam_res.command_buffer;

            /* Add any connected windows to this node */
            for (auto w2c : window_to_cam) {
                if ((w2c.second.first->get_id() == entities[e_id].get_camera()->get_id())) {
                    if (!glfw->get_swapchain(w2c.first)) continue;
                    auto swapchain_texture = glfw->get_texture(w2c.first);
                    if ( (!swapchain_texture) || (!swapchain_texture->is_initialized())) continue;
                    node->connected_windows.push_back(w2c.first);
                }
            }

            /* Mark the previous level as a set of dependencies */
            if (level_idx > 0) node->dependencies = last_level;
            current_level.push_back(node);
            command_found = true;
            queue_idx++;
        }

        if (command_found) {
            /* Keep a reference to the top of the graph. */
            compute_graph.push_back(current_level);

            /* Update children references */
            for (auto &node : last_level) {
                node->children = current_level;
            }

            /* Increase the graph level, and replace the last level with the current one */
            level_idx++;
            last_level = current_level;
            current_level = std::vector<std::shared_ptr<ComputeNode>>();
        }
    }

    /* Enqueue blit/present commands */
    if (blit_command_buffer_recorded) {
        auto node = std::make_shared<ComputeNode>();
        node->level = level_idx;
        node->queue_idx = 0;
        node->command_buffer = blit_command_buffer;
        if (blit_command_buffer_presenting) {
            for (auto &w2t : window_to_tex) {
                node->connected_windows.push_back(w2t.first);
            }
        }
        blit_command_buffer_recorded = false;
    }

    /* Create semaphores and fences for nodes in the graph */
	level_idx = 0;
    for (auto &level : compute_graph) {
		/* For now, assume the level doesnt need a fence */
		level_fences.push_back(vk::Fence());
        for (auto &node : level) {
            /* All internal nodes need to signal each child */
            for (uint32_t i = 0; i < node->children.size(); ++i) {
                node->signal_semaphores.push_back(get_semaphore());
            }

            /* If any windows are dependent on this node, signal those too. */
            for (auto &window_key : node->connected_windows) {
                auto image_available = glfw->get_image_available_semaphore(window_key, currentFrame);
                if (image_available != vk::Semaphore()) {
                    if (!glfw->get_swapchain(window_key)) continue;
                    if ((!glfw->get_texture(window_key)) || (!glfw->get_texture(window_key)->is_initialized())) continue;
                    
                    auto render_complete = get_semaphore();
                    node->window_signal_semaphores.push_back(render_complete);
                    final_renderpass_semaphores.push_back(render_complete);
                }
            }
			/* If the node has no children, we need to add a fence to this level */			
			if (node->children.size() == 0) {
				if (level_fences[level_idx] == vk::Fence())
					level_fences[level_idx] = get_fence();
			}
        }

		// /* TEMPORARY, trying to force acceleration structure to be built before tracing rays... */
		// if (bvh_command_buffer_recorded && (level_idx == 0)){
		// 	level_fences[level_idx] = get_fence(); 
		// }

		level_idx++;
    }

    level_idx = 0;
    for (auto &level : compute_graph) {
        vk::SemaphoreCreateInfo semaphoreInfo;
        vk::FenceCreateInfo fenceInfo;

        std::set<vk::Semaphore> wait_semaphores_set;
        std::set<vk::Semaphore> signal_semaphores_set;
        std::vector<vk::CommandBuffer> command_buffers;

        Vulkan::CommandQueueItem item;
        
        for (auto &node : level) {
            /* Connect signal semaphores to wait semaphore slots in the graph */
            if (node->dependencies.size() > 0) {
                for (auto &dependency : node->dependencies) {
                    for (uint32_t i = 0; i < dependency->children.size(); ++i) {
                        if (dependency->children[i] == node) {
                            wait_semaphores_set.insert(dependency->signal_semaphores[i]);
                            // Todo: optimize this 
                            // wait_stage_masks.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
                            break;
                        }
                    }
                }
            }

            /* If the node is connected to a window, wait for that window's image to be available */
            for (auto &window_key : node->connected_windows) {
                auto image_available = glfw->get_image_available_semaphore(window_key, currentFrame);
                if (image_available != vk::Semaphore()) {
                    wait_semaphores_set.insert(image_available);
                    // wait_stage_masks.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
                }
            }

            for (auto &signal_semaphore : node->signal_semaphores) 
                signal_semaphores_set.insert(signal_semaphore);

            for (auto &signal_semaphore : node->window_signal_semaphores)
                signal_semaphores_set.insert(signal_semaphore);

			item.fence = level_fences[level_idx];
            // if (node->fence != vk::Fence()) {
            //     if (item.fence != vk::Fence()) throw std::runtime_error("Error, not expecting more than one fence per level!");
            //     item.fence = node->fence;
            // }

            item.commandBuffers.push_back(node->command_buffer);            
            // For some reason, queues arent actually ran in parallel? Also calling submit many times turns out to be expensive...
            // vulkan->enqueue_graphics_commands(node->command_buffers, wait_semaphores, wait_stage_masks, signal_semaphores, 
            //     node->fence, "draw call lvl " + std::to_string(node->level) + " queue " + std::to_string(node->queue_idx), node->queue_idx);
        }

        /* Making hasty assumptions here about wait stage masks. */
        for (auto &semaphore : wait_semaphores_set) {
            item.waitSemaphores.push_back(semaphore);
            item.waitDstStageMask.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
			// if ((level_idx == 0) && (bvh_command_buffer_recorded)) {
			// 	item.waitDstStageMask[item.waitDstStageMask.size() - 1] |= vk::PipelineStageFlagBits::eRayTracingShaderNV;
			// }
			// if (bvh_command_buffer_recorded) {
			// 	item.waitDstStageMask[item.waitDstStageMask.size() - 1] |= vk::PipelineStageFlagBits::eAccelerationStructureBuildNV;
			// 	item.waitDstStageMask[item.waitDstStageMask.size() - 1] |= vk::PipelineStageFlagBits::eRayTracingShaderNV;
			// }
        }
        
        for (auto &semaphore : signal_semaphores_set) 
            item.signalSemaphores.push_back(semaphore);
        
        item.hint = "draw call lvl " + std::to_string(level_idx) + " queue " + std::to_string(0);
        item.queue_idx = 0;
        command_queue.push_back(item);
        level_idx++;
    }
    
    /* Enqueue the commands */
    for (auto &item : command_queue) vulkan->enqueue_graphics_commands(item);
    semaphores_submitted();
    fences_submitted();
    bvh_command_buffer_recorded = false;
}

void RenderSystem::mark_cameras_as_render_complete()
{
    /* Render all cameras */
    auto entities = Entity::GetFront();
    auto cameras = Camera::GetFront();
    for (uint32_t entity_id = 0; entity_id < Entity::GetCount(); ++entity_id) {
        /* Entity must be initialized */
        if (!entities[entity_id].is_initialized()) continue;

        /* Entity needs a camera */
        auto camera = entities[entity_id].get_camera();
        if (!camera) continue;

        /* Camera needs a texture */
        Texture * texture = camera->get_texture();
        if (!texture) continue;

        /* If entity is a shadow camera (TODO: REFACTOR THIS...) */
        if (entities[entity_id].get_light() != nullptr) {
            /* Skip shadowmaps which shouldn't cast shadows */
            auto light = entities[entity_id].get_light();
            if (!light->should_cast_shadows()) continue;
            if (!light->should_cast_dynamic_shadows()) continue;
        }

        camera->mark_render_as_complete();
    }

}

void RenderSystem::release_vulkan_resources() 
{
    auto vulkan = Vulkan::Get();

    /* If Vulkan was never initialized, we never allocated anything. */
    if (!vulkan->is_initialized()) return;

    /* Likewise if a device was never created, we never allocated anything. */
    auto device = vulkan->get_device();
    if (device == vk::Device()) return;

    /* Don't free vulkan resources more than once. */
    if (!vulkan_resources_created) return;

    /* Release vulkan resources */
    device.freeCommandBuffers(vulkan->get_graphics_command_pool(), {compute_command_buffer});
    compute_command_buffer = vk::CommandBuffer();
	
	device.freeCommandBuffers(vulkan->get_graphics_command_pool(), {blit_command_buffer});
    blit_command_buffer = vk::CommandBuffer();

	device.freeCommandBuffers(vulkan->get_graphics_command_pool(), {bvh_command_buffer});
    bvh_command_buffer = vk::CommandBuffer();

    device.destroyDescriptorSetLayout(componentDescriptorSetLayout);
	device.destroyDescriptorPool(componentDescriptorPool);

	device.destroyDescriptorSetLayout(textureDescriptorSetLayout);
	device.destroyDescriptorPool(textureDescriptorPool);

	device.destroyDescriptorSetLayout(positionsDescriptorSetLayout);
	device.destroyDescriptorPool(positionsDescriptorPool);

	device.destroyDescriptorSetLayout(normalsDescriptorSetLayout);
	device.destroyDescriptorPool(normalsDescriptorPool);

	device.destroyDescriptorSetLayout(colorsDescriptorSetLayout);
	device.destroyDescriptorPool(colorsDescriptorPool);

	device.destroyDescriptorSetLayout(texcoordsDescriptorSetLayout);
	device.destroyDescriptorPool(texcoordsDescriptorPool);

	device.destroyDescriptorSetLayout(indexDescriptorSetLayout);
	device.destroyDescriptorPool(indexDescriptorPool);

    componentDescriptorSetLayout = vk::DescriptorSetLayout();
	componentDescriptorPool = vk::DescriptorPool();
	textureDescriptorSetLayout = vk::DescriptorSetLayout();
	textureDescriptorPool = vk::DescriptorPool();
	positionsDescriptorSetLayout = vk::DescriptorSetLayout();
	positionsDescriptorPool = vk::DescriptorPool();
	normalsDescriptorSetLayout = vk::DescriptorSetLayout();
	normalsDescriptorPool = vk::DescriptorPool();
	colorsDescriptorSetLayout = vk::DescriptorSetLayout();
	colorsDescriptorPool = vk::DescriptorPool();
	texcoordsDescriptorSetLayout = vk::DescriptorSetLayout();
	texcoordsDescriptorPool = vk::DescriptorPool();
	indexDescriptorSetLayout = vk::DescriptorSetLayout();
	indexDescriptorPool = vk::DescriptorPool();

	for (auto item : shaderModuleCache) {
		device.destroyShaderModule(item.second);
	}

	for (auto &item : camera_resources) {
		auto &resource = item.second;
		if (resource.queryPool != vk::QueryPool()) {
			device.destroyQueryPool(resource.queryPool);
			resource.queryPool = vk::QueryPool();
		}
		if (resource.command_buffer != vk::CommandBuffer()) {
			device.freeCommandBuffers(resource.command_buffer_pool, {resource.command_buffer});
			resource.command_buffer = vk::CommandBuffer();
		}
		for (auto rp : resource.primary_visibility_renderpasses) {
			device.destroyRenderPass(rp);
		}
		resource.primary_visibility_renderpasses.clear();
		for (auto rp : resource.shadow_map_renderpasses) {
			device.destroyRenderPass(rp);
		}
		resource.shadow_map_renderpasses.clear();
		for (auto rp : resource.primary_visibility_depth_prepasses) {
			device.destroyRenderPass(rp);
		}
		resource.primary_visibility_depth_prepasses.clear();
		for (auto rp : resource.shadow_map_depth_prepasses) {
			device.destroyRenderPass(rp);
		}
		resource.shadow_map_depth_prepasses.clear();
	}
    vulkan_resources_created = false;
}

void RenderSystem::allocate_vulkan_resources()
{
    /* Don't create vulkan resources more than once. */
    if (vulkan_resources_created) return;
    vulkan_resources_created = true;
	
	auto vulkan = Vulkan::Get();
    auto device = vulkan->get_device();
    auto dldi = vulkan->get_dldi();
	
    create_descriptor_set_layouts();
	create_descriptor_pools();
	create_vertex_input_binding_descriptions();
	create_vertex_attribute_descriptions();
	update_global_descriptor_sets();

    setup_compute_pipelines();
	if (vulkan->is_ray_tracing_enabled()) {
    	setup_raytracing_pipelines();
	}

    /* Create a main command buffer if one does not already exist */
    vk::CommandBufferAllocateInfo mainCmdAllocInfo;
    mainCmdAllocInfo.level = vk::CommandBufferLevel::ePrimary;
    mainCmdAllocInfo.commandPool = vulkan->get_graphics_command_pool();
    mainCmdAllocInfo.commandBufferCount = max_frames_in_flight;

    vk::SemaphoreCreateInfo semaphoreInfo;
    vk::FenceCreateInfo fenceInfo;
    
    compute_command_buffer = device.allocateCommandBuffers(mainCmdAllocInfo)[0];
    bvh_command_buffer = device.allocateCommandBuffers(mainCmdAllocInfo)[0];
    blit_command_buffer = device.allocateCommandBuffers(mainCmdAllocInfo)[0];

    // Create top level acceleration structure resources
    if (vulkan->is_ray_tracing_enabled()) {
        auto CreateAccelerationStructure = [&](vk::AccelerationStructureTypeNV type, uint32_t geometryCount,
            vk::GeometryNV* geometries, uint32_t instanceCount, vk::AccelerationStructureNV& AS, vk::DeviceMemory& memory)
        {
            vk::AccelerationStructureCreateInfoNV accelerationStructureInfo;
            accelerationStructureInfo.compactedSize = 0;
            accelerationStructureInfo.info.type = type;
            accelerationStructureInfo.info.instanceCount = instanceCount;
            accelerationStructureInfo.info.geometryCount = geometryCount;
            accelerationStructureInfo.info.pGeometries = geometries;
			accelerationStructureInfo.info.flags = vk::BuildAccelerationStructureFlagBitsNV::eAllowUpdate;

            AS = device.createAccelerationStructureNV(accelerationStructureInfo, nullptr, dldi);

            vk::AccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo;
            memoryRequirementsInfo.accelerationStructure = AS;
            memoryRequirementsInfo.type = vk::AccelerationStructureMemoryRequirementsTypeNV::eObject;

            vk::MemoryRequirements2 memoryRequirements;
            memoryRequirements = device.getAccelerationStructureMemoryRequirementsNV(memoryRequirementsInfo, dldi);

            vk::MemoryAllocateInfo memoryAllocateInfo;
            memoryAllocateInfo.allocationSize = memoryRequirements.memoryRequirements.size;
            memoryAllocateInfo.memoryTypeIndex = vulkan->find_memory_type(memoryRequirements.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

            memory = device.allocateMemory(memoryAllocateInfo);
            
            vk::BindAccelerationStructureMemoryInfoNV bindInfo;
            bindInfo.accelerationStructure = AS;
            bindInfo.memory = memory;
            bindInfo.memoryOffset = 0;
            bindInfo.deviceIndexCount = 0;
            bindInfo.pDeviceIndices = nullptr;

            device.bindAccelerationStructureMemoryNV({bindInfo}, dldi);
        };

        /* Create top level acceleration structure */
        CreateAccelerationStructure(vk::AccelerationStructureTypeNV::eTopLevel,
            0, nullptr, MAX_ENTITIES, topAS, topASMemory);

        /* ----- Create Instance Buffer ----- */
		uint32_t instanceBufferSize = (uint32_t) (sizeof(VkGeometryInstance) * MAX_ENTITIES);

		vk::BufferCreateInfo instanceBufferInfo;
		instanceBufferInfo.size = instanceBufferSize;
		instanceBufferInfo.usage = vk::BufferUsageFlagBits::eRayTracingNV | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;
		instanceBufferInfo.sharingMode = vk::SharingMode::eExclusive;

		instanceBuffer = device.createBuffer(instanceBufferInfo);

		vk::MemoryRequirements instanceBufferRequirements;
		instanceBufferRequirements = device.getBufferMemoryRequirements(instanceBuffer);

		vk::MemoryAllocateInfo instanceMemoryAllocateInfo;
		instanceMemoryAllocateInfo.allocationSize = instanceBufferRequirements.size;
		instanceMemoryAllocateInfo.memoryTypeIndex = vulkan->find_memory_type(instanceBufferRequirements.memoryTypeBits, 
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent );

		instanceBufferMemory = device.allocateMemory(instanceMemoryAllocateInfo);
		
		device.bindBufferMemory(instanceBuffer, instanceBufferMemory, 0);

        /* Allocate scratch buffer for building */
        auto GetScratchBufferSize = [&](vk::AccelerationStructureNV handle, vk::AccelerationStructureMemoryRequirementsTypeNV type)
        {
            vk::AccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo;
            memoryRequirementsInfo.accelerationStructure = handle;
            memoryRequirementsInfo.type = type;

            vk::MemoryRequirements2 memoryRequirements;
            memoryRequirements = device.getAccelerationStructureMemoryRequirementsNV( memoryRequirementsInfo, dldi);

            vk::DeviceSize result = std::max((uint64_t)memoryRequirements.memoryRequirements.size, (uint64_t)memoryRequirements.memoryRequirements.alignment);
            return result;
        };

		{
			vk::DeviceSize scratchBufferSize = GetScratchBufferSize(topAS, vk::AccelerationStructureMemoryRequirementsTypeNV::eBuildScratch);

			vk::BufferCreateInfo bufferInfo;
			bufferInfo.size = scratchBufferSize;
			bufferInfo.usage = vk::BufferUsageFlagBits::eRayTracingNV;
			bufferInfo.sharingMode = vk::SharingMode::eExclusive;
			accelerationStructureBuildScratchBuffer = device.createBuffer(bufferInfo);
			
			vk::MemoryRequirements scratchBufferRequirements;
			scratchBufferRequirements = device.getBufferMemoryRequirements(accelerationStructureBuildScratchBuffer);
			
			vk::MemoryAllocateInfo scratchMemoryAllocateInfo;
			scratchMemoryAllocateInfo.allocationSize = scratchBufferRequirements.size;
			scratchMemoryAllocateInfo.memoryTypeIndex = vulkan->find_memory_type(scratchBufferRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

			accelerationStructureBuildScratchMemory = device.allocateMemory(scratchMemoryAllocateInfo);
			device.bindBufferMemory(accelerationStructureBuildScratchBuffer, accelerationStructureBuildScratchMemory, 0);
		}

		{
			record_build_top_level_bvh(true);
			device.getAccelerationStructureHandleNV(topAS, sizeof(uint64_t), &topASHandle, dldi);
		}

		{
			vk::DeviceSize scratchBufferSize = GetScratchBufferSize(topAS, vk::AccelerationStructureMemoryRequirementsTypeNV::eUpdateScratch);

			vk::BufferCreateInfo bufferInfo;
			bufferInfo.size = scratchBufferSize;
			bufferInfo.usage = vk::BufferUsageFlagBits::eRayTracingNV;
			bufferInfo.sharingMode = vk::SharingMode::eExclusive;
			accelerationStructureUpdateScratchBuffer = device.createBuffer(bufferInfo);
			
			vk::MemoryRequirements scratchBufferRequirements;
			scratchBufferRequirements = device.getBufferMemoryRequirements(accelerationStructureUpdateScratchBuffer);
			
			vk::MemoryAllocateInfo scratchMemoryAllocateInfo;
			scratchMemoryAllocateInfo.allocationSize = scratchBufferRequirements.size;
			scratchMemoryAllocateInfo.memoryTypeIndex = vulkan->find_memory_type(scratchBufferRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

			accelerationStructureUpdateScratchMemory = device.allocateMemory(scratchMemoryAllocateInfo);
			device.bindBufferMemory(accelerationStructureUpdateScratchBuffer, accelerationStructureUpdateScratchMemory, 0);
		}
    }
}

void RenderSystem::enqueue_bvh_rebuild()
{
	top_level_acceleration_structure_built = false;
}

bool RenderSystem::start()
{
    /* Dont start unless initialied. Dont start twice. */
    if (!initialized) return false;
    if (running) return false;

    auto loop = [this](future<void> futureObj) {
        last_frame_time = glfwGetTime();
        auto glfw = GLFW::Get();

        while (true)
        {
            if (futureObj.wait_for(.1ms) == future_status::ready) break;

            /* Regulate the framerate. */
            current_time = glfwGetTime();
            if ((!using_openvr) && ((current_time - last_frame_time) < .008)) continue;
			ms_per_frame = (float) (current_time - last_frame_time) * 1000.0f;
            last_frame_time = current_time;

            /* Wait until vulkan is initialized before rendering. */
            auto vulkan = Vulkan::Get();
            if (!vulkan->is_initialized()) continue;
            
            /* For error handling purposes related to queue access */
            vulkan->register_main_thread();


            /* 0. Allocate the resources we'll need to render this scene. */
            allocate_vulkan_resources();

            {
                /* Lock the window mutex to get access to swapchains and window textures. */
                std::shared_ptr<std::lock_guard<std::mutex>> window_lock;
                auto window_mutex = glfw->get_mutex();
                auto mutex = window_mutex.get();
                window_lock = std::make_shared<std::lock_guard<std::mutex>>(*mutex);

                /* 1. Optionally aquire swapchain images. Signal image available semaphores. */
				{
					float last_time = (float) glfwGetTime();
                	glfw->acquire_swapchain_images(currentFrame);
					float current_time = (float) glfwGetTime();
					ms_per_acquire_swapchain_images = (float) (current_time - last_time) * 1000.f;
				}

                /* 2. Record render commands. */
				{
					float last_time = (float) glfwGetTime();
                	record_render_commands();
					float current_time = (float) glfwGetTime();
					ms_per_record_commands = (float) (current_time - last_time) * 1000.f;
				}

                /* 3. Wait on image available. Enqueue graphics commands. Optionally signal render complete semaphore. */
				{
					float last_time = (float) glfwGetTime();
                	enqueue_render_commands();
					float current_time = (float) glfwGetTime();
					ms_per_enqueue_commands = (float) (current_time - last_time) * 1000.f;
				}

                /* Submit enqueued graphics commands */
				{
					float last_time = (float) glfwGetTime();
					
					vulkan->submit_graphics_commands();
					mark_cameras_as_render_complete();
					
					float current_time = (float) glfwGetTime();
					ms_per_submit_commands = (float) (current_time - last_time) * 1000.f;
				}

                /* 4. Optional: Wait on render complete. Present a frame. */
				{
					float last_time = (float) glfwGetTime();

					present_openvr_frames();
					glfw->present_glfw_frames(final_renderpass_semaphores);
					vulkan->submit_present_commands();

					float current_time = (float) glfwGetTime();
					ms_per_present_frames = (float) (current_time - last_time) * 1000.f;
				}

				{
					float last_time = (float) glfwGetTime();
                	download_visibility_queries();
					float current_time = (float) glfwGetTime();
					ms_per_download_visibility_queries = (float) (current_time - last_time) * 1000.f;
				}
            }

            currentFrame = (currentFrame + 1) % max_frames_in_flight;
        }

        release_vulkan_resources();
    };

    /* Use future promise to terminate the loop on stop. */
    exitSignal = promise<void>();
    future<void> futureObj = exitSignal.get_future();
    eventThread = thread(loop, move(futureObj));

    running = true;
    return true;
}

bool RenderSystem::stop()
{
    
    if (!initialized)
        return false;
    if (!running)
        return false;

    exitSignal.set_value();
    eventThread.join();

    running = false;
    return true;
}

/* Wrapper for shader module creation */
vk::ShaderModule RenderSystem::create_shader_module(std::string name, const std::vector<char>& code) {
	if (shaderModuleCache.find(name) != shaderModuleCache.end()) return shaderModuleCache[name];
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	vk::ShaderModuleCreateInfo createInfo;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	vk::ShaderModule shaderModule = device.createShaderModule(createInfo);
	shaderModuleCache[name] = shaderModule;
	return shaderModule;
}

/* Under the hood, all material types have a set of Vulkan pipeline objects. */
void RenderSystem::create_raster_pipeline(
	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages, // yes
	std::vector<vk::VertexInputBindingDescription> bindingDescriptions, // yes
	std::vector<vk::VertexInputAttributeDescription> attributeDescriptions, // yes
	std::vector<vk::DescriptorSetLayout> componentDescriptorSetLayouts, // yes
	PipelineParameters parameters,
	vk::RenderPass renderpass,
	uint32 subpass,
	std::unordered_map<PipelineType, vk::Pipeline> &pipelines,
	vk::PipelineLayout &layout 
) {
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	/* Vertex Input */
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
	vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)bindingDescriptions.size();
	vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
	vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	vk::PushConstantRange range;
	range.offset = 0;
	range.size = sizeof(PushConsts);
	range.stageFlags = vk::ShaderStageFlagBits::eAll;

	/* Connect things together with pipeline layout */
	vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
	pipelineLayoutInfo.setLayoutCount = (uint32_t)componentDescriptorSetLayouts.size();
	pipelineLayoutInfo.pSetLayouts = componentDescriptorSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 1; // TODO: this needs to account for entity id
	pipelineLayoutInfo.pPushConstantRanges = &range; // TODO: this needs to account for entity id

	/* Create the pipeline layout */
	layout = device.createPipelineLayout(pipelineLayoutInfo);
	
	vk::GraphicsPipelineCreateInfo pipelineInfo;
	pipelineInfo.stageCount = (uint32_t)shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &parameters.inputAssembly;
	pipelineInfo.pViewportState = &parameters.viewportState;
	pipelineInfo.pRasterizationState = &parameters.rasterizer;
	pipelineInfo.pMultisampleState = &parameters.multisampling;
	pipelineInfo.pDepthStencilState = &parameters.depthStencil;
	pipelineInfo.pColorBlendState = &parameters.colorBlending;
	pipelineInfo.pDynamicState = &parameters.dynamicState; // Optional
	pipelineInfo.layout = layout;
	pipelineInfo.renderPass = renderpass;
	pipelineInfo.subpass = subpass;
	pipelineInfo.basePipelineHandle = vk::Pipeline(); // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	#ifndef DISABLE_REVERSE_Z
	auto prepassDepthCompareOp = vk::CompareOp::eGreater;
	auto prepassDepthCompareOpEqual = vk::CompareOp::eGreaterOrEqual;
	auto depthCompareOpGreaterEqual = vk::CompareOp::eGreaterOrEqual;
	auto depthCompareOpLessEqual = vk::CompareOp::eLessOrEqual;
	#else
	auto prepassDepthCompareOp = vk::CompareOp::eLess;
	auto prepassDepthCompareOpEqual = vk::CompareOp::eLessOrEqual;
	auto depthCompareOpGreaterEqual = vk::CompareOp::eLessOrEqual;
	auto depthCompareOpLessEqual = vk::CompareOp::eGreaterOrEqual;
	#endif	


	/* Create pipeline */
	pipelines[PIPELINE_TYPE_NORMAL] = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
	
	auto old_depth_test_enable_setting = parameters.depthStencil.depthTestEnable;
	auto old_depth_write_enable_setting = parameters.depthStencil.depthWriteEnable;
	auto old_cull_mode = parameters.rasterizer.cullMode;
	auto old_depth_compare_op = parameters.depthStencil.depthCompareOp;

	parameters.depthStencil.setDepthTestEnable(true);
	parameters.depthStencil.setDepthWriteEnable(false); // possibly rename this?

	// fragmentdepth[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
	// fragmentdepth[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;
	parameters.depthStencil.setDepthCompareOp(prepassDepthCompareOpEqual);	// Needed for transparent objects.
	// parameters.depthStencil.setDepthCompareOp(prepassDepthCompareOpEqual);	
	
	parameters.rasterizer.setCullMode(vk::CullModeFlagBits::eNone); // possibly rename this?
	pipelines[PIPELINE_TYPE_DEPTH_WRITE_DISABLED] = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
	
	parameters.depthStencil.setDepthCompareOp(prepassDepthCompareOpEqual);	
	parameters.rasterizer.setCullMode(old_cull_mode); // possibly rename this?
	parameters.depthStencil.setDepthWriteEnable(true); // possibly rename this?

	// parameters.depthStencil.setDepthWriteEnable(old_depth_write_enable_setting);
	// parameters.depthStencil.setDepthTestEnable(true);
	parameters.depthStencil.setDepthCompareOp(depthCompareOpGreaterEqual);
	pipelines[PIPELINE_TYPE_DEPTH_TEST_GREATER] = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
	parameters.depthStencil.setDepthCompareOp(depthCompareOpLessEqual);
	pipelines[PIPELINE_TYPE_DEPTH_TEST_LESS] = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
	parameters.depthStencil.setDepthTestEnable(old_depth_test_enable_setting);
	parameters.depthStencil.setDepthCompareOp(old_depth_compare_op);
	parameters.depthStencil.setDepthWriteEnable(old_depth_write_enable_setting);


	// auto old_fill_setting = pipelineInfo.pRasterizationState->polygonMode;
	// pipelineInfo.pRasterizationState->polygonMode = vk::Poly

	auto old_polygon_mode = parameters.rasterizer.polygonMode;
	parameters.rasterizer.polygonMode = vk::PolygonMode::eLine;
	pipelines[PIPELINE_TYPE_WIREFRAME] = device.createGraphicsPipelines(vk::PipelineCache(), {pipelineInfo})[0];
	parameters.rasterizer.polygonMode = old_polygon_mode;
}


/* Compiles all shaders */
void RenderSystem::setup_primary_visibility_raster_pipelines(vk::RenderPass renderpass, uint32_t sampleCount, bool use_depth_prepass)
{
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	auto sampleFlag = vulkan->highest(vulkan->min(vulkan->get_closest_sample_count_flag(sampleCount), vulkan->get_msaa_sample_flags()));

	#ifndef DISABLE_REVERSE_Z
	auto depthCompareOp = vk::CompareOp::eGreater;
	auto depthCompareOpEqual = vk::CompareOp::eGreaterOrEqual;
	#else
	auto depthCompareOp = vk::CompareOp::eLess;
	auto depthCompareOpEqual = vk::CompareOp::eLessOrEqual;
	#endif

	/* RASTER GRAPHICS PIPELINES */

	/* ------ Primary Visibility  ------ */
	{
		raster_primary_visibility[renderpass] = RasterPipelineResources();
		raster_primary_visibility[renderpass].pipelineParameters.initialize(USED_PRIMARY_VISIBILITY_G_BUFFERS);

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/RasterPrograms/PrimaryVisibility/vert.spv"));
		auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/RasterPrograms/PrimaryVisibility/frag.spv"));

		/* Create shader modules */
		auto vertShaderModule = create_shader_module("raster_primary_visibility_vert", vertShaderCode);
		auto fragShaderModule = create_shader_module("raster_primary_visibility_frag", fragShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
		vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
		fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };

		/* Alpha channel used to store extra g buffer information */
		for (uint32_t gid = 0; gid < USED_PRIMARY_VISIBILITY_G_BUFFERS; gid++) {
			raster_primary_visibility[renderpass].pipelineParameters.blendAttachments[gid].blendEnable = false;
		}
		
		/* Because we use a depth prepass */
		if (use_depth_prepass) {
			raster_primary_visibility[renderpass].pipelineParameters.depthStencil.depthTestEnable = true;
			raster_primary_visibility[renderpass].pipelineParameters.depthStencil.depthWriteEnable = false; // not VK_TRUE since we have a depth prepass
			raster_primary_visibility[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
		}
		// depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; //not VK_COMPARE_OP_LESS since we have a depth prepass;
		create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
			{ componentDescriptorSetLayout, textureDescriptorSetLayout}, 
			raster_primary_visibility[renderpass].pipelineParameters, 
			renderpass, 0, 
			raster_primary_visibility[renderpass].pipelines, raster_primary_visibility[renderpass].pipelineLayout);
		raster_primary_visibility[renderpass].ready = true;
	}

	/* ------ SKYBOX  ------ */
	{
		skybox[renderpass] = RasterPipelineResources();
		skybox[renderpass].pipelineParameters.initialize(USED_PRIMARY_VISIBILITY_G_BUFFERS);

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/RasterPrograms/Skybox/vert.spv"));
		auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/RasterPrograms/Skybox/frag.spv"));

		/* Create shader modules */
		auto vertShaderModule = create_shader_module("skybox_vert", vertShaderCode);
		auto fragShaderModule = create_shader_module("skybox_frag", fragShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
		vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
		fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };

		/* Alpha channel used to store additional information */
		for (uint32_t gid = 0; gid < USED_PRIMARY_VISIBILITY_G_BUFFERS; gid++) {
			skybox[renderpass].pipelineParameters.blendAttachments[gid].blendEnable = false;
		}
		
		/* Skyboxes don't do back face culling. */
		skybox[renderpass].pipelineParameters.rasterizer.setCullMode(vk::CullModeFlagBits::eNone);

		create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
			{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
			skybox[renderpass].pipelineParameters, 
			renderpass, 0, 
			skybox[renderpass].pipelines, skybox[renderpass].pipelineLayout);

		skybox[renderpass].ready = true;
	}
}

void RenderSystem::setup_depth_prepass_raster_pipelines(vk::RenderPass renderpass, uint32_t sampleCount, bool use_depth_prepass, uint32_t num_g_buffers)
{
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	auto sampleFlag = vulkan->highest(vulkan->min(vulkan->get_closest_sample_count_flag(sampleCount), vulkan->get_msaa_sample_flags()));

	#ifndef DISABLE_REVERSE_Z
	auto depthCompareOp = vk::CompareOp::eGreater;
	auto depthCompareOpEqual = vk::CompareOp::eGreaterOrEqual;
	#else
	auto depthCompareOp = vk::CompareOp::eLess;
	auto depthCompareOpEqual = vk::CompareOp::eLessOrEqual;
	#endif

	/* ------ Primary Visibility Fragment Depth  ------ */
	{
		raster_depth_prepass[renderpass] = RasterPipelineResources();
		raster_depth_prepass[renderpass].pipelineParameters.initialize(num_g_buffers);

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/RasterPrograms/FragmentDepth/vert.spv"));

		/* Create shader modules */
		auto vertShaderModule = create_shader_module("frag_depth", vertShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
		vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo };
		
		raster_depth_prepass[renderpass].pipelineParameters.depthStencil.depthWriteEnable = true;
		raster_depth_prepass[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
		raster_depth_prepass[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;

		create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
			{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
			raster_depth_prepass[renderpass].pipelineParameters, 
			renderpass, 0, 
			raster_depth_prepass[renderpass].pipelines, raster_depth_prepass[renderpass].pipelineLayout);
		raster_depth_prepass[renderpass].ready = true;
	}

	/* ------ VR MASK ------ */
	{
		raster_vrmask[renderpass] = RasterPipelineResources();
		raster_vrmask[renderpass].pipelineParameters.initialize(num_g_buffers);

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/RasterPrograms/VRMask/vert.spv"));

		/* Create shader modules */
		auto vertShaderModule = create_shader_module("vr_mask", vertShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
		vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo };
		
		raster_vrmask[renderpass].pipelineParameters.depthStencil.depthWriteEnable = true;
		raster_vrmask[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
		raster_vrmask[renderpass].pipelineParameters.rasterizer.cullMode = vk::CullModeFlagBits::eNone;
		
		create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
			{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
			raster_vrmask[renderpass].pipelineParameters, 
			renderpass, 0, 
			raster_vrmask[renderpass].pipelines, raster_vrmask[renderpass].pipelineLayout);

		// device.destroyShaderModule(vertShaderModule);
		raster_vrmask[renderpass].ready = true;
	}
}

void RenderSystem::setup_shadow_map_raster_pipelines(vk::RenderPass renderpass, uint32_t sampleCount, bool use_depth_prepass)
{
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	auto sampleFlag = vulkan->highest(vulkan->min(vulkan->get_closest_sample_count_flag(sampleCount), vulkan->get_msaa_sample_flags()));

	#ifndef DISABLE_REVERSE_Z
	auto depthCompareOp = vk::CompareOp::eGreater;
	auto depthCompareOpEqual = vk::CompareOp::eGreaterOrEqual;
	#else
	auto depthCompareOp = vk::CompareOp::eLess;
	auto depthCompareOpEqual = vk::CompareOp::eLessOrEqual;
	#endif

	/* ------ SHADOWMAP  ------ */
	{
		shadowmap[renderpass] = RasterPipelineResources();
		shadowmap[renderpass].pipelineParameters.initialize(USED_SHADOW_MAP_G_BUFFERS);

		std::string ResourcePath = Options::GetResourcePath();
		auto vertShaderCode = readFile(ResourcePath + std::string("/Shaders/RasterPrograms/ShadowMap/vert.spv"));
		auto fragShaderCode = readFile(ResourcePath + std::string("/Shaders/RasterPrograms/ShadowMap/frag.spv"));

		/* Create shader modules */
		auto vertShaderModule = create_shader_module("shadow_map_vert", vertShaderCode);
		auto fragShaderModule = create_shader_module("shadow_map_frag", fragShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
		vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
		fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };
		
		/* Because we use a depth prepass */
		if (use_depth_prepass) {
			shadowmap[renderpass].pipelineParameters.depthStencil.depthTestEnable = true;
			shadowmap[renderpass].pipelineParameters.depthStencil.depthWriteEnable = false; // not VK_TRUE since we have a depth prepass
			shadowmap[renderpass].pipelineParameters.depthStencil.depthCompareOp = depthCompareOpEqual;
		}

		/* G Buffers */
		shadowmap[renderpass].pipelineParameters.blendAttachments[0].blendEnable = false;
		// shadowmap[renderpass].pipelineParameters.colorBlending.attachmentCount = (uint32_t)shadowmap[renderpass].pipelineParameters.blendAttachments.size();
		// shadowmap[renderpass].pipelineParameters.colorBlending.pAttachments = shadowmap[renderpass].pipelineParameters.blendAttachments.data();

		create_raster_pipeline(shaderStages, vertexInputBindingDescriptions, vertexInputAttributeDescriptions, 
			{ componentDescriptorSetLayout, textureDescriptorSetLayout }, 
			shadowmap[renderpass].pipelineParameters, 
			renderpass, 0, 
			shadowmap[renderpass].pipelines, shadowmap[renderpass].pipelineLayout);

		shadowmap[renderpass].ready = true;
	}
}

void RenderSystem::setup_raytracing_pipelines()
{
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();
	if (!vulkan->is_ray_tracing_enabled()) return;
	auto dldi = vulkan->get_dldi();
	auto rayTracingProps = vulkan->get_physical_device_ray_tracing_properties();

	/* RAY TRACING PIPELINES */
	{
		raytrace_primary_visibility = RaytracingPipelineResources();

		/* RAY GEN SHADERS */
		std::string ResourcePath = Options::GetResourcePath();
		auto raygenShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracePrograms/PrimaryVisibility/rgen.spv"));
		auto raychitShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracePrograms/PrimaryVisibility/rchit.spv"));
		auto raymissShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracePrograms/PrimaryVisibility/rmiss.spv"));
		
		/* Create shader modules */
		auto raygenShaderModule = create_shader_module("raytrace_primary_visibility_gen", raygenShaderCode);
		auto raychitShaderModule = create_shader_module("raytrace_primary_visibility_chit", raychitShaderCode);
		auto raymissShaderModule = create_shader_module("raytrace_primary_visibility_miss", raymissShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo raygenShaderStageInfo;
		raygenShaderStageInfo.stage = vk::ShaderStageFlagBits::eRaygenNV;
		raygenShaderStageInfo.module = raygenShaderModule;
		raygenShaderStageInfo.pName = "main"; 

		vk::PipelineShaderStageCreateInfo raymissShaderStageInfo;
		raymissShaderStageInfo.stage = vk::ShaderStageFlagBits::eMissNV;
		raymissShaderStageInfo.module = raymissShaderModule;
		raymissShaderStageInfo.pName = "main"; 

		vk::PipelineShaderStageCreateInfo raychitShaderStageInfo;
		raychitShaderStageInfo.stage = vk::ShaderStageFlagBits::eClosestHitNV;
		raychitShaderStageInfo.module = raychitShaderModule;
		raychitShaderStageInfo.pName = "main"; 

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { raygenShaderStageInfo, raymissShaderStageInfo, raychitShaderStageInfo};
		
		vk::PushConstantRange range;
		range.offset = 0;
		range.size = sizeof(PushConsts);
		range.stageFlags = vk::ShaderStageFlagBits::eAll;

		std::vector<vk::DescriptorSetLayout> layouts = { componentDescriptorSetLayout, textureDescriptorSetLayout, gbufferDescriptorSetLayout, 
			positionsDescriptorSetLayout, normalsDescriptorSetLayout, colorsDescriptorSetLayout, texcoordsDescriptorSetLayout, indexDescriptorSetLayout,
			raytracingDescriptorSetLayout };
		vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;

		pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)layouts.size();
		pipelineLayoutCreateInfo.pSetLayouts = layouts.data();
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &range;
		
		raytrace_primary_visibility.pipelineLayout = device.createPipelineLayout(pipelineLayoutCreateInfo);

		std::vector<vk::RayTracingShaderGroupCreateInfoNV> shaderGroups;
		
		// Ray gen group
		vk::RayTracingShaderGroupCreateInfoNV rayGenGroupInfo;
		rayGenGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eGeneral;
		rayGenGroupInfo.generalShader = 0;
		rayGenGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		rayGenGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayGenGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayGenGroupInfo);
		
		// Miss group
		vk::RayTracingShaderGroupCreateInfoNV rayMissGroupInfo;
		rayMissGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eGeneral;
		rayMissGroupInfo.generalShader = 1;
		rayMissGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		rayMissGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayMissGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayMissGroupInfo);
		
		// Intersection group
		vk::RayTracingShaderGroupCreateInfoNV rayCHitGroupInfo; //VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV
		rayCHitGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eTrianglesHitGroup;
		rayCHitGroupInfo.generalShader = VK_SHADER_UNUSED_NV;
		rayCHitGroupInfo.closestHitShader = 2;
		rayCHitGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayCHitGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayCHitGroupInfo);

		vk::RayTracingPipelineCreateInfoNV rayPipelineInfo;
		rayPipelineInfo.stageCount = (uint32_t) shaderStages.size();
		rayPipelineInfo.pStages = shaderStages.data();
		rayPipelineInfo.groupCount = (uint32_t) shaderGroups.size();
		rayPipelineInfo.pGroups = shaderGroups.data();
		rayPipelineInfo.maxRecursionDepth = vulkan->get_physical_device_ray_tracing_properties().maxRecursionDepth;
		rayPipelineInfo.layout = raytrace_primary_visibility.pipelineLayout;
		rayPipelineInfo.basePipelineHandle = vk::Pipeline();
		rayPipelineInfo.basePipelineIndex = 0;

		raytrace_primary_visibility.pipeline = device.createRayTracingPipelinesNV(vk::PipelineCache(), 
			{rayPipelineInfo}, nullptr, dldi)[0];

		const uint32_t groupNum = 3; // 1 group is listed in pGroupNumbers in VkRayTracingPipelineCreateInfoNV
		const uint32_t shaderBindingTableSize = rayTracingProps.shaderGroupHandleSize * groupNum;

		/* Create binding table buffer */
		vk::BufferCreateInfo bufferInfo;
		bufferInfo.size = shaderBindingTableSize;
		bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
		bufferInfo.sharingMode = vk::SharingMode::eExclusive;
		raytrace_primary_visibility.shaderBindingTable = device.createBuffer(bufferInfo);

		/* Create memory for binding table */
		vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(raytrace_primary_visibility.shaderBindingTable);
		vk::MemoryAllocateInfo allocInfo;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = vulkan->find_memory_type(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible);
		raytrace_primary_visibility.shaderBindingTableMemory = device.allocateMemory(allocInfo);

		/* Bind buffer to memeory */
		device.bindBufferMemory(raytrace_primary_visibility.shaderBindingTable, raytrace_primary_visibility.shaderBindingTableMemory, 0);

		/* Map the binding table, then fill with shader group handles */
		void* mappedMemory = device.mapMemory(raytrace_primary_visibility.shaderBindingTableMemory, 0, shaderBindingTableSize, vk::MemoryMapFlags());
		device.getRayTracingShaderGroupHandlesNV(raytrace_primary_visibility.pipeline, 0, groupNum, shaderBindingTableSize, mappedMemory, dldi);
		device.unmapMemory(raytrace_primary_visibility.shaderBindingTableMemory);

		raytrace_primary_visibility.ready = true;
	}

	{
		ddgi_path_tracer = RaytracingPipelineResources();

		/* RAY GEN SHADERS */
		std::string ResourcePath = Options::GetResourcePath();
		auto raygenShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracePrograms/DDGIPathTracer/rgen.spv"));
		auto raychitShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracePrograms/DDGIPathTracer/rchit.spv"));
		auto raymissShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracePrograms/DDGIPathTracer/rmiss.spv"));
		
		/* Create shader modules */
		auto raygenShaderModule = create_shader_module("ddgi_path_tracer_gen", raygenShaderCode);
		auto raychitShaderModule = create_shader_module("ddgi_path_tracer_chit", raychitShaderCode);
		auto raymissShaderModule = create_shader_module("ddgi_path_tracer_miss", raymissShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo raygenShaderStageInfo;
		raygenShaderStageInfo.stage = vk::ShaderStageFlagBits::eRaygenNV;
		raygenShaderStageInfo.module = raygenShaderModule;
		raygenShaderStageInfo.pName = "main"; 

		vk::PipelineShaderStageCreateInfo raymissShaderStageInfo;
		raymissShaderStageInfo.stage = vk::ShaderStageFlagBits::eMissNV;
		raymissShaderStageInfo.module = raymissShaderModule;
		raymissShaderStageInfo.pName = "main"; 

		vk::PipelineShaderStageCreateInfo raychitShaderStageInfo;
		raychitShaderStageInfo.stage = vk::ShaderStageFlagBits::eClosestHitNV;
		raychitShaderStageInfo.module = raychitShaderModule;
		raychitShaderStageInfo.pName = "main"; 

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { raygenShaderStageInfo, raymissShaderStageInfo, raychitShaderStageInfo};
		
		vk::PushConstantRange range;
		range.offset = 0;
		range.size = sizeof(PushConsts);
		range.stageFlags = vk::ShaderStageFlagBits::eAll;

		std::vector<vk::DescriptorSetLayout> layouts = { componentDescriptorSetLayout, textureDescriptorSetLayout, gbufferDescriptorSetLayout, 
			positionsDescriptorSetLayout, normalsDescriptorSetLayout, colorsDescriptorSetLayout, texcoordsDescriptorSetLayout, indexDescriptorSetLayout,
			raytracingDescriptorSetLayout };
		vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;

		pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)layouts.size();
		pipelineLayoutCreateInfo.pSetLayouts = layouts.data();
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &range;
		
		ddgi_path_tracer.pipelineLayout = device.createPipelineLayout(pipelineLayoutCreateInfo);

		std::vector<vk::RayTracingShaderGroupCreateInfoNV> shaderGroups;
		
		// Ray gen group
		vk::RayTracingShaderGroupCreateInfoNV rayGenGroupInfo;
		rayGenGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eGeneral;
		rayGenGroupInfo.generalShader = 0;
		rayGenGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		rayGenGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayGenGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayGenGroupInfo);
		
		// Miss group
		vk::RayTracingShaderGroupCreateInfoNV rayMissGroupInfo;
		rayMissGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eGeneral;
		rayMissGroupInfo.generalShader = 1;
		rayMissGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		rayMissGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayMissGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayMissGroupInfo);
		
		// Intersection group
		vk::RayTracingShaderGroupCreateInfoNV rayCHitGroupInfo; //VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV
		rayCHitGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eTrianglesHitGroup;
		rayCHitGroupInfo.generalShader = VK_SHADER_UNUSED_NV;
		rayCHitGroupInfo.closestHitShader = 2;
		rayCHitGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayCHitGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayCHitGroupInfo);

		vk::RayTracingPipelineCreateInfoNV rayPipelineInfo;
		rayPipelineInfo.stageCount = (uint32_t) shaderStages.size();
		rayPipelineInfo.pStages = shaderStages.data();
		rayPipelineInfo.groupCount = (uint32_t) shaderGroups.size();
		rayPipelineInfo.pGroups = shaderGroups.data();
		rayPipelineInfo.maxRecursionDepth = vulkan->get_physical_device_ray_tracing_properties().maxRecursionDepth;
		rayPipelineInfo.layout = ddgi_path_tracer.pipelineLayout;
		rayPipelineInfo.basePipelineHandle = vk::Pipeline();
		rayPipelineInfo.basePipelineIndex = 0;

		ddgi_path_tracer.pipeline = device.createRayTracingPipelinesNV(vk::PipelineCache(), 
			{rayPipelineInfo}, nullptr, dldi)[0];

		const uint32_t groupNum = 3; // 1 group is listed in pGroupNumbers in VkRayTracingPipelineCreateInfoNV
		const uint32_t shaderBindingTableSize = rayTracingProps.shaderGroupHandleSize * groupNum;

		/* Create binding table buffer */
		vk::BufferCreateInfo bufferInfo;
		bufferInfo.size = shaderBindingTableSize;
		bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
		bufferInfo.sharingMode = vk::SharingMode::eExclusive;
		ddgi_path_tracer.shaderBindingTable = device.createBuffer(bufferInfo);

		/* Create memory for binding table */
		vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(ddgi_path_tracer.shaderBindingTable);
		vk::MemoryAllocateInfo allocInfo;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = vulkan->find_memory_type(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible);
		ddgi_path_tracer.shaderBindingTableMemory = device.allocateMemory(allocInfo);

		/* Bind buffer to memeory */
		device.bindBufferMemory(ddgi_path_tracer.shaderBindingTable, ddgi_path_tracer.shaderBindingTableMemory, 0);

		/* Map the binding table, then fill with shader group handles */
		void* mappedMemory = device.mapMemory(ddgi_path_tracer.shaderBindingTableMemory, 0, shaderBindingTableSize, vk::MemoryMapFlags());
		device.getRayTracingShaderGroupHandlesNV(ddgi_path_tracer.pipeline, 0, groupNum, shaderBindingTableSize, mappedMemory, dldi);
		device.unmapMemory(ddgi_path_tracer.shaderBindingTableMemory);

		ddgi_path_tracer.ready = true;
	}

	{
		diffuse_path_tracer = RaytracingPipelineResources();

		/* RAY GEN SHADERS */
		std::string ResourcePath = Options::GetResourcePath();
		auto raygenShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracePrograms/DiffusePathTracer/rgen.spv"));
		auto raychitShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracePrograms/DiffusePathTracer/rchit.spv"));
		auto raymissShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracePrograms/DiffusePathTracer/rmiss.spv"));
		
		/* Create shader modules */
		auto raygenShaderModule = create_shader_module("diffuse_ray_tracing_gen", raygenShaderCode);
		auto raychitShaderModule = create_shader_module("diffuse_ray_tracing_chit", raychitShaderCode);
		auto raymissShaderModule = create_shader_module("diffuse_ray_tracing_miss", raymissShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo raygenShaderStageInfo;
		raygenShaderStageInfo.stage = vk::ShaderStageFlagBits::eRaygenNV;
		raygenShaderStageInfo.module = raygenShaderModule;
		raygenShaderStageInfo.pName = "main"; 

		vk::PipelineShaderStageCreateInfo raymissShaderStageInfo;
		raymissShaderStageInfo.stage = vk::ShaderStageFlagBits::eMissNV;
		raymissShaderStageInfo.module = raymissShaderModule;
		raymissShaderStageInfo.pName = "main"; 

		vk::PipelineShaderStageCreateInfo raychitShaderStageInfo;
		raychitShaderStageInfo.stage = vk::ShaderStageFlagBits::eClosestHitNV;
		raychitShaderStageInfo.module = raychitShaderModule;
		raychitShaderStageInfo.pName = "main"; 

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { raygenShaderStageInfo, raymissShaderStageInfo, raychitShaderStageInfo};
		
		vk::PushConstantRange range;
		range.offset = 0;
		range.size = sizeof(PushConsts);
		range.stageFlags = vk::ShaderStageFlagBits::eAll;

		std::vector<vk::DescriptorSetLayout> layouts = { componentDescriptorSetLayout, textureDescriptorSetLayout, gbufferDescriptorSetLayout, 
			positionsDescriptorSetLayout, normalsDescriptorSetLayout, colorsDescriptorSetLayout, texcoordsDescriptorSetLayout, indexDescriptorSetLayout,
			raytracingDescriptorSetLayout };
		vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;

		pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)layouts.size();
		pipelineLayoutCreateInfo.pSetLayouts = layouts.data();
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &range;
		
		diffuse_path_tracer.pipelineLayout = device.createPipelineLayout(pipelineLayoutCreateInfo);

		std::vector<vk::RayTracingShaderGroupCreateInfoNV> shaderGroups;
		
		// Ray gen group
		vk::RayTracingShaderGroupCreateInfoNV rayGenGroupInfo;
		rayGenGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eGeneral;
		rayGenGroupInfo.generalShader = 0;
		rayGenGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		rayGenGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayGenGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayGenGroupInfo);
		
		// Miss group
		vk::RayTracingShaderGroupCreateInfoNV rayMissGroupInfo;
		rayMissGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eGeneral;
		rayMissGroupInfo.generalShader = 1;
		rayMissGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		rayMissGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayMissGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayMissGroupInfo);
		
		// Intersection group
		vk::RayTracingShaderGroupCreateInfoNV rayCHitGroupInfo; //VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV
		rayCHitGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eTrianglesHitGroup;
		rayCHitGroupInfo.generalShader = VK_SHADER_UNUSED_NV;
		rayCHitGroupInfo.closestHitShader = 2;
		rayCHitGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayCHitGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayCHitGroupInfo);

		vk::RayTracingPipelineCreateInfoNV rayPipelineInfo;
		rayPipelineInfo.stageCount = (uint32_t) shaderStages.size();
		rayPipelineInfo.pStages = shaderStages.data();
		rayPipelineInfo.groupCount = (uint32_t) shaderGroups.size();
		rayPipelineInfo.pGroups = shaderGroups.data();
		rayPipelineInfo.maxRecursionDepth = vulkan->get_physical_device_ray_tracing_properties().maxRecursionDepth;
		rayPipelineInfo.layout = diffuse_path_tracer.pipelineLayout;
		rayPipelineInfo.basePipelineHandle = vk::Pipeline();
		rayPipelineInfo.basePipelineIndex = 0;

		diffuse_path_tracer.pipeline = device.createRayTracingPipelinesNV(vk::PipelineCache(), 
			{rayPipelineInfo}, nullptr, dldi)[0];

		const uint32_t groupNum = 3; // 1 group is listed in pGroupNumbers in VkRayTracingPipelineCreateInfoNV
		const uint32_t shaderBindingTableSize = rayTracingProps.shaderGroupHandleSize * groupNum;

		/* Create binding table buffer */
		vk::BufferCreateInfo bufferInfo;
		bufferInfo.size = shaderBindingTableSize;
		bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
		bufferInfo.sharingMode = vk::SharingMode::eExclusive;
		diffuse_path_tracer.shaderBindingTable = device.createBuffer(bufferInfo);

		/* Create memory for binding table */
		vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(diffuse_path_tracer.shaderBindingTable);
		vk::MemoryAllocateInfo allocInfo;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = vulkan->find_memory_type(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible);
		diffuse_path_tracer.shaderBindingTableMemory = device.allocateMemory(allocInfo);

		/* Bind buffer to memeory */
		device.bindBufferMemory(diffuse_path_tracer.shaderBindingTable, diffuse_path_tracer.shaderBindingTableMemory, 0);

		/* Map the binding table, then fill with shader group handles */
		void* mappedMemory = device.mapMemory(diffuse_path_tracer.shaderBindingTableMemory, 0, shaderBindingTableSize, vk::MemoryMapFlags());
		device.getRayTracingShaderGroupHandlesNV(diffuse_path_tracer.pipeline, 0, groupNum, shaderBindingTableSize, mappedMemory, dldi);
		device.unmapMemory(diffuse_path_tracer.shaderBindingTableMemory);

		diffuse_path_tracer.ready = true;
	}

	{
		specular_path_tracer = RaytracingPipelineResources();

		/* RAY GEN SHADERS */
		std::string ResourcePath = Options::GetResourcePath();
		auto raygenShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracePrograms/SpecularPathTracer/rgen.spv"));
		auto raychitShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracePrograms/SpecularPathTracer/rchit.spv"));
		auto raymissShaderCode = readFile(ResourcePath + std::string("/Shaders/RaytracePrograms/SpecularPathTracer/rmiss.spv"));
		
		/* Create shader modules */
		auto raygenShaderModule = create_shader_module("specular_ray_tracing_gen", raygenShaderCode);
		auto raychitShaderModule = create_shader_module("specular_ray_tracing_chit", raychitShaderCode);
		auto raymissShaderModule = create_shader_module("specular_ray_tracing_miss", raymissShaderCode);

		/* Info for shader stages */
		vk::PipelineShaderStageCreateInfo raygenShaderStageInfo;
		raygenShaderStageInfo.stage = vk::ShaderStageFlagBits::eRaygenNV;
		raygenShaderStageInfo.module = raygenShaderModule;
		raygenShaderStageInfo.pName = "main"; 

		vk::PipelineShaderStageCreateInfo raymissShaderStageInfo;
		raymissShaderStageInfo.stage = vk::ShaderStageFlagBits::eMissNV;
		raymissShaderStageInfo.module = raymissShaderModule;
		raymissShaderStageInfo.pName = "main"; 

		vk::PipelineShaderStageCreateInfo raychitShaderStageInfo;
		raychitShaderStageInfo.stage = vk::ShaderStageFlagBits::eClosestHitNV;
		raychitShaderStageInfo.module = raychitShaderModule;
		raychitShaderStageInfo.pName = "main"; 

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { raygenShaderStageInfo, raymissShaderStageInfo, raychitShaderStageInfo};
		
		vk::PushConstantRange range;
		range.offset = 0;
		range.size = sizeof(PushConsts);
		range.stageFlags = vk::ShaderStageFlagBits::eAll;

		std::vector<vk::DescriptorSetLayout> layouts = { componentDescriptorSetLayout, textureDescriptorSetLayout, gbufferDescriptorSetLayout, 
			positionsDescriptorSetLayout, normalsDescriptorSetLayout, colorsDescriptorSetLayout, texcoordsDescriptorSetLayout, indexDescriptorSetLayout,
			raytracingDescriptorSetLayout };
		vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;

		pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)layouts.size();
		pipelineLayoutCreateInfo.pSetLayouts = layouts.data();
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &range;
		
		specular_path_tracer.pipelineLayout = device.createPipelineLayout(pipelineLayoutCreateInfo);

		std::vector<vk::RayTracingShaderGroupCreateInfoNV> shaderGroups;
		
		// Ray gen group
		vk::RayTracingShaderGroupCreateInfoNV rayGenGroupInfo;
		rayGenGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eGeneral;
		rayGenGroupInfo.generalShader = 0;
		rayGenGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		rayGenGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayGenGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayGenGroupInfo);
		
		// Miss group
		vk::RayTracingShaderGroupCreateInfoNV rayMissGroupInfo;
		rayMissGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eGeneral;
		rayMissGroupInfo.generalShader = 1;
		rayMissGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		rayMissGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayMissGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayMissGroupInfo);
		
		// Intersection group
		vk::RayTracingShaderGroupCreateInfoNV rayCHitGroupInfo; //VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV
		rayCHitGroupInfo.type = vk::RayTracingShaderGroupTypeNV::eTrianglesHitGroup;
		rayCHitGroupInfo.generalShader = VK_SHADER_UNUSED_NV;
		rayCHitGroupInfo.closestHitShader = 2;
		rayCHitGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		rayCHitGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		shaderGroups.push_back(rayCHitGroupInfo);

		vk::RayTracingPipelineCreateInfoNV rayPipelineInfo;
		rayPipelineInfo.stageCount = (uint32_t) shaderStages.size();
		rayPipelineInfo.pStages = shaderStages.data();
		rayPipelineInfo.groupCount = (uint32_t) shaderGroups.size();
		rayPipelineInfo.pGroups = shaderGroups.data();
		rayPipelineInfo.maxRecursionDepth = vulkan->get_physical_device_ray_tracing_properties().maxRecursionDepth;
		rayPipelineInfo.layout = specular_path_tracer.pipelineLayout;
		rayPipelineInfo.basePipelineHandle = vk::Pipeline();
		rayPipelineInfo.basePipelineIndex = 0;

		specular_path_tracer.pipeline = device.createRayTracingPipelinesNV(vk::PipelineCache(), 
			{rayPipelineInfo}, nullptr, dldi)[0];

		const uint32_t groupNum = 3; // 1 group is listed in pGroupNumbers in VkRayTracingPipelineCreateInfoNV
		const uint32_t shaderBindingTableSize = rayTracingProps.shaderGroupHandleSize * groupNum;

		/* Create binding table buffer */
		vk::BufferCreateInfo bufferInfo;
		bufferInfo.size = shaderBindingTableSize;
		bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
		bufferInfo.sharingMode = vk::SharingMode::eExclusive;
		specular_path_tracer.shaderBindingTable = device.createBuffer(bufferInfo);

		/* Create memory for binding table */
		vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(specular_path_tracer.shaderBindingTable);
		vk::MemoryAllocateInfo allocInfo;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = vulkan->find_memory_type(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible);
		specular_path_tracer.shaderBindingTableMemory = device.allocateMemory(allocInfo);

		/* Bind buffer to memeory */
		device.bindBufferMemory(specular_path_tracer.shaderBindingTable, specular_path_tracer.shaderBindingTableMemory, 0);

		/* Map the binding table, then fill with shader group handles */
		void* mappedMemory = device.mapMemory(specular_path_tracer.shaderBindingTableMemory, 0, shaderBindingTableSize, vk::MemoryMapFlags());
		device.getRayTracingShaderGroupHandlesNV(specular_path_tracer.pipeline, 0, groupNum, shaderBindingTableSize, mappedMemory, dldi);
		device.unmapMemory(specular_path_tracer.shaderBindingTableMemory);

		specular_path_tracer.ready = true;
	}
}

void RenderSystem::setup_compute_pipelines()
{
    auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();
	std::string ResourcePath = Options::GetResourcePath();

	vk::PushConstantRange range;
	range.offset = 0;
	range.size = sizeof(PushConsts);
	range.stageFlags = vk::ShaderStageFlagBits::eAll;

	/* Info for shader stages */
	vk::PipelineShaderStageCreateInfo compShaderStageInfo;
	compShaderStageInfo.stage = vk::ShaderStageFlagBits::eCompute;
	compShaderStageInfo.pName = "main";

    std::vector<vk::DescriptorSetLayout> cameraComputeLayouts = { componentDescriptorSetLayout, textureDescriptorSetLayout, gbufferDescriptorSetLayout };
	vk::PipelineLayoutCreateInfo cameraComputeCreateInfo;
	cameraComputeCreateInfo.pushConstantRangeCount = 1;
	cameraComputeCreateInfo.pPushConstantRanges = &range;
	cameraComputeCreateInfo.setLayoutCount = (uint32_t)cameraComputeLayouts.size();
	cameraComputeCreateInfo.pSetLayouts = cameraComputeLayouts.data();

    std::vector<vk::DescriptorSetLayout> globalComputeLayouts = { componentDescriptorSetLayout, textureDescriptorSetLayout };
	vk::PipelineLayoutCreateInfo globalComputeCreateInfo;
	globalComputeCreateInfo.pushConstantRangeCount = 1;
	globalComputeCreateInfo.pPushConstantRanges = &range;
	globalComputeCreateInfo.setLayoutCount = (uint32_t)globalComputeLayouts.size();
	globalComputeCreateInfo.pSetLayouts = globalComputeLayouts.data();

    vk::ComputePipelineCreateInfo computeCreateInfo;

	
	// GLOBAL COMPUTE SHADERS

	/* DDGI Blend Irradiance*/
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/DDGI_Blend_Irradiance/comp.spv"));
		auto compShaderModule = create_shader_module("ddgi_blend_irradiance", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        ddgi_blend_irradiance.pipelineLayout = device.createPipelineLayout(globalComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = ddgi_blend_irradiance.pipelineLayout;
        ddgi_blend_irradiance.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		ddgi_blend_irradiance.ready = true;
	}

	/* DDGI Blend Visibility */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/DDGI_Blend_Visibility/comp.spv"));
		auto compShaderModule = create_shader_module("ddgi_blend_visibility", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        ddgi_blend_visibility.pipelineLayout = device.createPipelineLayout(globalComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = ddgi_blend_visibility.pipelineLayout;
        ddgi_blend_visibility.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		ddgi_blend_visibility.ready = true;
	}


	// PER CAMERA FX COMPUTE SHADERS
    
	/* ------ Deferred Final Pass  ------ */
    {
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/DeferredFinal/comp.spv"));
		auto compShaderModule = create_shader_module("deferred_final", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        deferred_final.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = deferred_final.pipelineLayout;
        deferred_final.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		deferred_final.ready = true;
	}

	/* ------ Edge Detection  ------ */
    {
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/EdgeDetect/comp.spv"));
		auto compShaderModule = create_shader_module("edge_detect", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        edgedetect.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = edgedetect.pipelineLayout;
        edgedetect.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		edgedetect.ready = true;
	}

	/* Progressive refinement */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/ProgressiveRefinement/comp.spv"));
		auto compShaderModule = create_shader_module("progressive_refinement", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        progressive_refinement.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = progressive_refinement.pipelineLayout;
        progressive_refinement.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		progressive_refinement.ready = true;
	}

	/* Tone Mapping */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/Compositing/comp.spv"));
		auto compShaderModule = create_shader_module("compositing", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        compositing.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = compositing.pipelineLayout;
        compositing.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		compositing.ready = true;
	}

	/* Copy History */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/CopyHistory/comp.spv"));
		auto compShaderModule = create_shader_module("copy_history", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        copy_history.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = copy_history.pipelineLayout;
        copy_history.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		copy_history.ready = true;
	}

	/* Gaussian X */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/VSM/GaussianBlurX/comp.spv"));
		auto compShaderModule = create_shader_module("gaussian_x", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        gaussian_x.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = gaussian_x.pipelineLayout;
        gaussian_x.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		gaussian_x.ready = true;
	}

	/* Gaussian Y */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/VSM/GaussianBlurY/comp.spv"));
		auto compShaderModule = create_shader_module("gaussian_y", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        gaussian_y.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = gaussian_y.pipelineLayout;
        gaussian_y.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		gaussian_y.ready = true;
	}

	/* Median 3x3 */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/Median3x3/comp.spv"));
		auto compShaderModule = create_shader_module("median_3x3", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        median_3x3.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = median_3x3.pipelineLayout;
        median_3x3.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		median_3x3.ready = true;
	}

	/* Median 5x5 */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/Median5x5/comp.spv"));
		auto compShaderModule = create_shader_module("median_5x5", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        median_5x5.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = median_5x5.pipelineLayout;
        median_5x5.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		median_5x5.ready = true;
	}

	/* Bilateral Upsampler */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/BilateralUpsample/comp.spv"));
		auto compShaderModule = create_shader_module("bilateral_upsample", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        bilateral_upsample.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = bilateral_upsample.pipelineLayout;
        bilateral_upsample.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		bilateral_upsample.ready = true;
	}

	/* ------ Temporal Antialiasing  ------ */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/TAA/comp.spv"));
		auto compShaderModule = create_shader_module("taa", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        taa.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = taa.pipelineLayout;
        taa.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		taa.ready = true;
	}

	/* ------ ASVGF Diffuse Temporal Accumulation  ------ */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/ASVGF_4_DiffuseTemporalAccumulation/comp.spv"));
		auto compShaderModule = create_shader_module("asvgf_diffuse_temporal_accumulation", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        asvgf_diffuse_temporal_accumulation.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = asvgf_diffuse_temporal_accumulation.pipelineLayout;
        asvgf_diffuse_temporal_accumulation.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		asvgf_diffuse_temporal_accumulation.ready = true;
	}

	/* ------ ASVGF Specular Temporal Accumulation  ------ */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/ASVGF_4_SpecularTemporalAccumulation/comp.spv"));
		auto compShaderModule = create_shader_module("asvgf_specular_temporal_accumulation", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        asvgf_specular_temporal_accumulation.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = asvgf_specular_temporal_accumulation.pipelineLayout;
        asvgf_specular_temporal_accumulation.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		asvgf_specular_temporal_accumulation.ready = true;
	}

	/* Denoiser - Reconstruct Diffuse */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/ASVGF_6_ReconstructDiffuse/comp.spv"));
		auto compShaderModule = create_shader_module("reconstruct_diffuse", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        reconstruct_diffuse.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = reconstruct_diffuse.pipelineLayout;
        reconstruct_diffuse.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		reconstruct_diffuse.ready = true;
	}

	/* Denoiser - Reconstruct Glossy */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/ASVGF_6_ReconstructGlossy/comp.spv"));
		auto compShaderModule = create_shader_module("reconstruct_glossy", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        reconstruct_glossy.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = reconstruct_glossy.pipelineLayout;
        reconstruct_glossy.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		reconstruct_glossy.ready = true;
	}

	/* SVGF Remodulate */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/ASVGF_7_Remodulate/comp.spv"));
		auto compShaderModule = create_shader_module("svgf_remodulate", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        svgf_remodulate.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = svgf_remodulate.pipelineLayout;
        svgf_remodulate.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		svgf_remodulate.ready = true;
	}

	/* ASVGF Compute Sparse Gradient */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/ASVGF_2_ComputeGradientAndMoments/comp.spv"));
		auto compShaderModule = create_shader_module("asvgf_compute_gradient", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        asvgf_compute_gradient.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = asvgf_compute_gradient.pipelineLayout;
        asvgf_compute_gradient.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		asvgf_compute_gradient.ready = true;
	}

	/* ASVGF Reconstruct Dense Gradient */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/ASVGF_3_ReconstructGradientAndMoments/comp.spv"));
		auto compShaderModule = create_shader_module("asvgf_reconstruct_gradient", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        asvgf_reconstruct_gradient.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = asvgf_reconstruct_gradient.pipelineLayout;
        asvgf_reconstruct_gradient.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		asvgf_reconstruct_gradient.ready = true;
	}

	/* Fill Diffuse Disocclusions */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/ASVGF_5_FillDiffuseHoles/comp.spv"));
		auto compShaderModule = create_shader_module("asvgf_fill_diffuse_holes", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        asvgf_fill_diffuse_holes.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = asvgf_fill_diffuse_holes.pipelineLayout;
        asvgf_fill_diffuse_holes.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		asvgf_fill_diffuse_holes.ready = true;
	}

	/* Fill Specular Disocclusions */
	{
		auto compShaderCode = readFile(ResourcePath + std::string("/Shaders/ComputePrograms/ASVGF_5_FillSpecularHoles/comp.spv"));
		auto compShaderModule = create_shader_module("asvgf_fill_specular_holes", compShaderCode);
		compShaderStageInfo.module = compShaderModule;
        asvgf_fill_specular_holes.pipelineLayout = device.createPipelineLayout(cameraComputeCreateInfo);
        computeCreateInfo.stage = compShaderStageInfo;
        computeCreateInfo.layout = asvgf_fill_specular_holes.pipelineLayout;
        asvgf_fill_specular_holes.pipeline = device.createComputePipeline(vk::PipelineCache(), computeCreateInfo);
		asvgf_fill_specular_holes.ready = true;
	}
}

void RenderSystem::create_descriptor_set_layouts()
{
	/* Descriptor set layouts are standardized across shaders for optimized runtime binding */

	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	/* SSBO descriptor bindings */

	// Entity SSBO
	vk::DescriptorSetLayoutBinding eboLayoutBinding;
	eboLayoutBinding.binding = 0;
	eboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	eboLayoutBinding.descriptorCount = 1;
	eboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	eboLayoutBinding.pImmutableSamplers = nullptr; // Optional

	// Transform SSBO
	vk::DescriptorSetLayoutBinding tboLayoutBinding;
	tboLayoutBinding.binding = 1;
	tboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	tboLayoutBinding.descriptorCount = 1;
	tboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	tboLayoutBinding.pImmutableSamplers = nullptr; // Optional

	// Camera SSBO
	vk::DescriptorSetLayoutBinding cboLayoutBinding;
	cboLayoutBinding.binding = 2;
	cboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	cboLayoutBinding.descriptorCount = 1;
	cboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	cboLayoutBinding.pImmutableSamplers = nullptr; // Optional

	// Material SSBO
	vk::DescriptorSetLayoutBinding mboLayoutBinding;
	mboLayoutBinding.binding = 3;
	mboLayoutBinding.descriptorCount = 1;
	mboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	mboLayoutBinding.pImmutableSamplers = nullptr;
	mboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	// Light SSBO
	vk::DescriptorSetLayoutBinding lboLayoutBinding;
	lboLayoutBinding.binding = 4;
	lboLayoutBinding.descriptorCount = 1;
	lboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	lboLayoutBinding.pImmutableSamplers = nullptr;
	lboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	// Mesh SSBO
	vk::DescriptorSetLayoutBinding meshssboLayoutBinding;
	meshssboLayoutBinding.binding = 5;
	meshssboLayoutBinding.descriptorCount = 1;
	meshssboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	meshssboLayoutBinding.pImmutableSamplers = nullptr;
	meshssboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	// Light Entity SSBO
	vk::DescriptorSetLayoutBinding lidboLayoutBinding;
	lidboLayoutBinding.binding = 6;
	lidboLayoutBinding.descriptorCount = 1;
	lidboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	lidboLayoutBinding.pImmutableSamplers = nullptr;
	lidboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

	std::array<vk::DescriptorSetLayoutBinding, 7> SSBObindings = { eboLayoutBinding, tboLayoutBinding, cboLayoutBinding, mboLayoutBinding, lboLayoutBinding, meshssboLayoutBinding, lidboLayoutBinding};
	vk::DescriptorSetLayoutCreateInfo SSBOLayoutInfo;
	SSBOLayoutInfo.bindingCount = (uint32_t)SSBObindings.size();
	SSBOLayoutInfo.pBindings = SSBObindings.data();
	
	/* Texture descriptor bindings */
	
	// Texture struct
	vk::DescriptorSetLayoutBinding txboLayoutBinding;
	txboLayoutBinding.descriptorCount = 1;
	txboLayoutBinding.binding = 0;
	txboLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	txboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	txboLayoutBinding.pImmutableSamplers = 0;

	// Texture samplers
	vk::DescriptorSetLayoutBinding samplerBinding;
	samplerBinding.descriptorCount = MAX_SAMPLERS;
	samplerBinding.binding = 1;
	samplerBinding.descriptorType = vk::DescriptorType::eSampler;
	samplerBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	samplerBinding.pImmutableSamplers = 0;

	// 2D Textures
	vk::DescriptorSetLayoutBinding texture2DsBinding;
	texture2DsBinding.descriptorCount = MAX_TEXTURES;
	texture2DsBinding.binding = 2;
	texture2DsBinding.descriptorType = vk::DescriptorType::eSampledImage;
	texture2DsBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	texture2DsBinding.pImmutableSamplers = 0;

	// Texture Cubes
	vk::DescriptorSetLayoutBinding textureCubesBinding;
	textureCubesBinding.descriptorCount = MAX_TEXTURES;
	textureCubesBinding.binding = 3;
	textureCubesBinding.descriptorType = vk::DescriptorType::eSampledImage;
	textureCubesBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	textureCubesBinding.pImmutableSamplers = 0;

	// 3D Textures
	vk::DescriptorSetLayoutBinding texture3DsBinding;
	texture3DsBinding.descriptorCount = MAX_TEXTURES;
	texture3DsBinding.binding = 4;
	texture3DsBinding.descriptorType = vk::DescriptorType::eSampledImage;
	texture3DsBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	texture3DsBinding.pImmutableSamplers = 0;

	// Blue Noise Tile
	vk::DescriptorSetLayoutBinding blueNoiseBinding;
	blueNoiseBinding.descriptorCount = 1;
	blueNoiseBinding.binding = 5;
	blueNoiseBinding.descriptorType = vk::DescriptorType::eSampledImage;
	blueNoiseBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	blueNoiseBinding.pImmutableSamplers = 0;

	// BRDF LUT
	vk::DescriptorSetLayoutBinding brdfLUTBinding;
	brdfLUTBinding.descriptorCount = 1;
	brdfLUTBinding.binding = 6;
	brdfLUTBinding.descriptorType = vk::DescriptorType::eSampledImage;
	brdfLUTBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	brdfLUTBinding.pImmutableSamplers = 0;

	// LTC MAT
	vk::DescriptorSetLayoutBinding ltcMatBinding;
	ltcMatBinding.descriptorCount = 1;
	ltcMatBinding.binding = 7;
	ltcMatBinding.descriptorType = vk::DescriptorType::eSampledImage;
	ltcMatBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	ltcMatBinding.pImmutableSamplers = 0;

	// LTC AMP
	vk::DescriptorSetLayoutBinding ltcAmpBinding;
	ltcAmpBinding.descriptorCount = 1;
	ltcAmpBinding.binding = 8;
	ltcAmpBinding.descriptorType = vk::DescriptorType::eSampledImage;
	ltcAmpBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	ltcAmpBinding.pImmutableSamplers = 0;

	// Environment
	vk::DescriptorSetLayoutBinding environmentBinding;
	environmentBinding.descriptorCount = 1;
	environmentBinding.binding = 9;
	environmentBinding.descriptorType = vk::DescriptorType::eSampledImage;
	environmentBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	environmentBinding.pImmutableSamplers = 0;

	// Specular Environment
	vk::DescriptorSetLayoutBinding specularEnvironmentBinding;
	specularEnvironmentBinding.descriptorCount = 1;
	specularEnvironmentBinding.binding = 10;
	specularEnvironmentBinding.descriptorType = vk::DescriptorType::eSampledImage;
	specularEnvironmentBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	specularEnvironmentBinding.pImmutableSamplers = 0;

	// Diffuse Environment
	vk::DescriptorSetLayoutBinding diffuseEnvironmentBinding;
	diffuseEnvironmentBinding.descriptorCount = 1;
	diffuseEnvironmentBinding.binding = 11;
	diffuseEnvironmentBinding.descriptorType = vk::DescriptorType::eSampledImage;
	diffuseEnvironmentBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	diffuseEnvironmentBinding.pImmutableSamplers = 0;

	// DDGI Irradiance Tile
	vk::DescriptorSetLayoutBinding ddgiIrradianceBinding;
	ddgiIrradianceBinding.descriptorCount = 1;
	ddgiIrradianceBinding.binding = 12;
	ddgiIrradianceBinding.descriptorType = vk::DescriptorType::eStorageImage;
	ddgiIrradianceBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	ddgiIrradianceBinding.pImmutableSamplers = 0;

	// DDGI Visibility Tile
	vk::DescriptorSetLayoutBinding ddgiVisibilityBinding;
	ddgiVisibilityBinding.descriptorCount = 1;
	ddgiVisibilityBinding.binding = 13;
	ddgiVisibilityBinding.descriptorType = vk::DescriptorType::eStorageImage;
	ddgiVisibilityBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute 
		| vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
	ddgiVisibilityBinding.pImmutableSamplers = 0;

	std::array<vk::DescriptorSetLayoutBinding, 14> textureBindings = {
		txboLayoutBinding, samplerBinding, texture2DsBinding, textureCubesBinding, 
		texture3DsBinding, blueNoiseBinding, brdfLUTBinding, ltcMatBinding, ltcAmpBinding, 
		environmentBinding, specularEnvironmentBinding, diffuseEnvironmentBinding,
		ddgiIrradianceBinding, ddgiVisibilityBinding
	};
	vk::DescriptorSetLayoutCreateInfo textureLayoutInfo;
	textureLayoutInfo.bindingCount = (uint32_t)textureBindings.size();
	textureLayoutInfo.pBindings = textureBindings.data();

    // G Buffers
    vk::DescriptorSetLayoutBinding outputImageLayoutBinding;
    outputImageLayoutBinding.binding = 0;
    outputImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
    outputImageLayoutBinding.descriptorCount = 1;
    outputImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
    outputImageLayoutBinding.pImmutableSamplers = nullptr;

    vk::DescriptorSetLayoutBinding gbufferImageLayoutBinding;
    gbufferImageLayoutBinding.binding = 1;
    gbufferImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
    gbufferImageLayoutBinding.descriptorCount = MAX_G_BUFFERS;
    gbufferImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
    gbufferImageLayoutBinding.pImmutableSamplers = nullptr;

	vk::DescriptorSetLayoutBinding gbufferTextureImageLayoutBinding;
    gbufferTextureImageLayoutBinding.binding = 2;
    gbufferTextureImageLayoutBinding.descriptorType = vk::DescriptorType::eSampledImage;
    gbufferTextureImageLayoutBinding.descriptorCount = MAX_G_BUFFERS;
    gbufferTextureImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
    gbufferTextureImageLayoutBinding.pImmutableSamplers = nullptr;
    
    std::array<vk::DescriptorSetLayoutBinding, 3> gbufferBindings = {outputImageLayoutBinding, gbufferImageLayoutBinding, gbufferTextureImageLayoutBinding};
    vk::DescriptorSetLayoutCreateInfo gbufferLayoutInfo;
    gbufferLayoutInfo.bindingCount = (uint32_t)gbufferBindings.size();
	gbufferLayoutInfo.pBindings = gbufferBindings.data();

	// Create the layouts
	componentDescriptorSetLayout = device.createDescriptorSetLayout(SSBOLayoutInfo);
	textureDescriptorSetLayout = device.createDescriptorSetLayout(textureLayoutInfo);
	gbufferDescriptorSetLayout = device.createDescriptorSetLayout(gbufferLayoutInfo);

	if (vulkan->is_ray_tracing_enabled()) {
        /* Vertex descriptors (mainly for ray tracing access) */
        vk::DescriptorBindingFlagsEXT bindingFlag = vk::DescriptorBindingFlagBitsEXT::eVariableDescriptorCount;
        vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags;
        bindingFlags.pBindingFlags = &bindingFlag;
        bindingFlags.bindingCount = 1;

        vk::DescriptorSetLayoutBinding positionBinding;
        positionBinding.binding = 0;
        positionBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        positionBinding.descriptorCount = MAX_MESHES;
        positionBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

        vk::DescriptorSetLayoutBinding normalBinding;
        normalBinding.binding = 0;
        normalBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        normalBinding.descriptorCount = MAX_MESHES;
        normalBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

        vk::DescriptorSetLayoutBinding colorBinding;
        colorBinding.binding = 0;
        colorBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        colorBinding.descriptorCount = MAX_MESHES;
        colorBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

        vk::DescriptorSetLayoutBinding texcoordBinding;
        texcoordBinding.binding = 0;
        texcoordBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        texcoordBinding.descriptorCount = MAX_MESHES;
        texcoordBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;

        vk::DescriptorSetLayoutBinding indexBinding;
        indexBinding.binding = 0;
        indexBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
        indexBinding.descriptorCount = MAX_MESHES;
        indexBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
        
        // std::array<vk::DescriptorSetLayoutBinding, 5> vertexBindings = {positionBinding, normalBinding, colorBinding, texcoordBinding, indexBinding };

		vk::DescriptorSetLayoutCreateInfo positionLayoutInfo;
		positionLayoutInfo.bindingCount = 1;
		positionLayoutInfo.pBindings = &positionBinding;
		positionLayoutInfo.pNext = &bindingFlags;
		positionsDescriptorSetLayout = device.createDescriptorSetLayout(positionLayoutInfo);

		vk::DescriptorSetLayoutCreateInfo normalLayoutInfo;
		normalLayoutInfo.bindingCount = 1;
		normalLayoutInfo.pBindings = &normalBinding;
		normalLayoutInfo.pNext = &bindingFlags;
		normalsDescriptorSetLayout = device.createDescriptorSetLayout(normalLayoutInfo);

		vk::DescriptorSetLayoutCreateInfo colorLayoutInfo;
		colorLayoutInfo.bindingCount = 1;
		colorLayoutInfo.pBindings = &colorBinding;
		colorLayoutInfo.pNext = &bindingFlags;
		colorsDescriptorSetLayout = device.createDescriptorSetLayout(colorLayoutInfo);

		vk::DescriptorSetLayoutCreateInfo texcoordLayoutInfo;
		texcoordLayoutInfo.bindingCount = 1;
		texcoordLayoutInfo.pBindings = &texcoordBinding;
		texcoordLayoutInfo.pNext = &bindingFlags;
		texcoordsDescriptorSetLayout = device.createDescriptorSetLayout(texcoordLayoutInfo);

		vk::DescriptorSetLayoutCreateInfo indexLayoutInfo;
		indexLayoutInfo.bindingCount = 1;
		indexLayoutInfo.pBindings = &indexBinding;
		indexLayoutInfo.pNext = &bindingFlags;
		indexDescriptorSetLayout = device.createDescriptorSetLayout(indexLayoutInfo);

		vk::DescriptorSetLayoutBinding accelerationStructureLayoutBinding;
		accelerationStructureLayoutBinding.binding = 0;
		accelerationStructureLayoutBinding.descriptorType = vk::DescriptorType::eAccelerationStructureNV;
		accelerationStructureLayoutBinding.descriptorCount = 1;
		accelerationStructureLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV | vk::ShaderStageFlagBits::eClosestHitNV | vk::ShaderStageFlagBits::eMissNV;
		accelerationStructureLayoutBinding.pImmutableSamplers = nullptr;

		std::vector<vk::DescriptorSetLayoutBinding> bindings({ accelerationStructureLayoutBinding });

		vk::DescriptorSetLayoutCreateInfo layoutInfo;
		layoutInfo.bindingCount = (uint32_t)(bindings.size());
		layoutInfo.pBindings = bindings.data();

		raytracingDescriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
	}
}

void RenderSystem::create_descriptor_pools()
{
	/* Since the descriptor layout is consistent across shaders, the descriptor
		pool can be shared. */

	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	/* SSBO Descriptor Pool Info */
	std::array<vk::DescriptorPoolSize, 6> SSBOPoolSizes = {};
	
	// Entity SSBO
	SSBOPoolSizes[0].type = vk::DescriptorType::eStorageBuffer;
	SSBOPoolSizes[0].descriptorCount = MAX_MATERIALS;
	
	// Transform SSBO
	SSBOPoolSizes[1].type = vk::DescriptorType::eStorageBuffer;
	SSBOPoolSizes[1].descriptorCount = MAX_MATERIALS;
	
	// Camera SSBO
	SSBOPoolSizes[2].type = vk::DescriptorType::eStorageBuffer;
	SSBOPoolSizes[2].descriptorCount = MAX_MATERIALS;
	
	// Material SSBO
	SSBOPoolSizes[3].type = vk::DescriptorType::eStorageBuffer;
	SSBOPoolSizes[3].descriptorCount = MAX_MATERIALS;
	
	// Light SSBO
	SSBOPoolSizes[4].type = vk::DescriptorType::eStorageBuffer;
	SSBOPoolSizes[4].descriptorCount = MAX_MATERIALS;

	// Mesh SSBO
	SSBOPoolSizes[5].type = vk::DescriptorType::eStorageBuffer;
	SSBOPoolSizes[5].descriptorCount = MAX_MESHES;

	vk::DescriptorPoolCreateInfo SSBOPoolInfo;
	SSBOPoolInfo.poolSizeCount = (uint32_t)SSBOPoolSizes.size();
	SSBOPoolInfo.pPoolSizes = SSBOPoolSizes.data();
	SSBOPoolInfo.maxSets = MAX_MATERIALS;
	SSBOPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	/* Texture Descriptor Pool Info */
	std::array<vk::DescriptorPoolSize, 6> texturePoolSizes = {};
	
	// TextureSSBO
	texturePoolSizes[0].type = vk::DescriptorType::eStorageBuffer;
	texturePoolSizes[0].descriptorCount = MAX_MATERIALS;

	// Sampler
	texturePoolSizes[1].type = vk::DescriptorType::eSampler;
	texturePoolSizes[1].descriptorCount = MAX_MATERIALS;
	
	// 2D Texture array
	texturePoolSizes[2].type = vk::DescriptorType::eSampledImage;
	texturePoolSizes[2].descriptorCount = MAX_MATERIALS;

	// Texture Cube array
	texturePoolSizes[3].type = vk::DescriptorType::eSampledImage;
	texturePoolSizes[3].descriptorCount = MAX_MATERIALS;

	// 3D Texture array
	texturePoolSizes[4].type = vk::DescriptorType::eSampledImage;
	texturePoolSizes[4].descriptorCount = MAX_MATERIALS;

	texturePoolSizes[5].type = vk::DescriptorType::eSampledImage;
	texturePoolSizes[5].descriptorCount = 1;
	
	vk::DescriptorPoolCreateInfo texturePoolInfo;
	texturePoolInfo.poolSizeCount = (uint32_t)texturePoolSizes.size();
	texturePoolInfo.pPoolSizes = texturePoolSizes.data();
	texturePoolInfo.maxSets = MAX_MATERIALS;
	texturePoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

    /* G Buffer Pool Info */
    std::array<vk::DescriptorPoolSize, 2> gbufferPoolSizes = {};
    
    // Output texture
	gbufferPoolSizes[0].type = vk::DescriptorType::eStorageImage;
	gbufferPoolSizes[0].descriptorCount = 1;

	// G Buffers
	gbufferPoolSizes[1].type = vk::DescriptorType::eStorageImage;
	gbufferPoolSizes[1].descriptorCount = MAX_G_BUFFERS;

    vk::DescriptorPoolCreateInfo gbufferPoolInfo;
	gbufferPoolInfo.poolSizeCount = (uint32_t)gbufferPoolSizes.size();
	gbufferPoolInfo.pPoolSizes = gbufferPoolSizes.data();
	gbufferPoolInfo.maxSets = (MAX_G_BUFFERS + 1); // up to 6 views per camera
	gbufferPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	/* Vertex Descriptor Pool Info */

	// PositionSSBO
	vk::DescriptorPoolSize positionsPoolSize;
	positionsPoolSize.type = vk::DescriptorType::eStorageBuffer;
	positionsPoolSize.descriptorCount = MAX_MESHES;

	// NormalSSBO
	vk::DescriptorPoolSize normalsPoolSize;
	normalsPoolSize.type = vk::DescriptorType::eStorageBuffer;
	normalsPoolSize.descriptorCount = MAX_MESHES;
	
	// ColorSSBO
	vk::DescriptorPoolSize colorsPoolSize;
	colorsPoolSize.type = vk::DescriptorType::eStorageBuffer;
	colorsPoolSize.descriptorCount = MAX_MESHES;

	// TexCoordSSBO
	vk::DescriptorPoolSize texcoordsPoolSize;
	texcoordsPoolSize.type = vk::DescriptorType::eStorageBuffer;
	texcoordsPoolSize.descriptorCount = MAX_MESHES;

	// IndexSSBO
	vk::DescriptorPoolSize indexPoolSize;
	indexPoolSize.type = vk::DescriptorType::eStorageBuffer;
	indexPoolSize.descriptorCount = MAX_MESHES;

	vk::DescriptorPoolCreateInfo positionsPoolInfo;
	positionsPoolInfo.poolSizeCount = 1;
	positionsPoolInfo.pPoolSizes = &positionsPoolSize;
	positionsPoolInfo.maxSets = MAX_MESHES;
	positionsPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	vk::DescriptorPoolCreateInfo normalsPoolInfo;
	normalsPoolInfo.poolSizeCount = 1;
	normalsPoolInfo.pPoolSizes = &normalsPoolSize;
	normalsPoolInfo.maxSets = MAX_MESHES;
	normalsPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	
	vk::DescriptorPoolCreateInfo colorsPoolInfo;
	colorsPoolInfo.poolSizeCount = 1;
	colorsPoolInfo.pPoolSizes = &colorsPoolSize;
	colorsPoolInfo.maxSets = MAX_MESHES;
	colorsPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	vk::DescriptorPoolCreateInfo texcoordsPoolInfo;
	texcoordsPoolInfo.poolSizeCount = 1;
	texcoordsPoolInfo.pPoolSizes = &texcoordsPoolSize;
	texcoordsPoolInfo.maxSets = MAX_MESHES;
	texcoordsPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	vk::DescriptorPoolCreateInfo indexPoolInfo;
	indexPoolInfo.poolSizeCount = 1;
	indexPoolInfo.pPoolSizes = &indexPoolSize;
	indexPoolInfo.maxSets = MAX_MESHES;
	indexPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	
	/* Raytrace Descriptor Pool Info */
	std::array<vk::DescriptorPoolSize, 1> raytracingPoolSizes = {};
	
	// Acceleration Structure
	raytracingPoolSizes[0].type = vk::DescriptorType::eAccelerationStructureNV;
	raytracingPoolSizes[0].descriptorCount = 1;
	
	vk::DescriptorPoolCreateInfo raytracingPoolInfo;
	raytracingPoolInfo.poolSizeCount = (uint32_t)raytracingPoolSizes.size();
	raytracingPoolInfo.pPoolSizes = raytracingPoolSizes.data();
	raytracingPoolInfo.maxSets = 1;
	raytracingPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	// Create the pools
	componentDescriptorPool = device.createDescriptorPool(SSBOPoolInfo);
	textureDescriptorPool = device.createDescriptorPool(texturePoolInfo);
	// gbufferDescriptorPool = device.createDescriptorPool(gbufferPoolInfo);

	/* This is dumb. Why do I need to do this?... Cant allocate more than one gbuffer 
	descriptor set on intel without "vk_out_of_host_memory" ... */
	for (uint32_t i = 0; i < MAX_CAMERAS * 6; ++i) {
	 	gbufferDescriptorPools.push_back(device.createDescriptorPool(gbufferPoolInfo));
	}

	if (vulkan->is_ray_tracing_enabled()) {
		positionsDescriptorPool = device.createDescriptorPool(positionsPoolInfo);
		normalsDescriptorPool = device.createDescriptorPool(normalsPoolInfo);
		colorsDescriptorPool = device.createDescriptorPool(colorsPoolInfo);
		texcoordsDescriptorPool = device.createDescriptorPool(texcoordsPoolInfo);
		indexDescriptorPool = device.createDescriptorPool(indexPoolInfo);
		raytracingDescriptorPool = device.createDescriptorPool(raytracingPoolInfo);
	}
}

void RenderSystem::update_global_descriptor_sets()
{
	if (  (componentDescriptorPool == vk::DescriptorPool()) || (textureDescriptorPool == vk::DescriptorPool())) return;
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();
	
	/* ------ Component Descriptor Set  ------ */
	vk::DescriptorSetLayout SSBOLayouts[] = { componentDescriptorSetLayout };
	std::array<vk::WriteDescriptorSet, 7> SSBODescriptorWrites = {};
	if (componentDescriptorSet == vk::DescriptorSet())
	{
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = componentDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = SSBOLayouts;
		componentDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
	}

	// Entity SSBO
	vk::DescriptorBufferInfo entityBufferInfo;
	entityBufferInfo.buffer = Entity::GetSSBO();
	entityBufferInfo.offset = 0;
	entityBufferInfo.range = Entity::GetSSBOSize();

	if (entityBufferInfo.buffer == vk::Buffer()) return;

	SSBODescriptorWrites[0].dstSet = componentDescriptorSet;
	SSBODescriptorWrites[0].dstBinding = 0;
	SSBODescriptorWrites[0].dstArrayElement = 0;
	SSBODescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
	SSBODescriptorWrites[0].descriptorCount = 1;
	SSBODescriptorWrites[0].pBufferInfo = &entityBufferInfo;

	// Transform SSBO
	vk::DescriptorBufferInfo transformBufferInfo;
	transformBufferInfo.buffer = Transform::GetSSBO();
	transformBufferInfo.offset = 0;
	transformBufferInfo.range = Transform::GetSSBOSize();

	if (transformBufferInfo.buffer == vk::Buffer()) return;

	SSBODescriptorWrites[1].dstSet = componentDescriptorSet;
	SSBODescriptorWrites[1].dstBinding = 1;
	SSBODescriptorWrites[1].dstArrayElement = 0;
	SSBODescriptorWrites[1].descriptorType = vk::DescriptorType::eStorageBuffer;
	SSBODescriptorWrites[1].descriptorCount = 1;
	SSBODescriptorWrites[1].pBufferInfo = &transformBufferInfo;

	// Camera SSBO
	vk::DescriptorBufferInfo cameraBufferInfo;
	cameraBufferInfo.buffer = Camera::GetSSBO();
	cameraBufferInfo.offset = 0;
	cameraBufferInfo.range = Camera::GetSSBOSize();

	if (cameraBufferInfo.buffer == vk::Buffer()) return;

	SSBODescriptorWrites[2].dstSet = componentDescriptorSet;
	SSBODescriptorWrites[2].dstBinding = 2;
	SSBODescriptorWrites[2].dstArrayElement = 0;
	SSBODescriptorWrites[2].descriptorType = vk::DescriptorType::eStorageBuffer;
	SSBODescriptorWrites[2].descriptorCount = 1;
	SSBODescriptorWrites[2].pBufferInfo = &cameraBufferInfo;

	// Material SSBO
	vk::DescriptorBufferInfo materialBufferInfo;
	materialBufferInfo.buffer = Material::GetSSBO();
	materialBufferInfo.offset = 0;
	materialBufferInfo.range = Material::GetSSBOSize();

	if (materialBufferInfo.buffer == vk::Buffer()) return;

	SSBODescriptorWrites[3].dstSet = componentDescriptorSet;
	SSBODescriptorWrites[3].dstBinding = 3;
	SSBODescriptorWrites[3].dstArrayElement = 0;
	SSBODescriptorWrites[3].descriptorType = vk::DescriptorType::eStorageBuffer;
	SSBODescriptorWrites[3].descriptorCount = 1;
	SSBODescriptorWrites[3].pBufferInfo = &materialBufferInfo;

	// Light SSBO
	vk::DescriptorBufferInfo lightBufferInfo;
	lightBufferInfo.buffer = Light::GetSSBO();
	lightBufferInfo.offset = 0;
	lightBufferInfo.range = Light::GetSSBOSize();

	if (lightBufferInfo.buffer == vk::Buffer()) return;

	SSBODescriptorWrites[4].dstSet = componentDescriptorSet;
	SSBODescriptorWrites[4].dstBinding = 4;
	SSBODescriptorWrites[4].dstArrayElement = 0;
	SSBODescriptorWrites[4].descriptorType = vk::DescriptorType::eStorageBuffer;
	SSBODescriptorWrites[4].descriptorCount = 1;
	SSBODescriptorWrites[4].pBufferInfo = &lightBufferInfo;

	// Mesh SSBO
	vk::DescriptorBufferInfo meshBufferInfo;
	meshBufferInfo.buffer = Mesh::GetSSBO();
	meshBufferInfo.offset = 0;
	meshBufferInfo.range = Mesh::GetSSBOSize();

	SSBODescriptorWrites[5].dstSet = componentDescriptorSet;
	SSBODescriptorWrites[5].dstBinding = 5;
	SSBODescriptorWrites[5].dstArrayElement = 0;
	SSBODescriptorWrites[5].descriptorType = vk::DescriptorType::eStorageBuffer;
	SSBODescriptorWrites[5].descriptorCount = 1;
	SSBODescriptorWrites[5].pBufferInfo = &meshBufferInfo;

	// Light Entities SSBO
	vk::DescriptorBufferInfo lightEntityBufferInfo;
	lightEntityBufferInfo.buffer = Light::GetLightEntitiesSSBO();
	lightEntityBufferInfo.offset = 0;
	lightEntityBufferInfo.range = Light::GetLightEntitiesSSBOSize();

	if (lightEntityBufferInfo.buffer == vk::Buffer()) return;

	SSBODescriptorWrites[6].dstSet = componentDescriptorSet;
	SSBODescriptorWrites[6].dstBinding = 6;
	SSBODescriptorWrites[6].dstArrayElement = 0;
	SSBODescriptorWrites[6].descriptorType = vk::DescriptorType::eStorageBuffer;
	SSBODescriptorWrites[6].descriptorCount = 1;
	SSBODescriptorWrites[6].pBufferInfo = &lightEntityBufferInfo;
	
	device.updateDescriptorSets((uint32_t)SSBODescriptorWrites.size(), SSBODescriptorWrites.data(), 0, nullptr);
	
	/* ------ Texture Descriptor Set  ------ */
	vk::DescriptorSetLayout textureLayouts[] = { textureDescriptorSetLayout };
	std::array<vk::WriteDescriptorSet, 14> textureDescriptorWrites = {};
	
	if (textureDescriptorSet == vk::DescriptorSet())
	{
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = textureDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = textureLayouts;

		textureDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
	}

	auto texture2DLayouts = Texture::GetLayouts(vk::ImageViewType::e2D);
	auto texture2DViews = Texture::GetImageViews(vk::ImageViewType::e2D);
	auto textureCubeLayouts = Texture::GetLayouts(vk::ImageViewType::eCube);
	auto textureCubeViews = Texture::GetImageViews(vk::ImageViewType::eCube);
	auto texture3DLayouts = Texture::GetLayouts(vk::ImageViewType::e3D);
	auto texture3DViews = Texture::GetImageViews(vk::ImageViewType::e3D);
	auto samplers = Texture::GetSamplers();
	
	Texture* blueNoiseTile = nullptr;
	Texture* brdfLUT = nullptr;
	Texture* ltcMat = nullptr;
	Texture* ltcAmp = nullptr;
	Texture* environment = nullptr;
	Texture* specularEnvironment = nullptr;
	Texture* diffuseEnvironment = nullptr;
	Texture* ddgiIrradiance = nullptr;
	Texture* ddgiVisibility = nullptr;
	try {
		blueNoiseTile = Texture::Get("BLUENOISETILE");
        brdfLUT = Texture::Get("BRDF");
        ltcMat = Texture::Get("LTCMAT");
        ltcAmp = Texture::Get("LTCAMP");
        environment = (environment_id != -1) ? Texture::Get(environment_id) : Texture::Get("DefaultTexCube");
        specularEnvironment = (specular_environment_id != -1) ? Texture::Get(specular_environment_id) : Texture::Get("DefaultTexCube");
        diffuseEnvironment = (diffuse_environment_id != -1) ? Texture::Get(diffuse_environment_id) : Texture::Get("DefaultTexCube");
		ddgiIrradiance = Texture::Get("DDGI_IRRADIANCE");
        ddgiVisibility = Texture::Get("DDGI_VISIBILITY");
    } catch (...) {}

	if (texture3DLayouts.size() <= 0) return;
	if (texture2DLayouts.size() <= 0) return;
	if (textureCubeLayouts.size() <= 0) return;
	if (blueNoiseTile == nullptr) return;
	if (brdfLUT == nullptr) return;
	if (ltcMat == nullptr) return;
	if (ltcAmp == nullptr) return;
	if (environment == nullptr) return;
	if (specularEnvironment == nullptr) return;
	if (diffuseEnvironment == nullptr) return;
	if (ddgiIrradiance == nullptr) return;
	if (ddgiVisibility == nullptr) return;
	

	// Texture SSBO
	vk::DescriptorBufferInfo textureBufferInfo;
	textureBufferInfo.buffer = Texture::GetSSBO();
	textureBufferInfo.offset = 0;
	textureBufferInfo.range = Texture::GetSSBOSize();

	if (textureBufferInfo.buffer == vk::Buffer()) return;

	textureDescriptorWrites[0].dstSet = textureDescriptorSet;
	textureDescriptorWrites[0].dstBinding = 0;
	textureDescriptorWrites[0].dstArrayElement = 0;
	textureDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
	textureDescriptorWrites[0].descriptorCount = 1;
	textureDescriptorWrites[0].pBufferInfo = &textureBufferInfo;

	// Samplers
	vk::DescriptorImageInfo samplerDescriptorInfos[MAX_SAMPLERS];
	for (int i = 0; i < MAX_SAMPLERS; ++i) 
	{
		samplerDescriptorInfos[i].sampler = samplers[i];
	}

	textureDescriptorWrites[1].dstSet = textureDescriptorSet;
	textureDescriptorWrites[1].dstBinding = 1;
	textureDescriptorWrites[1].dstArrayElement = 0;
	textureDescriptorWrites[1].descriptorType = vk::DescriptorType::eSampler;
	textureDescriptorWrites[1].descriptorCount = MAX_SAMPLERS;
	textureDescriptorWrites[1].pImageInfo = samplerDescriptorInfos;

	// 2D Textures
	vk::DescriptorImageInfo texture2DDescriptorInfos[MAX_TEXTURES];
	for (int i = 0; i < MAX_TEXTURES; ++i) 
	{
		texture2DDescriptorInfos[i].sampler = nullptr;
		texture2DDescriptorInfos[i].imageLayout = texture2DLayouts[i];
		texture2DDescriptorInfos[i].imageView = texture2DViews[i];
	}

	textureDescriptorWrites[2].dstSet = textureDescriptorSet;
	textureDescriptorWrites[2].dstBinding = 2;
	textureDescriptorWrites[2].dstArrayElement = 0;
	textureDescriptorWrites[2].descriptorType = vk::DescriptorType::eSampledImage;
	textureDescriptorWrites[2].descriptorCount = MAX_TEXTURES;
	textureDescriptorWrites[2].pImageInfo = texture2DDescriptorInfos;

	// Texture Cubes
	vk::DescriptorImageInfo textureCubeDescriptorInfos[MAX_TEXTURES];
	for (int i = 0; i < MAX_TEXTURES; ++i) 
	{
		textureCubeDescriptorInfos[i].sampler = nullptr;
		textureCubeDescriptorInfos[i].imageLayout = textureCubeLayouts[i];
		textureCubeDescriptorInfos[i].imageView = textureCubeViews[i];
	}

	textureDescriptorWrites[3].dstSet = textureDescriptorSet;
	textureDescriptorWrites[3].dstBinding = 3;
	textureDescriptorWrites[3].dstArrayElement = 0;
	textureDescriptorWrites[3].descriptorType = vk::DescriptorType::eSampledImage;
	textureDescriptorWrites[3].descriptorCount = MAX_TEXTURES;
	textureDescriptorWrites[3].pImageInfo = textureCubeDescriptorInfos;

	// 3D Textures
	vk::DescriptorImageInfo texture3DDescriptorInfos[MAX_TEXTURES];
	for (int i = 0; i < MAX_TEXTURES; ++i) 
	{
		texture3DDescriptorInfos[i].sampler = nullptr;
		texture3DDescriptorInfos[i].imageLayout = texture3DLayouts[i];
		texture3DDescriptorInfos[i].imageView = texture3DViews[i];
	}

	textureDescriptorWrites[4].dstSet = textureDescriptorSet;
	textureDescriptorWrites[4].dstBinding = 4;
	textureDescriptorWrites[4].dstArrayElement = 0;
	textureDescriptorWrites[4].descriptorType = vk::DescriptorType::eSampledImage;
	textureDescriptorWrites[4].descriptorCount = MAX_TEXTURES;
	textureDescriptorWrites[4].pImageInfo = texture3DDescriptorInfos;

	// Blue Noise Tile
	vk::DescriptorImageInfo blueNoiseTileDescriptorInfo;
	blueNoiseTileDescriptorInfo.sampler = nullptr;
	blueNoiseTileDescriptorInfo.imageLayout = blueNoiseTile->get_color_image_layout();
	blueNoiseTileDescriptorInfo.imageView = blueNoiseTile->get_color_image_view();
	
	textureDescriptorWrites[5].dstSet = textureDescriptorSet;
	textureDescriptorWrites[5].dstBinding = 5;
	textureDescriptorWrites[5].dstArrayElement = 0;
	textureDescriptorWrites[5].descriptorType = vk::DescriptorType::eSampledImage;
	textureDescriptorWrites[5].descriptorCount = 1;
	textureDescriptorWrites[5].pImageInfo = &blueNoiseTileDescriptorInfo;

	// BRDF LUT
	vk::DescriptorImageInfo brdfLUTDescriptorInfo;
	brdfLUTDescriptorInfo.sampler = nullptr;
	brdfLUTDescriptorInfo.imageLayout = brdfLUT->get_color_image_layout();
	brdfLUTDescriptorInfo.imageView = brdfLUT->get_color_image_view();
	
	textureDescriptorWrites[6].dstSet = textureDescriptorSet;
	textureDescriptorWrites[6].dstBinding = 6;
	textureDescriptorWrites[6].dstArrayElement = 0;
	textureDescriptorWrites[6].descriptorType = vk::DescriptorType::eSampledImage;
	textureDescriptorWrites[6].descriptorCount = 1;
	textureDescriptorWrites[6].pImageInfo = &brdfLUTDescriptorInfo;

	// LTC Mat
	vk::DescriptorImageInfo ltcMatDescriptorInfo;
	ltcMatDescriptorInfo.sampler = nullptr;
	ltcMatDescriptorInfo.imageLayout = ltcMat->get_color_image_layout();
	ltcMatDescriptorInfo.imageView = ltcMat->get_color_image_view();
	
	textureDescriptorWrites[7].dstSet = textureDescriptorSet;
	textureDescriptorWrites[7].dstBinding = 7;
	textureDescriptorWrites[7].dstArrayElement = 0;
	textureDescriptorWrites[7].descriptorType = vk::DescriptorType::eSampledImage;
	textureDescriptorWrites[7].descriptorCount = 1;
	textureDescriptorWrites[7].pImageInfo = &ltcMatDescriptorInfo;

	// LTC Amp
	vk::DescriptorImageInfo ltcAmpDescriptorInfo;
	ltcAmpDescriptorInfo.sampler = nullptr;
	ltcAmpDescriptorInfo.imageLayout = ltcAmp->get_color_image_layout();
	ltcAmpDescriptorInfo.imageView = ltcAmp->get_color_image_view();
	
	textureDescriptorWrites[8].dstSet = textureDescriptorSet;
	textureDescriptorWrites[8].dstBinding = 8;
	textureDescriptorWrites[8].dstArrayElement = 0;
	textureDescriptorWrites[8].descriptorType = vk::DescriptorType::eSampledImage;
	textureDescriptorWrites[8].descriptorCount = 1;
	textureDescriptorWrites[8].pImageInfo = &ltcAmpDescriptorInfo;

	// Environment
	vk::DescriptorImageInfo environmentDescriptorInfo;
	environmentDescriptorInfo.sampler = nullptr;
	environmentDescriptorInfo.imageLayout = environment->get_color_image_layout();
	environmentDescriptorInfo.imageView = environment->get_color_image_view();
	
	textureDescriptorWrites[9].dstSet = textureDescriptorSet;
	textureDescriptorWrites[9].dstBinding = 9;
	textureDescriptorWrites[9].dstArrayElement = 0;
	textureDescriptorWrites[9].descriptorType = vk::DescriptorType::eSampledImage;
	textureDescriptorWrites[9].descriptorCount = 1;
	textureDescriptorWrites[9].pImageInfo = &environmentDescriptorInfo;

	// Diffuse Environment
	vk::DescriptorImageInfo diffuseEnvironmentDescriptorInfo;
	diffuseEnvironmentDescriptorInfo.sampler = nullptr;
	diffuseEnvironmentDescriptorInfo.imageLayout = diffuseEnvironment->get_color_image_layout();
	diffuseEnvironmentDescriptorInfo.imageView = diffuseEnvironment->get_color_image_view();
	
	textureDescriptorWrites[10].dstSet = textureDescriptorSet;
	textureDescriptorWrites[10].dstBinding = 10;
	textureDescriptorWrites[10].dstArrayElement = 0;
	textureDescriptorWrites[10].descriptorType = vk::DescriptorType::eSampledImage;
	textureDescriptorWrites[10].descriptorCount = 1;
	textureDescriptorWrites[10].pImageInfo = &diffuseEnvironmentDescriptorInfo;

	// Specular Environment
	vk::DescriptorImageInfo specularEnvironmentDescriptorInfo;
	specularEnvironmentDescriptorInfo.sampler = nullptr;
	specularEnvironmentDescriptorInfo.imageLayout = specularEnvironment->get_color_image_layout();
	specularEnvironmentDescriptorInfo.imageView = specularEnvironment->get_color_image_view();
	
	textureDescriptorWrites[11].dstSet = textureDescriptorSet;
	textureDescriptorWrites[11].dstBinding = 11;
	textureDescriptorWrites[11].dstArrayElement = 0;
	textureDescriptorWrites[11].descriptorType = vk::DescriptorType::eSampledImage;
	textureDescriptorWrites[11].descriptorCount = 1;
	textureDescriptorWrites[11].pImageInfo = &specularEnvironmentDescriptorInfo;
	
	// DDGI Irradiance
	vk::DescriptorImageInfo ddgiIrradianceDescriptorInfo;
	ddgiIrradianceDescriptorInfo.sampler = nullptr;
	ddgiIrradianceDescriptorInfo.imageLayout = ddgiIrradiance->get_color_image_layout();
	ddgiIrradianceDescriptorInfo.imageView = ddgiIrradiance->get_color_image_view();
	
	textureDescriptorWrites[12].dstSet = textureDescriptorSet;
	textureDescriptorWrites[12].dstBinding = 12;
	textureDescriptorWrites[12].dstArrayElement = 0;
	textureDescriptorWrites[12].descriptorType = vk::DescriptorType::eStorageImage;
	textureDescriptorWrites[12].descriptorCount = 1;
	textureDescriptorWrites[12].pImageInfo = &ddgiIrradianceDescriptorInfo;
	
	// DDGI Visibility
	vk::DescriptorImageInfo ddgiVisibilityDescriptorInfo;
	ddgiVisibilityDescriptorInfo.sampler = nullptr;
	ddgiVisibilityDescriptorInfo.imageLayout = ddgiVisibility->get_color_image_layout();
	ddgiVisibilityDescriptorInfo.imageView = ddgiVisibility->get_color_image_view();
	
	textureDescriptorWrites[13].dstSet = textureDescriptorSet;
	textureDescriptorWrites[13].dstBinding = 13;
	textureDescriptorWrites[13].dstArrayElement = 0;
	textureDescriptorWrites[13].descriptorType = vk::DescriptorType::eStorageImage;
	textureDescriptorWrites[13].descriptorCount = 1;
	textureDescriptorWrites[13].pImageInfo = &ddgiVisibilityDescriptorInfo;

	device.updateDescriptorSets((uint32_t)textureDescriptorWrites.size(), textureDescriptorWrites.data(), 0, nullptr);

	if (vulkan->is_ray_tracing_enabled()) {
		/* ------ Vertex Descriptor Set  ------ */
		vk::DescriptorSetLayout positionsLayouts[] = { positionsDescriptorSetLayout };
		vk::DescriptorSetLayout normalsLayouts[] = { normalsDescriptorSetLayout };
		vk::DescriptorSetLayout colorsLayouts[] = { colorsDescriptorSetLayout };
		vk::DescriptorSetLayout texcoordsLayouts[] = { texcoordsDescriptorSetLayout };
		vk::DescriptorSetLayout indexLayouts[] = { indexDescriptorSetLayout };
		std::array<vk::WriteDescriptorSet, 1> positionsDescriptorWrites = {};
		std::array<vk::WriteDescriptorSet, 1> normalsDescriptorWrites = {};
		std::array<vk::WriteDescriptorSet, 1> colorsDescriptorWrites = {};
		std::array<vk::WriteDescriptorSet, 1> texcoordsDescriptorWrites = {};
		std::array<vk::WriteDescriptorSet, 1> indexDescriptorWrites = {};
		
		if (positionsDescriptorSet == vk::DescriptorSet())
		{
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = positionsDescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = positionsLayouts;

			positionsDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
		}

		if (normalsDescriptorSet == vk::DescriptorSet())
		{
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = normalsDescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = normalsLayouts;

			normalsDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
		}

		if (colorsDescriptorSet == vk::DescriptorSet())
		{
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = colorsDescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = colorsLayouts;

			colorsDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
		}

		if (texcoordsDescriptorSet == vk::DescriptorSet())
		{
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = texcoordsDescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = texcoordsLayouts;

			texcoordsDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
		}

		if (indexDescriptorSet == vk::DescriptorSet())
		{
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = indexDescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = indexLayouts;

			indexDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
		}

		auto positionSSBOs = Mesh::GetPositionSSBOs();
		auto positionSSBOSizes = Mesh::GetPositionSSBOSizes();
		auto normalSSBOs = Mesh::GetNormalSSBOs();
		auto normalSSBOSizes = Mesh::GetNormalSSBOSizes();
		auto colorSSBOs = Mesh::GetColorSSBOs();
		auto colorSSBOSizes = Mesh::GetColorSSBOSizes();
		auto texcoordSSBOs = Mesh::GetTexCoordSSBOs();
		auto texcoordSSBOSizes = Mesh::GetTexCoordSSBOSizes();
		auto indexSSBOs = Mesh::GetIndexSSBOs();
		auto indexSSBOSizes = Mesh::GetIndexSSBOSizes();
		
		// Positions SSBO
		vk::DescriptorBufferInfo positionBufferInfos[MAX_MESHES];
		for (int i = 0; i < MAX_MESHES; ++i) 
		{
			if (positionSSBOs[i] == vk::Buffer()) return;
			positionBufferInfos[i].buffer = positionSSBOs[i];
			positionBufferInfos[i].offset = 0;
			positionBufferInfos[i].range = positionSSBOSizes[i];
		}

		positionsDescriptorWrites[0].dstSet = positionsDescriptorSet;
		positionsDescriptorWrites[0].dstBinding = 0;
		positionsDescriptorWrites[0].dstArrayElement = 0;
		positionsDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
		positionsDescriptorWrites[0].descriptorCount = MAX_MESHES;
		positionsDescriptorWrites[0].pBufferInfo = positionBufferInfos;

		// Normals SSBO
		vk::DescriptorBufferInfo normalBufferInfos[MAX_MESHES];
		for (int i = 0; i < MAX_MESHES; ++i) 
		{
			if (normalSSBOs[i] == vk::Buffer()) return;
			normalBufferInfos[i].buffer = normalSSBOs[i];
			normalBufferInfos[i].offset = 0;
			normalBufferInfos[i].range = normalSSBOSizes[i];
		}

		normalsDescriptorWrites[0].dstSet = normalsDescriptorSet;
		normalsDescriptorWrites[0].dstBinding = 0;
		normalsDescriptorWrites[0].dstArrayElement = 0;
		normalsDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
		normalsDescriptorWrites[0].descriptorCount = MAX_MESHES;
		normalsDescriptorWrites[0].pBufferInfo = normalBufferInfos;

		// Colors SSBO
		vk::DescriptorBufferInfo colorBufferInfos[MAX_MESHES];
		for (int i = 0; i < MAX_MESHES; ++i) 
		{
			if (colorSSBOs[i] == vk::Buffer()) return;
			colorBufferInfos[i].buffer = colorSSBOs[i];
			colorBufferInfos[i].offset = 0;
			colorBufferInfos[i].range = colorSSBOSizes[i];
		}

		colorsDescriptorWrites[0].dstSet = colorsDescriptorSet;
		colorsDescriptorWrites[0].dstBinding = 0;
		colorsDescriptorWrites[0].dstArrayElement = 0;
		colorsDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
		colorsDescriptorWrites[0].descriptorCount = MAX_MESHES;
		colorsDescriptorWrites[0].pBufferInfo = colorBufferInfos;

		// TexCoords SSBO
		vk::DescriptorBufferInfo texcoordBufferInfos[MAX_MESHES];
		for (int i = 0; i < MAX_MESHES; ++i) 
		{
			if (texcoordSSBOs[i] == vk::Buffer()) return;
			texcoordBufferInfos[i].buffer = texcoordSSBOs[i];
			texcoordBufferInfos[i].offset = 0;
			texcoordBufferInfos[i].range = texcoordSSBOSizes[i];
		}

		texcoordsDescriptorWrites[0].dstSet = texcoordsDescriptorSet;
		texcoordsDescriptorWrites[0].dstBinding = 0;
		texcoordsDescriptorWrites[0].dstArrayElement = 0;
		texcoordsDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
		texcoordsDescriptorWrites[0].descriptorCount = MAX_MESHES;
		texcoordsDescriptorWrites[0].pBufferInfo = texcoordBufferInfos;

		// Indices SSBO
		vk::DescriptorBufferInfo indexBufferInfos[MAX_MESHES];
		for (int i = 0; i < MAX_MESHES; ++i) 
		{
			if (indexSSBOs[i] == vk::Buffer()) return;
			indexBufferInfos[i].buffer = indexSSBOs[i];
			indexBufferInfos[i].offset = 0;
			indexBufferInfos[i].range = indexSSBOSizes[i];
		}

		indexDescriptorWrites[0].dstSet = indexDescriptorSet;
		indexDescriptorWrites[0].dstBinding = 0;
		indexDescriptorWrites[0].dstArrayElement = 0;
		indexDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
		indexDescriptorWrites[0].descriptorCount = MAX_MESHES;
		indexDescriptorWrites[0].pBufferInfo = indexBufferInfos;

		device.updateDescriptorSets((uint32_t)positionsDescriptorWrites.size(), positionsDescriptorWrites.data(), 0, nullptr);
		device.updateDescriptorSets((uint32_t)normalsDescriptorWrites.size(), normalsDescriptorWrites.data(), 0, nullptr);
		device.updateDescriptorSets((uint32_t)colorsDescriptorWrites.size(), colorsDescriptorWrites.data(), 0, nullptr);
		device.updateDescriptorSets((uint32_t)texcoordsDescriptorWrites.size(), texcoordsDescriptorWrites.data(), 0, nullptr);
		device.updateDescriptorSets((uint32_t)indexDescriptorWrites.size(), indexDescriptorWrites.data(), 0, nullptr);
	}
}

void RenderSystem::update_gbuffer_descriptor_sets(Entity &camera_entity, uint32_t rp_idx)
{
    auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

    if (!camera_entity.is_initialized()) return;
	if (!camera_entity.get_camera()) return;
	if (!camera_entity.get_camera()->get_texture()) return;

    // vk::DescriptorSetLayout gbufferLayouts[] = { gbufferDescriptorSetLayout };
	std::vector<vk::DescriptorSetLayout> gbufferLayouts(1, gbufferDescriptorSetLayout);

	auto key = std::pair<uint32_t, int32_t>(camera_entity.get_id(), rp_idx);

    if (gbufferDescriptorSets.find(key) == gbufferDescriptorSets.end()) {
        vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = gbufferDescriptorPools[gbufferPoolIdx]; // this is DUUUMB.... Why do I need to do this @intel...
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = gbufferLayouts.data();

		gbufferDescriptorSets[key] = device.allocateDescriptorSets(allocInfo)[0];
		gbufferPoolIdx++;
    }

    std::array<vk::WriteDescriptorSet, 3> gbufferDescriptorWrites = {};

	auto colorImageViewLayers = camera_entity.get_camera()->get_texture()->get_color_image_view_layers();

    // Output image 
	vk::DescriptorImageInfo descriptorOutputImageInfo;
	descriptorOutputImageInfo.imageLayout = vk::ImageLayout::eGeneral;
	descriptorOutputImageInfo.imageView = colorImageViewLayers[rp_idx];

	gbufferDescriptorWrites[0].dstSet = gbufferDescriptorSets[key];
	gbufferDescriptorWrites[0].dstBinding = 0;
	gbufferDescriptorWrites[0].dstArrayElement = 0;
	gbufferDescriptorWrites[0].descriptorCount = 1;
	gbufferDescriptorWrites[0].descriptorType = vk::DescriptorType::eStorageImage;
	gbufferDescriptorWrites[0].pImageInfo = &descriptorOutputImageInfo;

	// G Buffers
	std::vector<vk::DescriptorImageInfo> descriptorGBufferImageInfos(MAX_G_BUFFERS);

	for (uint32_t g_idx = 0; g_idx < MAX_G_BUFFERS; ++g_idx) {
		descriptorGBufferImageInfos[g_idx].imageLayout = vk::ImageLayout::eGeneral;
		auto gbufferImageViewLayers = camera_entity.get_camera()->get_texture()->get_g_buffer_image_view_layers(g_idx);
		descriptorGBufferImageInfos[g_idx].imageView = gbufferImageViewLayers[rp_idx];
	}
	
	gbufferDescriptorWrites[1].dstSet = gbufferDescriptorSets[key];
	gbufferDescriptorWrites[1].dstBinding = 1;
	gbufferDescriptorWrites[1].dstArrayElement = 0;
	gbufferDescriptorWrites[1].descriptorCount = (uint32_t)descriptorGBufferImageInfos.size();
	gbufferDescriptorWrites[1].descriptorType = vk::DescriptorType::eStorageImage;
	gbufferDescriptorWrites[1].pImageInfo = descriptorGBufferImageInfos.data();

	// G Buffer textures
	std::vector<vk::DescriptorImageInfo> descriptorGBufferTextureImageInfos(MAX_G_BUFFERS);

	for (uint32_t g_idx = 0; g_idx < MAX_G_BUFFERS; ++g_idx) {
		descriptorGBufferTextureImageInfos[g_idx].imageLayout = vk::ImageLayout::eGeneral;
		auto gbufferImageViewLayers = camera_entity.get_camera()->get_texture()->get_g_buffer_image_view_layers(g_idx);
		descriptorGBufferTextureImageInfos[g_idx].imageView = gbufferImageViewLayers[rp_idx];
	}
	
	gbufferDescriptorWrites[2].dstSet = gbufferDescriptorSets[key];
	gbufferDescriptorWrites[2].dstBinding = 2;
	gbufferDescriptorWrites[2].dstArrayElement = 0;
	gbufferDescriptorWrites[2].descriptorCount = (uint32_t)descriptorGBufferTextureImageInfos.size();
	gbufferDescriptorWrites[2].descriptorType = vk::DescriptorType::eSampledImage;
	gbufferDescriptorWrites[2].pImageInfo = descriptorGBufferTextureImageInfos.data();

    device.updateDescriptorSets((uint32_t)gbufferDescriptorWrites.size(), gbufferDescriptorWrites.data(), 0, nullptr);
}

void RenderSystem::update_raytracing_descriptor_sets(vk::AccelerationStructureNV &tlas)
{
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	if (!vulkan->is_ray_tracing_enabled()) return;
	if (tlas == vk::AccelerationStructureNV()) return;
		
	vk::DescriptorSetLayout raytracingLayouts[] = { raytracingDescriptorSetLayout };

	if (raytracingDescriptorSet == vk::DescriptorSet())
	{
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = raytracingDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = raytracingLayouts;

		raytracingDescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
	}

	std::array<vk::WriteDescriptorSet, 1> raytraceDescriptorWrites = {};

	vk::WriteDescriptorSetAccelerationStructureNV descriptorAccelerationStructureInfo;
	descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
	descriptorAccelerationStructureInfo.pAccelerationStructures = &tlas;

	raytraceDescriptorWrites[0].dstSet = raytracingDescriptorSet;
	raytraceDescriptorWrites[0].pNext = &descriptorAccelerationStructureInfo;
	raytraceDescriptorWrites[0].dstBinding = 0;
	raytraceDescriptorWrites[0].dstArrayElement = 0;
	raytraceDescriptorWrites[0].descriptorCount = 1;
	raytraceDescriptorWrites[0].descriptorType = vk::DescriptorType::eAccelerationStructureNV;
	
	device.updateDescriptorSets((uint32_t)raytraceDescriptorWrites.size(), raytraceDescriptorWrites.data(), 0, nullptr);
}

void RenderSystem::create_vertex_input_binding_descriptions() {
	/* Vertex input bindings are consistent across shaders */
	vk::VertexInputBindingDescription pointBindingDescription;
	pointBindingDescription.binding = 0;
	pointBindingDescription.stride = 4 * sizeof(float);
	pointBindingDescription.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputBindingDescription colorBindingDescription;
	colorBindingDescription.binding = 1;
	colorBindingDescription.stride = 4 * sizeof(float);
	colorBindingDescription.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputBindingDescription normalBindingDescription;
	normalBindingDescription.binding = 2;
	normalBindingDescription.stride = 4 * sizeof(float);
	normalBindingDescription.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputBindingDescription texcoordBindingDescription;
	texcoordBindingDescription.binding = 3;
	texcoordBindingDescription.stride = 2 * sizeof(float);
	texcoordBindingDescription.inputRate = vk::VertexInputRate::eVertex;

	vertexInputBindingDescriptions = { pointBindingDescription, colorBindingDescription, normalBindingDescription, texcoordBindingDescription };
}

void RenderSystem::create_vertex_attribute_descriptions() {
	/* Vertex attribute descriptions are consistent across shaders */
	std::vector<vk::VertexInputAttributeDescription> attributeDescriptions(4);

	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
	attributeDescriptions[0].offset = 0;

	attributeDescriptions[1].binding = 1;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = vk::Format::eR32G32B32A32Sfloat;
	attributeDescriptions[1].offset = 0;

	attributeDescriptions[2].binding = 2;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
	attributeDescriptions[2].offset = 0;

	attributeDescriptions[3].binding = 3;
	attributeDescriptions[3].location = 3;
	attributeDescriptions[3].format = vk::Format::eR32G32Sfloat;
	attributeDescriptions[3].offset = 0;

	vertexInputAttributeDescriptions = attributeDescriptions;
}

void RenderSystem::bind_depth_prepass_descriptor_sets(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass) 
{
	if (
		(raster_depth_prepass[render_pass].ready == false) ||
		(raster_vrmask[render_pass].ready == false)
	) return;

	std::vector<vk::DescriptorSet> descriptorSets = {componentDescriptorSet, textureDescriptorSet};
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, raster_depth_prepass[render_pass].pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, raster_vrmask[render_pass].pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
}

void RenderSystem::bind_shadow_map_descriptor_sets(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass) 
{
	if (
		(shadowmap[render_pass].ready == false)
	) return;

	std::vector<vk::DescriptorSet> descriptorSets = {componentDescriptorSet, textureDescriptorSet};
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shadowmap[render_pass].pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
}

void RenderSystem::bind_raster_primary_visibility_descriptor_sets(vk::CommandBuffer &command_buffer, vk::RenderPass &render_pass) 
{
	if (
		(raster_primary_visibility[render_pass].ready == false) || 
		(skybox[render_pass].ready == false)
	) return;

	std::vector<vk::DescriptorSet> descriptorSets = {componentDescriptorSet, textureDescriptorSet};
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, raster_primary_visibility[render_pass].pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, skybox[render_pass].pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
}

bool RenderSystem::bind_compute_descriptor_sets(vk::CommandBuffer &command_buffer)
{
	if (
		(ddgi_blend_irradiance.ready == false) ||
		(ddgi_blend_visibility.ready == false)
	) return false;

	if (!componentDescriptorSet) return false;
	if (!textureDescriptorSet) return false;

	std::vector<vk::DescriptorSet> descriptorSets = {componentDescriptorSet, textureDescriptorSet};
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, ddgi_blend_irradiance.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, ddgi_blend_visibility.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	return true;
}

void RenderSystem::bind_camera_compute_descriptor_sets(vk::CommandBuffer &command_buffer, Entity &camera_entity, uint32_t rp_idx)
{
	if (
		(edgedetect.ready == false) || 
		(asvgf_diffuse_temporal_accumulation.ready == false) || 
		(asvgf_specular_temporal_accumulation.ready == false) || 
		(progressive_refinement.ready == false) || 
		(compositing.ready == false) || 
		(copy_history.ready == false) || 
		(gaussian_x.ready == false) || 
		(gaussian_y.ready == false) || 
		(median_3x3.ready == false) || 
		(median_5x5.ready == false) || 
		(bilateral_upsample.ready == false) ||
		(reconstruct_diffuse.ready == false) ||
		(reconstruct_glossy.ready == false) ||
		(svgf_remodulate.ready == false) ||
		(asvgf_compute_gradient.ready == false) ||
		(asvgf_reconstruct_gradient.ready == false) ||
		(asvgf_fill_diffuse_holes.ready == false) ||
		(asvgf_fill_specular_holes.ready == false)
	) return;

	auto key = std::pair<uint32_t, uint32_t>(camera_entity.get_id(), rp_idx);
	std::vector<vk::DescriptorSet> descriptorSets = {componentDescriptorSet, textureDescriptorSet, gbufferDescriptorSets[key]};
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, edgedetect.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, asvgf_diffuse_temporal_accumulation.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, asvgf_specular_temporal_accumulation.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, taa.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, progressive_refinement.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compositing.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, copy_history.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, gaussian_x.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, gaussian_y.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, median_3x3.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, median_5x5.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, bilateral_upsample.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, reconstruct_diffuse.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, reconstruct_glossy.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, svgf_remodulate.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, asvgf_compute_gradient.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, asvgf_reconstruct_gradient.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, asvgf_fill_diffuse_holes.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, asvgf_fill_specular_holes.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
}

void RenderSystem::bind_raytracing_descriptor_sets(vk::CommandBuffer &command_buffer, Entity &camera_entity, uint32_t rp_idx)
{
	if (
		(raytrace_primary_visibility.ready == false) ||
		(ddgi_path_tracer.ready == false) ||
		(diffuse_path_tracer.ready == false) ||
		(specular_path_tracer.ready == false)
	) return;

	auto key = std::pair<uint32_t, uint32_t>(camera_entity.get_id(), rp_idx);
	std::vector<vk::DescriptorSet> descriptorSets = {componentDescriptorSet, textureDescriptorSet, gbufferDescriptorSets[key], positionsDescriptorSet, normalsDescriptorSet, colorsDescriptorSet, texcoordsDescriptorSet, indexDescriptorSet, raytracingDescriptorSet};
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingNV, raytrace_primary_visibility.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingNV, diffuse_path_tracer.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingNV, specular_path_tracer.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingNV, ddgi_path_tracer.pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
}

void RenderSystem::draw_entity(
	vk::CommandBuffer &command_buffer, 
	vk::RenderPass &render_pass, 
	Entity &entity, 
	PushConsts &push_constants, 
	RenderMode rendermode_override, 
	PipelineType pipeline_type_override, 
	bool render_bounding_box_override)
{
	auto pipeline_type = pipeline_type_override;

	/* Need a mesh to render. */
	auto m = entity.get_mesh();
	if (!m) return;

	bool show_bounding_box = m->should_show_bounding_box() | render_bounding_box_override;
	push_constants.flags = (show_bounding_box) ? 
		(push_constants.flags | (1 << RenderSystemOptions::RASTERIZE_BOUNDING_BOX)) : 
		(push_constants.flags & ~(1 << RenderSystemOptions::RASTERIZE_BOUNDING_BOX)); 

	/* Need a transform to render. */
	auto transform = entity.get_transform();
	if (!transform) return;

	if (show_bounding_box) {
	    m = Mesh::Get("BoundingBox");
	}

	/* Need a material to render. */
	auto material = entity.get_material();
	if (!material) return;

	auto rendermode = RenderMode::RENDER_MODE_NONE;
	if (rendermode_override != RENDER_MODE_NONE) rendermode = rendermode_override;
	else {
		if (material->is_hidden()) rendermode = RenderMode::RENDER_MODE_HIDDEN;
		else if (material->should_show_skybox()) rendermode = RenderMode::RENDER_MODE_SKYBOX;
		else rendermode = RenderMode::RENDER_MODE_PRIMARY_VISIBILITY;
	}

	if (rendermode == RENDER_MODE_HIDDEN) return;

	if (rendermode == RENDER_MODE_PRIMARY_VISIBILITY) {
		command_buffer.pushConstants(raster_primary_visibility[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_PRIMARY_VISIBILITY) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, raster_primary_visibility[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_PRIMARY_VISIBILITY;
			currentRenderpass = render_pass;
		}
	}
	else if (rendermode == RENDER_MODE_SKYBOX) {
		command_buffer.pushConstants(skybox[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_SKYBOX) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, skybox[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_SKYBOX;
			currentRenderpass = render_pass;
		}
	}
	else if (rendermode == RENDER_MODE_FRAGMENTDEPTH) {
		command_buffer.pushConstants(raster_depth_prepass[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_FRAGMENTDEPTH) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, raster_depth_prepass[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_FRAGMENTDEPTH;
			currentRenderpass = render_pass;
		}
	}
	else if (rendermode == RENDER_MODE_SHADOWMAP) {
		command_buffer.pushConstants(shadowmap[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_SHADOWMAP) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, shadowmap[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_SHADOWMAP;
			currentRenderpass = render_pass;
		}
	}
	else if (rendermode == RENDER_MODE_VRMASK) {
		command_buffer.pushConstants(raster_vrmask[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_VRMASK) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, raster_vrmask[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_VRMASK;
			currentRenderpass = render_pass;
		}
	}
	else {
		command_buffer.pushConstants(raster_primary_visibility[render_pass].pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConsts), &push_constants);
		if ((currentlyBoundRenderMode != RENDER_MODE_PRIMARY_VISIBILITY) || (currentRenderpass != render_pass) || (currentlyBoundPipelineType != pipeline_type)) {
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, raster_primary_visibility[render_pass].pipelines[pipeline_type]);
			currentlyBoundRenderMode = RENDER_MODE_PRIMARY_VISIBILITY;
			currentRenderpass = render_pass;
		}
	}
	
	currentlyBoundPipelineType = pipeline_type;
	
	command_buffer.bindVertexBuffers(0, {m->get_point_buffer(), m->get_color_buffer(), m->get_normal_buffer(), m->get_texcoord_buffer()}, {0,0,0,0});
	command_buffer.bindIndexBuffer(m->get_triangle_index_buffer(), 0, vk::IndexType::eUint32);
	command_buffer.drawIndexed(m->get_total_triangle_indices(), 1, 0, 0, 0);
}

void RenderSystem::reset_bound_material()
{
	currentRenderpass = vk::RenderPass();
	currentlyBoundRenderMode = RENDER_MODE_NONE;
}

/* SETS */
void RenderSystem::set_gamma(float gamma) { this->push_constants.gamma = gamma; }
void RenderSystem::set_exposure(float exposure) { this->push_constants.exposure = exposure; }
void RenderSystem::set_environment_map(int32_t id) { this->environment_id = id; }
void RenderSystem::set_environment_map(Texture *texture) { this->environment_id = texture->get_id(); }
void RenderSystem::set_environment_roughness(float roughness) { this->push_constants.environment_roughness = roughness; }
void RenderSystem::set_environment_intensity(float intensity) { this->push_constants.environment_intensity = intensity; }
void RenderSystem::clear_environment_map() { this->environment_id = -1; }
void RenderSystem::set_irradiance_map(int32_t id) { this->specular_environment_id = id; }
void RenderSystem::set_irradiance_map(Texture *texture) { this->specular_environment_id = texture->get_id(); }
void RenderSystem::clear_irradiance_map() { this->specular_environment_id = -1; }
void RenderSystem::set_diffuse_map(int32_t id) { this->diffuse_environment_id = id; }
void RenderSystem::set_diffuse_map(Texture *texture) { this->diffuse_environment_id = texture->get_id(); }
void RenderSystem::clear_diffuse_map() { this->diffuse_environment_id = -1; }
void RenderSystem::set_top_sky_color(glm::vec3 color) { push_constants.top_sky_color = glm::vec4(color.r, color.g, color.b, 1.0); }
void RenderSystem::set_bottom_sky_color(glm::vec3 color) { push_constants.bottom_sky_color = glm::vec4(color.r, color.g, color.b, 1.0); }
void RenderSystem::set_top_sky_color(float r, float g, float b) { set_top_sky_color(glm::vec4(r, g, b, 1.0)); }
void RenderSystem::set_bottom_sky_color(float r, float g, float b) { set_bottom_sky_color(glm::vec4(r, g, b, 1.0)); }
void RenderSystem::set_sky_transition(float transition) { push_constants.sky_transition = transition; }
void RenderSystem::use_openvr(bool useOpenVR) { this->using_openvr = useOpenVR; }
void RenderSystem::use_openvr_hidden_area_masks(bool use_masks) {this->using_vr_hidden_area_masks = use_masks;};
void RenderSystem::enable_ray_tracing(bool enable) {
	auto vulkan = Libraries::Vulkan::Get();
	if (!vulkan->is_ray_tracing_enabled()) throw std::runtime_error("Error, the \"VK_NV_ray_tracing\" device extension was not provided during initialization!");
	this->ray_tracing_enabled = enable;
};

void RenderSystem::rasterize_primary_visibility(bool enable)
{
	auto vulkan = Libraries::Vulkan::Get();
	if (!vulkan->is_ray_tracing_enabled()) throw std::runtime_error("Error, the \"VK_NV_ray_tracing\" device extension was not provided during initialization!");
	this->rasterize_primary_visibility_enabled = enable;
}

void RenderSystem::enable_taa(bool enable) {
	this->taa_enabled = enable; 
	this->push_constants.frame = 0; 
	if (this->taa_enabled) {
		enable_progressive_refinement(false);
		this->push_constants.flags |= ( 1 << RenderSystemOptions::ENABLE_TAA );
	} else {
		this->push_constants.flags &= ~( 1 << RenderSystemOptions::ENABLE_TAA );
	}
};

void RenderSystem::enable_asvgf(bool enable)
{
	this->asvgf_enabled = enable;
	if (this->asvgf_enabled) {
		this->push_constants.flags |= ( 1 << RenderSystemOptions::ENABLE_ASVGF );
		this->enable_progressive_refinement(false);
	} else {
		this->push_constants.flags &= ~( 1 << RenderSystemOptions::ENABLE_ASVGF );
	}
}

void RenderSystem::enable_asvgf_gradient(bool enable)
{
	this->asvgf_gradient_enabled = enable;
	if (this->asvgf_gradient_enabled) {
		this->push_constants.flags |= ( 1 << RenderSystemOptions::ENABLE_ASVGF_GRADIENT );
	} else {
		this->push_constants.flags &= ~( 1 << RenderSystemOptions::ENABLE_ASVGF_GRADIENT );
	}
}

void RenderSystem::set_asvgf_gradient_reconstruction_iterations(int iterations)
{
	this->asvgf_gradient_reconstruction_iterations = iterations;
}

void RenderSystem::set_asvgf_gradient_reconstruction_sigma(float sigma)
{
	this->asvgf_gradient_reconstruction_sigma = sigma;
	if (sigma > 1.0) this->asvgf_gradient_reconstruction_sigma = 1.0;
	if (sigma < 0.0) this->asvgf_gradient_reconstruction_sigma = 0.0;
}

void RenderSystem::set_asvgf_gradient_influence(float influence)
{
	this->asvgf_diffuse_gradient_influence = influence;
	this->asvgf_specular_gradient_influence = influence;
}

void RenderSystem::set_asvgf_diffuse_gradient_influence(float influence)
{
	this->asvgf_diffuse_gradient_influence = influence;
}

void RenderSystem::set_asvgf_specular_gradient_influence(float influence)
{
	this->asvgf_specular_gradient_influence = influence;
}

void RenderSystem::enable_asvgf_temporal_accumulation(bool enable)
{
	this->asvgf_temporal_accumulation_enabled = enable;
	if (this->asvgf_temporal_accumulation_enabled) {
		this->push_constants.flags |= ( 1 << RenderSystemOptions::ENABLE_ASVGF_TEMPORAL_ACCUMULATION );
	} else {
		this->push_constants.flags &= ~( 1 << RenderSystemOptions::ENABLE_ASVGF_TEMPORAL_ACCUMULATION );
	}
}

void RenderSystem::enable_asvgf_variance_estimation(bool enable)
{
	this->asvgf_variance_estimation_enabled = enable;
	if (this->asvgf_variance_estimation_enabled) {
		this->push_constants.flags |= ( 1 << RenderSystemOptions::ENABLE_ASVGF_VARIANCE_ESTIMATION );
	} else {
		this->push_constants.flags &= ~( 1 << RenderSystemOptions::ENABLE_ASVGF_VARIANCE_ESTIMATION );
	}
}

void RenderSystem::enable_asvgf_atrous(bool enable)
{
	this->asvgf_atrous_enabled = enable;
	if (this->asvgf_atrous_enabled) {
		this->push_constants.flags |= ( 1 << RenderSystemOptions::ENABLE_ASVGF_ATROUS );
	} else {
		this->push_constants.flags &= ~( 1 << RenderSystemOptions::ENABLE_ASVGF_ATROUS );
	}
}

void RenderSystem::set_asvgf_atrous_iterations(int iterations)
{
	this->asvgf_atrous_iterations = iterations;
}

void RenderSystem::set_asvgf_atrous_sigma(float sigma)
{
	this->asvgf_direct_diffuse_blur = sigma;
	this->asvgf_direct_glossy_blur = sigma;
	this->asvgf_indirect_diffuse_blur = sigma;
	this->asvgf_indirect_glossy_blur = sigma;
}

void RenderSystem::set_direct_diffuse_blur(float percent)
{
	this->asvgf_direct_diffuse_blur = percent;
}

void RenderSystem::set_direct_glossy_blur(float percent)
{
	this->asvgf_direct_glossy_blur = percent;
}

void RenderSystem::set_indirect_diffuse_blur(float percent)
{
	this->asvgf_indirect_diffuse_blur = percent;
}

void RenderSystem::set_indirect_glossy_blur(float percent)
{
	this->asvgf_indirect_glossy_blur = percent;
}

void RenderSystem::enable_progressive_refinement(bool enable) {
	this->progressive_refinement_enabled = enable; 
	this->push_constants.frame = 0; 
	if (this->progressive_refinement_enabled) {
		enable_taa(false);
		this->push_constants.flags |= ( 1 << RenderSystemOptions::ENABLE_PROGRESSIVE_REFINEMENT );
	} else {
		this->push_constants.flags &= ~( 1 << RenderSystemOptions::ENABLE_PROGRESSIVE_REFINEMENT );
	}
};

void RenderSystem::enable_tone_mapping(bool enable) {
	this->tone_mapping_enabled = enable; 
};

void RenderSystem::set_max_bounces(uint32_t max_bounces) {
	auto vulkan = Libraries::Vulkan::Get();
	uint32_t limit = vulkan->get_physical_device_ray_tracing_properties().maxRecursionDepth;
	if (max_bounces > limit - 1) max_bounces = limit - 1;
	this->max_bounces = max_bounces;
}

float RenderSystem::get_milliseconds_per_frame() 
{
	return this->ms_per_frame;
}

float RenderSystem::get_milliseconds_per_acquire_swapchain_images() 
{
	return this->ms_per_acquire_swapchain_images;
}

float RenderSystem::get_milliseconds_per_record_commands() 
{
	return this->ms_per_record_commands;
}

float RenderSystem::get_milliseconds_per_record_raster_pass() 
{
	return this->ms_per_record_raster_pass;
}

float RenderSystem::get_milliseconds_per_record_ray_trace_pass() 
{
	return this->ms_per_record_raytrace_pass;
}

float RenderSystem::get_milliseconds_per_record_compute_pass() 
{
	return this->ms_per_record_compute_pass;
}

float RenderSystem::get_milliseconds_per_enqueue_commands()
{
	return this->ms_per_enqueue_commands;
}

float RenderSystem::get_milliseconds_per_submit_commands()
{
	return this->ms_per_submit_commands;
}

float RenderSystem::get_milliseconds_per_present_frames()
{
	return this->ms_per_present_frames;
}

float RenderSystem::get_milliseconds_per_download_visibility_queries()
{
	return this->ms_per_download_visibility_queries;
}

float RenderSystem::get_milliseconds_per_upload_ssbo()
{
	return this->ms_per_upload_ssbo;
}

float RenderSystem::get_milliseconds_per_update_push_constants()
{
	return this->ms_per_update_push_constants;
}

float RenderSystem::get_milliseconds_per_record_cameras()
{
	return this->ms_per_record_cameras;
}

float RenderSystem::get_milliseconds_per_record_blit_textures()
{
	return this->ms_per_record_blit_textures;
}


void RenderSystem::enable_blue_noise(bool enable) {
	if (enable) {
		this->push_constants.flags |= ( 1 << RenderSystemOptions::ENABLE_BLUE_NOISE );
	} else {
		this->push_constants.flags &= ~( 1 << RenderSystemOptions::ENABLE_BLUE_NOISE );
	}
}

void RenderSystem::enable_analytical_arealights(bool enable) {
	if (enable) {
		this->push_constants.flags |= ( 1 << RenderSystemOptions::ENABLE_ANALYTICAL_AREA_LIGHTS );
	} else {
		this->push_constants.flags &= ~( 1 << RenderSystemOptions::ENABLE_ANALYTICAL_AREA_LIGHTS );
	}
}

void RenderSystem::show_direct_illumination(bool enable)
{
	if (enable) {
		show_indirect_illumination(false);
		this->push_constants.flags |= ( 1 << RenderSystemOptions::SHOW_DIRECT_ILLUMINATION );
	} else {
		this->push_constants.flags &= ~( 1 << RenderSystemOptions::SHOW_DIRECT_ILLUMINATION );
	}
}


void RenderSystem::enable_median(bool enable)
{
	this->enable_median_filter = enable;
}

void RenderSystem::enable_bilateral_upsampling(bool enable)
{
	this->enable_bilateral_upsampling_filter = enable;
	if (this->enable_bilateral_upsampling_filter) {
		this->push_constants.flags |= ( 1 << RenderSystemOptions::ENABLE_BILATERAL_UPSAMPLING );
	} else {
		this->push_constants.flags &= ~( 1 << RenderSystemOptions::ENABLE_BILATERAL_UPSAMPLING );
	}
}

void RenderSystem::set_upsampling_radius(float radius)
{
	this->upsampling_filter_radius = radius;
}

void RenderSystem::force_flat_reflectors(bool force)
{
	this->force_flat_reflectors_on = force;
	if (this->force_flat_reflectors_on) {
		this->push_constants.flags |= ( 1 << RenderSystemOptions::FORCE_FLAT_REFLECTORS );
	} else {
		this->push_constants.flags &= ~( 1 << RenderSystemOptions::FORCE_FLAT_REFLECTORS );
	}
}

void RenderSystem::force_nearest_temporal_sampling(bool force)
{
	this->force_nearest_temporal_sampling_on = force;
}

void RenderSystem::show_indirect_illumination(bool enable)
{
	if (enable) {
		show_direct_illumination(false);
		this->push_constants.flags |= ( 1 << RenderSystemOptions::SHOW_INDIRECT_ILLUMINATION );
	} else {
		this->push_constants.flags &= ~( 1 << RenderSystemOptions::SHOW_INDIRECT_ILLUMINATION );
	}
}

void RenderSystem::show_albedo(bool enable)
{
	if (enable) {
		show_direct_illumination(false);
		this->push_constants.flags |= ( 1 << RenderSystemOptions::SHOW_ALBEDO );
	} else {
		this->push_constants.flags &= ~( 1 << RenderSystemOptions::SHOW_ALBEDO );
	}
}

void RenderSystem::show_gbuffer(uint32_t idx)
{
	// NOTE: final render image shows up as gbuffer 0, 
	// ddgi irradiance shows as 1, 
	// ddgi visibility shows as 2, 
	// leading to MAX_G_BUFFERS + 3 buffers
	gbuffer_override_idx=std::min(idx, (uint32_t)MAX_G_BUFFERS + 2);
}

void RenderSystem::reset_progressive_refinement() {
	if ((this->push_constants.flags & ( 1 << RenderSystemOptions::ENABLE_PROGRESSIVE_REFINEMENT )) != 0) this->push_constants.frame = this->push_constants.frame % 3;
}

// vk::RenderPass RenderSystem::get_renderpass(uint32_t camera_idx, uint32_t index)
// {
//     if(index >= renderpasses[camera_idx].size())
//         throw std::runtime_error( std::string("Error: renderpass index out of bounds"));
// 	return renderpasses[camera_idx][index];
// }

// vk::RenderPass RenderSystem::get_depth_prepass(uint32_t camera_idx, uint32_t index)
// {
//     if(index >= depth_prepasses[camera_idx].size())
//         throw std::runtime_error( std::string("Error: depthPrepasses index out of bounds"));
// 	return depth_prepasses[camera_idx][index];
// }

// uint32_t RenderSystem::get_num_renderpasses(uint32_t id)
// {
//     return (uint32_t) renderpasses[id].size();
// }

// vk::CommandBuffer RenderSystem::get_camera_command_buffer(uint32_t id) {
// 	return camera_command_buffers[id];
// }

void RenderSystem::create_camera_resources(uint32_t camera_id)
{
	if (camera_resources.find(camera_id) == camera_resources.end()) {
		camera_resources[camera_id] = {};
	}

	auto camera = Camera::Get(camera_id);
	auto &resources = camera_resources[camera_id];
	create_camera_render_passes(camera_id, camera->maxViews, camera->msaa_samples);
	create_camera_frame_buffers(camera_id, camera->maxViews);
	create_camera_query_pool(camera_id, camera->maxViews);

	for(auto renderpass : resources.primary_visibility_renderpasses) {
		setup_primary_visibility_raster_pipelines(renderpass, camera->msaa_samples, camera->should_record_depth_prepass());
	}
	for(auto renderpass : resources.shadow_map_renderpasses) {
		setup_shadow_map_raster_pipelines(renderpass, camera->msaa_samples, camera->should_record_depth_prepass());
	}

	if (camera->should_record_depth_prepass())
	{
		for(auto renderpass : resources.primary_visibility_depth_prepasses) {
			setup_depth_prepass_raster_pipelines(renderpass, camera->msaa_samples, camera->should_record_depth_prepass(), USED_PRIMARY_VISIBILITY_G_BUFFERS);
		}
		for(auto renderpass : resources.shadow_map_depth_prepasses) {
			setup_depth_prepass_raster_pipelines(renderpass, camera->msaa_samples, camera->should_record_depth_prepass(), USED_SHADOW_MAP_G_BUFFERS);
		}
	}
}

void RenderSystem::create_camera_query_pool(uint32_t camera_id, uint32_t max_views)
{
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();
	
	vk::QueryPoolCreateInfo queryPoolInfo;
	queryPoolInfo.queryType = vk::QueryType::eOcclusion;
	queryPoolInfo.queryCount = MAX_ENTITIES * max_views;

	auto &resource = camera_resources[camera_id];
	
	resource.queryPool = device.createQueryPool(queryPoolInfo);
	resource.index_queried = std::vector<bool>();
	resource.next_index_queried = std::vector<bool>();
	resource.queryResults = std::vector<uint64_t>();
	resource.previousQueryResults = std::vector<uint64_t>();

	resource.index_queried.resize(MAX_ENTITIES * max_views, 0);
	resource.next_index_queried.resize(MAX_ENTITIES * max_views, 0);

	resource.queryResults.resize(MAX_ENTITIES * max_views + 1, 0);
	resource.previousQueryResults.resize(MAX_ENTITIES * max_views + 1, 0);
}

void RenderSystem::create_camera_frame_buffers(uint32_t camera_id, uint32_t layers) {
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();
	auto camera = Camera::Get(camera_id);
    auto &cam_res = camera_resources[camera->get_id()];

	bool use_multiview = camera->should_use_multiview();
	auto &renderTexture = camera->renderTexture;
	auto &resolveTexture = camera->resolveTexture;

	/* Primary Visibility Frame Buffers */
	{
		cam_res.primary_visibility_renderpass_framebuffers.clear();
		cam_res.primary_visibility_depth_prepass_framebuffers.clear();

		/* Attachments for depth, all g buffers, and resolve buffers */
		vk::ImageView attachments[1 + USED_PRIMARY_VISIBILITY_G_BUFFERS];

		/* If we're using multiview, we only need one framebuffer. */
		/* Otherwise, we need a framebuffer per view */
		for (uint32_t i = 0; i < (use_multiview ? 1 : layers); i++)
		{
			// attachments[0] = (use_multiview) ? renderTexture->get_color_image_view() : renderTexture->get_color_image_view_layers()[i];
			for (uint g_idx = 0; g_idx < USED_PRIMARY_VISIBILITY_G_BUFFERS; g_idx++)
				attachments[g_idx] = (use_multiview) ?  renderTexture->get_g_buffer_image_view(g_idx) : renderTexture->get_g_buffer_image_view_layers(g_idx)[i];
			attachments[USED_PRIMARY_VISIBILITY_G_BUFFERS] = (use_multiview) ? renderTexture->get_depth_image_view() : renderTexture->get_depth_image_view_layers()[i];
			
			vk::FramebufferCreateInfo fbufCreateInfo;
			fbufCreateInfo.renderPass = cam_res.primary_visibility_renderpasses[i];
			fbufCreateInfo.attachmentCount = 1 + USED_PRIMARY_VISIBILITY_G_BUFFERS; // one depth, plus all used g buffers
			fbufCreateInfo.pAttachments = attachments;
			fbufCreateInfo.width = renderTexture->get_width();
			fbufCreateInfo.height = renderTexture->get_height();
			fbufCreateInfo.layers = (use_multiview) ? renderTexture->get_total_layers() : 1;

			cam_res.primary_visibility_renderpass_framebuffers.push_back(device.createFramebuffer(fbufCreateInfo));

			if (camera->should_record_depth_prepass())
			{
				fbufCreateInfo.renderPass = cam_res.primary_visibility_depth_prepasses[i];
				cam_res.primary_visibility_depth_prepass_framebuffers.push_back(device.createFramebuffer(fbufCreateInfo));
			}
		}
	}

	/* Shadow Map Frame Buffers */
	{
		cam_res.shadow_map_renderpass_framebuffers.clear();
		cam_res.shadow_map_depth_prepass_framebuffers.clear();

		/* Attachments for color, depth, and resolve buffers */
		vk::ImageView attachments[3];

		/* If we're using multiview, we only need one framebuffer. */
		/* Otherwise, we need a framebuffer per view */
		for (uint32_t i = 0; i < (use_multiview ? 1 : layers); i++)
		{
			attachments[0] = (use_multiview) ? renderTexture->get_color_image_view() : renderTexture->get_color_image_view_layers()[i];
			attachments[1] = (use_multiview) ? renderTexture->get_depth_image_view() : renderTexture->get_depth_image_view_layers()[i];
			if (camera->msaa_samples != 1)
				attachments[2] = (use_multiview) ? resolveTexture->get_color_image_view() : resolveTexture->get_color_image_view_layers()[i];

			vk::FramebufferCreateInfo fbufCreateInfo;
			fbufCreateInfo.renderPass = cam_res.shadow_map_renderpasses[i];
			fbufCreateInfo.attachmentCount = (camera->msaa_samples == 1) ? 2 : 3;
			fbufCreateInfo.pAttachments = attachments;
			fbufCreateInfo.width = renderTexture->get_width();
			fbufCreateInfo.height = renderTexture->get_height();
			fbufCreateInfo.layers = (use_multiview) ? renderTexture->get_total_layers() : 1;

			cam_res.shadow_map_renderpass_framebuffers.push_back(device.createFramebuffer(fbufCreateInfo));

			if (camera->should_record_depth_prepass())
			{
				fbufCreateInfo.renderPass = cam_res.shadow_map_depth_prepasses[i];
				cam_res.shadow_map_depth_prepass_framebuffers.push_back(device.createFramebuffer(fbufCreateInfo));
			}
		}
	}
}

void RenderSystem::create_camera_render_passes(uint32_t camera_id, uint32_t layers, uint32_t sample_count)
{
	/* NOTE: BEGIN RENDERPASS ASSUMES DEPTH ATTACHMENT IS THE LAST ATTACHMENT */
	auto camera = Camera::Get(camera_id);
	auto &cam_res = camera_resources[camera->get_id()];

    cam_res.primary_visibility_renderpasses.clear();
    cam_res.primary_visibility_depth_prepasses.clear();
    cam_res.shadow_map_renderpasses.clear();
    cam_res.shadow_map_depth_prepasses.clear();
    
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	auto sampleFlag = vulkan->highest(vulkan->min(vulkan->get_closest_sample_count_flag(sample_count), vulkan->get_msaa_sample_flags()));

	/* Primary Visibility Renderpasses ( Note: does not support MSAA resolve yet. Only renders to g buffers ) */
	{
		std::array<vk::AttachmentDescription, USED_PRIMARY_VISIBILITY_G_BUFFERS> gbufferAttachments;
		std::array<vk::AttachmentReference, USED_PRIMARY_VISIBILITY_G_BUFFERS> gbufferAttachmentRefs;

		for (uint32_t g_idx = 0; g_idx < USED_PRIMARY_VISIBILITY_G_BUFFERS; ++g_idx) {
			gbufferAttachments[g_idx].format = camera->renderTexture->get_color_format(); // TODO
			gbufferAttachments[g_idx].samples = sampleFlag;
			gbufferAttachments[g_idx].loadOp = vk::AttachmentLoadOp::eDontCare; // Dont clear g buffers
			gbufferAttachments[g_idx].storeOp = vk::AttachmentStoreOp::eStore;
			gbufferAttachments[g_idx].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
			gbufferAttachments[g_idx].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
			gbufferAttachments[g_idx].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
			gbufferAttachments[g_idx].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
			gbufferAttachmentRefs[g_idx].attachment = g_idx; // note: starting at 0, see primary visibility frame buffer for details
			gbufferAttachmentRefs[g_idx].layout = vk::ImageLayout::eColorAttachmentOptimal;
		}

		vk::AttachmentDescription depthAttachment;
		depthAttachment.format = camera->renderTexture->get_depth_format();
		depthAttachment.samples = sampleFlag;
		depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
		depthAttachment.storeOp = vk::AttachmentStoreOp::eStore; 
		depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		depthAttachment.initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
		depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		vk::AttachmentReference depthAttachmentRef;
		depthAttachmentRef.attachment = USED_PRIMARY_VISIBILITY_G_BUFFERS;
		depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		// Use subpass dependencies for layout transitions
		std::array<vk::SubpassDependency, 2> dependencies;
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
		dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
		dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
		dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
		dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
		dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
		dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

		std::vector<vk::AttachmentReference> attachmentReferences;
		for (uint32_t g_idx = 0; g_idx < USED_PRIMARY_VISIBILITY_G_BUFFERS; ++g_idx) {
			attachmentReferences.push_back(gbufferAttachmentRefs[g_idx]);
		}
		vk::SubpassDescription subpass;
		subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpass.colorAttachmentCount = (uint32_t) attachmentReferences.size();
		subpass.pColorAttachments = attachmentReferences.data();
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		// Support for multiview
		uint32_t mask = 0;
		for (uint32_t i = 0; i < layers; ++i)
			mask |= 1 << i;

		const uint32_t viewMasks[] = {mask};
		const uint32_t correlationMasks[] = {mask};

		vk::RenderPassMultiviewCreateInfo renderPassMultiviewInfo;
		renderPassMultiviewInfo.subpassCount = 1;
		renderPassMultiviewInfo.pViewMasks = viewMasks;
		renderPassMultiviewInfo.dependencyCount = 0;
		renderPassMultiviewInfo.pViewOffsets = NULL;
		renderPassMultiviewInfo.correlationMaskCount = 1;
		renderPassMultiviewInfo.pCorrelationMasks = correlationMasks;

		std::vector<vk::AttachmentDescription> attachments;
		for (uint32_t g_idx = 0; g_idx < USED_PRIMARY_VISIBILITY_G_BUFFERS; ++g_idx) attachments.push_back(gbufferAttachments[g_idx]);
		attachments.push_back(depthAttachment);

		vk::RenderPassCreateInfo renderPassInfo;
		renderPassInfo.attachmentCount = (uint32_t) attachments.size();
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = (uint32_t) dependencies.size();
		renderPassInfo.pDependencies = dependencies.data();
		renderPassInfo.pNext = (camera->should_use_multiview()) ? (&renderPassMultiviewInfo) : nullptr;

		int iterations = (camera->should_use_multiview()) ? 1 : layers;
		for(int i = 0; i < iterations; i++) {
			if (camera->should_record_depth_prepass())
			{
				/* Depth prepass will transition from undefined to optimal now. */
				for (uint32_t g_idx = 0; g_idx < USED_PRIMARY_VISIBILITY_G_BUFFERS; ++g_idx) {
					attachments[g_idx].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
					attachments[g_idx].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
				}

				/* Depth attachment stuff */
				attachments[USED_PRIMARY_VISIBILITY_G_BUFFERS].initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
				attachments[USED_PRIMARY_VISIBILITY_G_BUFFERS].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
				attachments[USED_PRIMARY_VISIBILITY_G_BUFFERS].loadOp = vk::AttachmentLoadOp::eDontCare;
				attachments[USED_PRIMARY_VISIBILITY_G_BUFFERS].storeOp = vk::AttachmentStoreOp::eDontCare;
				cam_res.primary_visibility_renderpasses.push_back(device.createRenderPass(renderPassInfo));
				
				/* Transition from undefined to attachment optimal in depth prepass. */
				for (uint32_t g_idx = 0; g_idx < USED_PRIMARY_VISIBILITY_G_BUFFERS; ++g_idx) {
					attachments[g_idx].loadOp = vk::AttachmentLoadOp::eDontCare; // dont clear
					attachments[g_idx].storeOp = vk::AttachmentStoreOp::eDontCare;
					attachments[g_idx].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
					attachments[g_idx].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
					attachments[g_idx].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
					attachments[g_idx].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
				}

				/* Depth attachment stuff */
				attachments[USED_PRIMARY_VISIBILITY_G_BUFFERS].initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
				attachments[USED_PRIMARY_VISIBILITY_G_BUFFERS].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
				attachments[USED_PRIMARY_VISIBILITY_G_BUFFERS].loadOp = vk::AttachmentLoadOp::eClear;
				attachments[USED_PRIMARY_VISIBILITY_G_BUFFERS].storeOp = vk::AttachmentStoreOp::eStore;
				cam_res.primary_visibility_depth_prepasses.push_back(device.createRenderPass(renderPassInfo));
			}
			else {
				cam_res.primary_visibility_renderpasses.push_back(device.createRenderPass(renderPassInfo));
			}
		}
	}

	/* Shadow Map Renderpasses */
	{
		// Shadow "color" attachment
		vk::AttachmentDescription colorAttachment;
		colorAttachment.format = camera->renderTexture->get_color_format(); // TODO
		colorAttachment.samples = sampleFlag;
		colorAttachment.loadOp = vk::AttachmentLoadOp::eClear; // clears image to black
		colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
		colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		colorAttachment.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
		colorAttachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

		vk::AttachmentReference colorAttachmentRef;
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

		vk::AttachmentDescription depthAttachment;
		depthAttachment.format = camera->renderTexture->get_depth_format();
		depthAttachment.samples = sampleFlag;
		depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
		depthAttachment.storeOp = vk::AttachmentStoreOp::eStore; 
		depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		depthAttachment.initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
		depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		vk::AttachmentReference depthAttachmentRef;
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		// "Color" resolve attachment
		vk::AttachmentDescription colorResolveAttachment;
		colorResolveAttachment.format = camera->renderTexture->get_color_format(); // TODO
		colorResolveAttachment.samples = vk::SampleCountFlagBits::e1;
		colorResolveAttachment.loadOp = vk::AttachmentLoadOp::eDontCare; // dont clear
		colorResolveAttachment.storeOp = vk::AttachmentStoreOp::eStore;
		colorResolveAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		colorResolveAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		colorResolveAttachment.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
		colorResolveAttachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

		vk::AttachmentReference colorResolveAttachmentRef;
		colorResolveAttachmentRef.attachment = 2;
		colorResolveAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

		// Use subpass dependencies for layout transitions
		std::array<vk::SubpassDependency, 2> dependencies;
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
		dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
		dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
		dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
		dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
		dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
		dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

		std::vector<vk::AttachmentReference> attachmentReferences;
		attachmentReferences.push_back(colorAttachmentRef);
		
		vk::SubpassDescription subpass;
		subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpass.colorAttachmentCount = (uint32_t) attachmentReferences.size();
		subpass.pColorAttachments = attachmentReferences.data();
		subpass.pDepthStencilAttachment = &depthAttachmentRef;
		if (camera->msaa_samples != 1)
			subpass.pResolveAttachments = &colorResolveAttachmentRef;

		// Support for multiview
		uint32_t mask = 0;
		for (uint32_t i = 0; i < layers; ++i)
			mask |= 1 << i;

		const uint32_t viewMasks[] = {mask};
		const uint32_t correlationMasks[] = {mask};

		vk::RenderPassMultiviewCreateInfo renderPassMultiviewInfo;
		renderPassMultiviewInfo.subpassCount = 1;
		renderPassMultiviewInfo.pViewMasks = viewMasks;
		renderPassMultiviewInfo.dependencyCount = 0;
		renderPassMultiviewInfo.pViewOffsets = NULL;
		renderPassMultiviewInfo.correlationMaskCount = 1;
		renderPassMultiviewInfo.pCorrelationMasks = correlationMasks;

		std::vector<vk::AttachmentDescription> attachments;
		attachments.push_back(colorAttachment);
		attachments.push_back(depthAttachment);	
		if (camera->msaa_samples != 1) attachments.push_back(colorResolveAttachment);
		
		vk::RenderPassCreateInfo renderPassInfo;
		renderPassInfo.attachmentCount = (uint32_t) attachments.size();
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = (uint32_t) dependencies.size();
		renderPassInfo.pDependencies = dependencies.data();
		renderPassInfo.pNext = (camera->should_use_multiview()) ? (&renderPassMultiviewInfo) : nullptr;

		int iterations = (camera->should_use_multiview()) ? 1 : layers;
		for(int i = 0; i < iterations; i++) {
			if (camera->should_record_depth_prepass())
			{
				/* Depth prepass will transition from undefined to optimal now. */
				attachments[0].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
				attachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

				/* Depth attachment stuff */
				attachments[1].initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
				attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
				attachments[1].loadOp = vk::AttachmentLoadOp::eDontCare;
				attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
				cam_res.shadow_map_renderpasses.push_back(device.createRenderPass(renderPassInfo));
				
				/* Transition from undefined to attachment optimal in depth prepass. */
				attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // dont clear
				attachments[0].storeOp = vk::AttachmentStoreOp::eDontCare;
				attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
				attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
				attachments[0].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
				attachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

				/* Depth attachment stuff */
				attachments[1].initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
				attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
				attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
				attachments[1].storeOp = vk::AttachmentStoreOp::eStore;
				cam_res.shadow_map_depth_prepasses.push_back(device.createRenderPass(renderPassInfo));
			}
			else {
				cam_res.shadow_map_renderpasses.push_back(device.createRenderPass(renderPassInfo));
			}
		}
	}
}

void RenderSystem::begin_renderpass(vk::CommandBuffer command_buffer, vk::RenderPass renderpass, vk::Framebuffer framebuffer, uint32_t camera_id, uint32_t num_g_buffers)
{
	auto camera = Camera::Get(camera_id);
	auto &cam_res = camera_resources[camera->get_id()];

	if (!camera->should_record_depth_prepass()) {
		auto m = camera->render_complete_mutex.get();
		std::lock_guard<std::mutex> lk(*m);
		camera->render_ready = false;
	}

	camera->renderTexture->make_renderable(command_buffer);
	
	vk::RenderPassBeginInfo rpInfo;
	rpInfo.renderPass = renderpass;
	rpInfo.framebuffer = framebuffer;
    rpInfo.renderArea.offset = vk::Offset2D{0, 0};
	rpInfo.renderArea.extent = vk::Extent2D{camera->renderTexture->get_width(), camera->renderTexture->get_height()};

	std::vector<vk::ClearValue> clearValues(num_g_buffers, vk::ClearColorValue(std::array<float, 4>{camera->clearColor.r, camera->clearColor.g, camera->clearColor.b, camera->clearColor.a}));
	if (!camera->should_record_depth_prepass()){
		clearValues.push_back(vk::ClearValue());
		clearValues[clearValues.size() - 1].depthStencil = vk::ClearDepthStencilValue(camera->clearDepth, camera->clearStencil);
	}

	rpInfo.clearValueCount = (uint32_t)clearValues.size();
	rpInfo.pClearValues = clearValues.data();

	/* Start the render pass */
	command_buffer.beginRenderPass(rpInfo, vk::SubpassContents::eInline);

	/* Set viewport*/
	vk::Viewport viewport;
	viewport.width = (float)camera->renderTexture->get_width();
	viewport.height = -(float)camera->renderTexture->get_height();
	viewport.y = (float)camera->renderTexture->get_height();
	viewport.x = 0;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	command_buffer.setViewport(0, {viewport});

	/* Set Scissors */
	vk::Rect2D rect2D;
	rect2D.extent.width = camera->renderTexture->get_width();
	rect2D.extent.height = camera->renderTexture->get_height();
	rect2D.offset.x = 0;
	rect2D.offset.y = 0;

	command_buffer.setScissor(0, {rect2D});
}

void RenderSystem::end_renderpass(vk::CommandBuffer command_buffer, uint32_t camera_id) {
	auto camera = Camera::Get(camera_id);
	auto &cam_res = camera_resources[camera->get_id()];
    
	command_buffer.endRenderPass();

	camera->renderTexture->overrideColorImageLayout(vk::ImageLayout::eColorAttachmentOptimal);	
	camera->renderTexture->overrideDepthImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);	

	// /* hack for now*/ 
	if (camera->renderOrder < 0) {
		camera->renderTexture->make_samplable(command_buffer);
	}
}

void RenderSystem::begin_depth_prepass(vk::CommandBuffer command_buffer, vk::RenderPass renderpass, vk::Framebuffer framebuffer, uint32_t camera_id, uint32_t num_g_buffers)
{
	auto camera = Camera::Get(camera_id);
	auto &cam_res = camera_resources[camera_id];

	if (!camera->use_depth_prepass)
		throw std::runtime_error( std::string("Error: depth prepass not enabled on this camera"));

	auto m = camera->render_complete_mutex.get();
	std::lock_guard<std::mutex> lk(*m);
	camera->render_ready = false;
	
	camera->renderTexture->make_renderable(command_buffer);

	vk::RenderPassBeginInfo rpInfo;
	rpInfo.renderPass = renderpass;
	rpInfo.framebuffer = framebuffer;
    rpInfo.renderArea.offset = vk::Offset2D{0, 0};
	rpInfo.renderArea.extent = vk::Extent2D{camera->renderTexture->get_width(), camera->renderTexture->get_height()};

	std::vector<vk::ClearValue> clearValues(num_g_buffers, vk::ClearColorValue(std::array<float, 4>{camera->clearColor.r, camera->clearColor.g, camera->clearColor.b, camera->clearColor.a}));
	clearValues.push_back(vk::ClearValue());
	clearValues[clearValues.size() - 1].depthStencil = vk::ClearDepthStencilValue(camera->clearDepth, camera->clearStencil);
	
	rpInfo.clearValueCount = (uint32_t)clearValues.size();
	rpInfo.pClearValues = clearValues.data();

	/* Start the render pass */
	command_buffer.beginRenderPass(rpInfo, vk::SubpassContents::eInline);

	/* Set viewport*/
	vk::Viewport viewport;
	viewport.width = (float)camera->renderTexture->get_width();
	viewport.height = -(float)camera->renderTexture->get_height();
	viewport.y = (float)camera->renderTexture->get_height();
	viewport.x = 0;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	command_buffer.setViewport(0, {viewport});

	/* Set Scissors */
	vk::Rect2D rect2D;
	rect2D.extent.width = camera->renderTexture->get_width();
	rect2D.extent.height = camera->renderTexture->get_height();
	rect2D.offset.x = 0;
	rect2D.offset.y = 0;

	command_buffer.setScissor(0, {rect2D});
}

void RenderSystem::end_depth_prepass(vk::CommandBuffer command_buffer, uint32_t camera_id) {
	auto camera = Camera::Get(camera_id);
	auto &cam_res = camera_resources[camera->get_id()];
	if (!camera->should_record_depth_prepass())
		throw std::runtime_error( std::string("Error: depth prepass not enabled on this camera"));
	
	command_buffer.endRenderPass();

	camera->renderTexture->overrideColorImageLayout(vk::ImageLayout::eColorAttachmentOptimal);	
	camera->renderTexture->overrideDepthImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);	
}

void RenderSystem::reset_query_pool(vk::CommandBuffer command_buffer, uint32_t camera_id)
{
	auto camera = Camera::Get(camera_id);
	auto &resource = camera_resources[camera_id];
	// This might need to be changed, depending on if MAX_ENTITIES is the "number of queries in the query pool".
	// The spec might be referring to visable_entities.size() instead...
	// if (queryDownloaded) {
		command_buffer.resetQueryPool(resource.queryPool, 0, MAX_ENTITIES*camera->maxViews); 
		camera->max_queried = 0;
	// }

	camera->queryRecorded = false;

}

void RenderSystem::download_query_pool_results(uint32_t camera_id)
{
	auto camera = Camera::Get(camera_id);
	auto &resource = camera_resources[camera_id];

	/* If we arent recording a depth prepass for this camera, we won't have queried for visibility. */
	if (!camera->should_record_depth_prepass()) return;

	// This seems to not flicker if i don't sort/frustum cull, 
	// so there's something that has to do with how I am 
	// reordering draws which messes up visibility
	if (!camera->visibilityTestingPaused)
	{
		if (resource.queryPool == vk::QueryPool()) return;

		if (!camera->queryRecorded) return;
		
		if (camera->max_queried == 0) return;

		auto vulkan = Libraries::Vulkan::Get();
		auto device = vulkan->get_device();
		// vulkan->flush_queues();


		uint32_t num_queries = uint32_t(camera->max_queried);
		uint32_t first_query = 0;
		uint32_t stride = sizeof(uint64_t);

		bool with_availability = false;
		std::vector<uint64_t> temp((MAX_ENTITIES*camera->maxViews + ( (with_availability) ? 1 : 0)), 0);
		
		// I believe partial will write a value between 0 and the number of fragments inclusive.
		// With availablility sets things to non-zero if they are available

		// If availability is used in combination with wait, are results ever 1, marking available?
		
		uint32_t data_size = sizeof(uint64_t) * (MAX_ENTITIES*camera->maxViews + ( (with_availability) ? 1 : 0)); // Why do I need an extra number here?
		
		bool previous_value = false;
		
		std::pair<uint32_t, uint32_t> query_range;

		uint32_t num_segments = 0;

		/* Find a range of queries to download */
		for (uint32_t i = 0; i <= resource.next_index_queried.size(); ++i) 
		{
			auto next_queried = (i == resource.next_index_queried.size()) ? false : resource.next_index_queried[i];

			/* Start of segment */
			if ((previous_value == 0) && (next_queried == 1))
			{
				query_range.first = i;
				query_range.second = i + 1;
				num_segments ++;
			}

			/* Extending segment */
			else if ((previous_value == 1) && (next_queried == 1)) {
				query_range.second++;
			}

			/* End of segment, download queries */
			else if ((previous_value == 1) && (next_queried == 0))
			{
				auto result = device.getQueryPoolResults(resource.queryPool, query_range.first, query_range.second - query_range.first,
					sizeof(uint64_t) * (query_range.second - query_range.first), &resource.queryResults[query_range.first], sizeof(uint64_t), 
					vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::ePartial | vk::QueryResultFlagBits::eWait);

				// if (result == vk::Result::eSuccess) {
				
				// }
				camera->queryDownloaded = true;
			}

			previous_value = next_queried;
		}
		
		resource.index_queried = resource.next_index_queried;
		std::fill(resource.next_index_queried.begin(), resource.next_index_queried.end(), false);

			
		
		// /* This is difficult, since queries are quite possibly sparse, as draw order changes and objects become visible/invisible. */
		// auto result = device.getQueryPoolResults(queryPool, first_query, num_queries, 
		// 		data_size, temp.data(), stride, // wait locks up... with availability requires an extra integer?
		// 		vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait/* | vk::QueryResultFlagBits::eWait*/);// | vk::QueryResultFlagBits::eWait);
		
		// // | vk::QueryResultFlagBits::ePartial
		// if (result == vk::Result::eSuccess) {
		// 	previousQueryResults = queryResults;
		// 	index_queried = next_index_queried[get_id()];
		// 	std::fill(next_index_queried[get_id()].begin(), next_index_queried[get_id()].end(), false);
		// 	// entity_to_query_idx = next_entity_to_query_idx;
		// 	// queryResults = temp;

		// 	for (uint32_t i = 0; i < max_queried; ++i) {
		// 		// if (temp[i] > 0) {
		// 			// a result of 1 means ready, and not visible. 
		// 			// anything larger than 1 is both ready _and_ visible.
		// 			queryResults[i] = temp[i];// - 1;// - 1;
					
		// 			// - 1;//(((int64_t)temp[i]) - 1) <= 0 ? 0 : 1;// std::max(temp[i] - 1, 0);
		// 		// }
		// 	}
		// 	queryDownloaded = true;
		// }

		// // previousEntityToDrawIdx = entityToDrawIdx;
		// else {
		// 	//std::cout<<"camera " << id << " " << vk::to_string(result)<<std::endl;
		// }
	}
}

std::vector<uint64_t> & RenderSystem::get_query_pool_results(uint32_t camera_id)
{
	return camera_resources[camera_id].queryResults;
}

// TODO: Improve performance of this. We really shouldn't be using a map like this...
bool RenderSystem::is_entity_visible(uint32_t renderpass_idx, uint32_t camera_id, uint32_t entity_id)
{
	return camera_resources[camera_id].queryResults[/*entity_to_query_idx[*/entity_id + (renderpass_idx * MAX_ENTITIES)/*]*/] > 0;
}

void RenderSystem::begin_visibility_query(vk::CommandBuffer command_buffer, uint32_t camera_id, uint32_t renderpass_idx, uint32_t entity_id)
{
	auto camera = Camera::Get(camera_id);
	auto &resource = camera_resources[camera_id];
	if (!camera->visibilityTestingPaused)
	{
		camera->queryRecorded = true;
		camera->queryDownloaded = false;
		resource.next_index_queried[entity_id + (renderpass_idx * MAX_ENTITIES)] = true; //entity_id + (renderpass_idx * MAX_ENTITIES);
		command_buffer.beginQuery(resource.queryPool, entity_id + (renderpass_idx * MAX_ENTITIES), vk::QueryControlFlags());
	}
}

void RenderSystem::end_visibility_query(vk::CommandBuffer command_buffer, uint32_t camera_id, uint32_t renderpass_idx, uint32_t entity_id)
{
	auto camera = Camera::Get(camera_id);
	auto &resource = camera_resources[camera_id];
	if (!camera->visibilityTestingPaused)
	{
		camera->max_queried = std::max((uint64_t)(entity_id + (renderpass_idx * MAX_ENTITIES)) + 1, camera->max_queried);
		command_buffer.endQuery(resource.queryPool, entity_id + (renderpass_idx * MAX_ENTITIES));
	}
}

enum side { LEFT = 0, RIGHT = 1, TOP = 3, BOTTOM = 2, BACK = 4, FRONT = 5 };

bool checkSphere(std::array<glm::vec4, 6> &planes, glm::vec3 pos, float radius)
{
	// The point is the center of
	// the radius.  So, the point might be outside of the frustum, but it doesn't
	// mean that the rest of the sphere is.  It could be half and half.  So instead of
	// checking if it's less than 0, we need to add on the radius to that.  Say the
	// equation produced -2, which means the center of the sphere is the distance of
	// 2 behind the plane.  Well, what if the radius was 5?  The sphere is still inside,
	// so we would say, if(-2 < -5) then we are outside.  In that case it's false,
	// so we are inside of the frustum, but a distance of 3.  This is reflected below.

	for (auto i = 0; i < planes.size(); i++)
	{
		if ((planes[i].x * pos.x) + (planes[i].y * pos.y) + (planes[i].z * pos.z) + planes[i].w < -radius)
		{
			return false;
		}
	}
	return true;
}

std::vector<std::vector<VisibleEntityInfo>> RenderSystem::get_visible_entities(uint32_t camera_id, uint32_t camera_entity_id) 
{
	auto camera = Camera::Get(camera_id);
	if (camera->visibilityTestingPaused) return camera->frustum_culling_results;

	std::vector<std::vector<VisibleEntityInfo>> visible_entity_maps;

	Entity* entities = Entity::GetFront();
	auto camera_entity = Entity::Get(camera_entity_id);
	if (!camera_entity) return {};
	if (camera_entity->get_transform() == nullptr) return {};
	auto cam_transform = camera_entity->get_transform();
	if (!cam_transform) return {};
	glm::vec3 cam_pos = cam_transform->get_position();
	
	visible_entity_maps.resize(camera->get_max_views(), std::vector<VisibleEntityInfo>(Entity::GetCount()));

	/* Compute visibility and distance */
	for (uint32_t view_idx = 0; view_idx < camera->usedViews; ++view_idx) {
		glm::mat4 camera_world_to_local = camera->camera_struct.multiviews[view_idx].proj * camera->camera_struct.multiviews[view_idx].view * cam_transform->get_parent_to_local_rotation_matrix() * 
			cam_transform->get_parent_to_local_translation_matrix();

		for (uint32_t i = 0; i < Entity::GetCount(); ++i) 
		{
			visible_entity_maps[view_idx][i].entity = &entities[i];
			visible_entity_maps[view_idx][i].entity_index = i;

			/* If any of these are true, the object is invisible. */
			if ((i == camera_entity_id) || (!entities[i].is_initialized()) || (!entities[i].get_mesh()) || (!entities[i].get_transform()) || (!entities[i].get_material())) {
				visible_entity_maps[view_idx][i].distance = std::numeric_limits<float>::max();
				visible_entity_maps[view_idx][i].visible = false;
			} else {

				std::array<glm::vec4, 6> planes;

				/* Get projection planes for frustum/sphere intersection */
				auto m = camera_world_to_local * entities[i].get_transform()->get_local_to_world_matrix();
				planes[LEFT].x = m[0].w + m[0].x; planes[LEFT].y = m[1].w + m[1].x; planes[LEFT].z = m[2].w + m[2].x; planes[LEFT].w = m[3].w + m[3].x;
				planes[RIGHT].x = m[0].w - m[0].x; planes[RIGHT].y = m[1].w - m[1].x; planes[RIGHT].z = m[2].w - m[2].x; planes[RIGHT].w = m[3].w - m[3].x;
				planes[TOP].x = m[0].w - m[0].y; planes[TOP].y = m[1].w - m[1].y; planes[TOP].z = m[2].w - m[2].y; planes[TOP].w = m[3].w - m[3].y;
				planes[BOTTOM].x = m[0].w + m[0].y; planes[BOTTOM].y = m[1].w + m[1].y; planes[BOTTOM].z = m[2].w + m[2].y; planes[BOTTOM].w = m[3].w + m[3].y;
				planes[BACK].x = m[0].w + m[0].z; planes[BACK].y = m[1].w + m[1].z; planes[BACK].z = m[2].w + m[2].z; planes[BACK].w = m[3].w + m[3].z;
				planes[FRONT].x = m[0].w - m[0].z; planes[FRONT].y = m[1].w - m[1].z; planes[FRONT].z = m[2].w - m[2].z; planes[FRONT].w = m[3].w - m[3].z;
				for (auto i = 0; i < planes.size(); i++) planes[i] /= sqrtf(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);

				auto centroid = entities[i].get_mesh()->get_centroid();
				auto radius = entities[i].get_mesh()->get_bounding_sphere_radius();
				bool entity_seen = checkSphere(planes, centroid, radius);
				if (entity_seen) {
					auto m_cam_pos = entities[i].get_transform()->get_world_to_local_matrix() * glm::vec4(cam_pos.x, cam_pos.y, cam_pos.z, 1.0f);
					auto to_camera = glm::normalize(glm::vec3(m_cam_pos) - centroid);
					float centroid_dist = glm::distance(glm::vec3(m_cam_pos), centroid);
					if (radius < centroid_dist)
						to_camera = to_camera * radius;

					auto w_to_camera = glm::vec3(entities[i].get_transform()->get_local_to_world_matrix() * glm::vec4(to_camera.x, to_camera.y, to_camera.z, 0.0));
					auto w_centroid = glm::vec3(entities[i].get_transform()->get_local_to_world_matrix() * glm::vec4(centroid.x, centroid.y, centroid.z, 1.0));
					visible_entity_maps[view_idx][i].visible = true;
					visible_entity_maps[view_idx][i].distance = glm::distance(cam_pos, w_centroid + w_to_camera);
				} else {
					visible_entity_maps[view_idx][i].distance = std::numeric_limits<float>::max();
					visible_entity_maps[view_idx][i].visible = false;
				}
			}
		}
	}

	/* Sort by depth */
	for (uint32_t view_idx = 0; view_idx < camera->usedViews; ++view_idx) {
		std::sort(visible_entity_maps[view_idx].begin(), visible_entity_maps[view_idx].end(), std::less<VisibleEntityInfo>());	
	}
	camera->frustum_culling_results = visible_entity_maps;
	return visible_entity_maps;
}


} // namespace Systems
