// ┌──────────────────────────────────────────────────────────────────┐
// │  Camera                                                          │
// └──────────────────────────────────────────────────────────────────┘

#pragma once

#include <vulkan/vulkan.hpp>
#include <mutex>

#include "Pluto/Tools/StaticFactory.hxx"
#include "Pluto/Camera/CameraStruct.hxx"

class Texture;
class Entity;

enum RenderMode : uint32_t;

class Camera : public StaticFactory
{
  public:
	/* Creates a camera, which can be used to record the scene. Can be used to render to several texture layers for use in cubemap rendering/VR renderpasses. 
		Note, "layers" parameter is ignored if cubemap is enabled. */
	static Camera *Create(std::string name, bool cubemap = false, uint32_t tex_width = 0, uint32_t tex_height = 0, uint32_t msaa_samples = 1, uint32_t layers = 1, bool use_depth_prepass = true, bool use_multiview = false);

	/* Retrieves a camera component by name. */
	static Camera *Get(std::string name);

	/* Retrieves a camera component by id. */
	static Camera *Get(uint32_t id);

	/* Returns a pointer to the list of camera components. */
	static Camera *GetFront();

	/* Returns the total number of reserved cameras. */
	static uint32_t GetCount();

	/* Returns a vector containing a list of camera components with the given order. */
	static std::vector<Camera *> GetCamerasByOrder(uint32_t order);

	/* Deallocates a camera with the given name. */
	static void Delete(std::string name);

	/* Deallocates a camera with the given id. */
	static void Delete(uint32_t id);

	/* Initializes the Camera factory. Loads any default components. */
	static void Initialize();

	/* Transfers all camera components to an SSBO */
	static void UploadSSBO(vk::CommandBuffer command_buffer);

	/* Returns the SSBO vulkan buffer handle */
	static vk::Buffer GetSSBO();

	/* Returns the size in bytes of the current camera SSBO. */
	static uint32_t GetSSBOSize();

	/* Releases vulkan resources */
	static void CleanUp();

	/* Creates an uninitialized camera. Useful for preallocation. */
	Camera();

	/* Creates a camera with the given name and id. */
	Camera(std::string name, uint32_t id);

	/* Constructs an orthographic projection for the given multiview. */
	// bool set_orthographic_projection(float left, float right, float bottom, float top, float near_pos, uint32_t multiview = 0);

	/* Constructs a reverse Z perspective projection for the given multiview. */
	void set_perspective_projection(float fov_in_radians, float width, float height, float near_pos, uint32_t multiview = 0);

	/* Uses an external projection matrix for the given multiview. 
		Note, the projection must be a reversed Z projection. */
	void set_custom_projection(glm::mat4 custom_projection, float near_pos, uint32_t multiview = 0);

	/* Returns the near position of the given multiview */
	float get_near_pos(uint32_t multiview = 0);

	/* Returns the entity transform to camera matrix for the given multiview. 
		This additional transform is applied on top of an entity transform during a renderpass
		to see a particular "view". */
	glm::mat4 get_view(uint32_t multiview = 0);

	/* Sets the entity transform to camera matrix for the given multiview. 
		This additional transform is applied on top of an entity transform during a renderpass
		to see a particular "view". */
	void set_view(glm::mat4 view, uint32_t multiview = 0);

	/* Returns the camera to projection matrix for the given multiview.
		This transform can be used to achieve perspective (eg a vanishing point), or for scaling
		an orthographic view. */
	glm::mat4 get_projection(uint32_t multiview = 0);

	/* Returns the texture component being rendered to. 
		Otherwise, returns None/nullptr. */
	Texture *get_texture();

	/* Returns a json string summarizing the camera. */
	std::string to_string();

	/* Records vulkan commands to the given command buffer required to 
		start a renderpass for the current camera setup. This should only be called by the render 
		system, and only after the command buffer has begun recording commands. */
	void begin_renderpass(vk::CommandBuffer command_buffer, uint32_t index = 0);

	/* Records vulkan commands to the given command buffer required to 
		end a renderpass for the current camera setup. */
	void end_renderpass(vk::CommandBuffer command_buffer, uint32_t index = 0);

	/* Records vulkan commands to the given command buffer required to 
		start a depth prepass for the current camera setup. This should only be called by the render 
		system, and only after the command buffer has begun recording commands. */
	void begin_depth_prepass(vk::CommandBuffer command_buffer, uint32_t index = 0);

	/* Records vulkan commands to the given command buffer required to 
		end a depth prepass for the current camera setup. */
	void end_depth_prepass(vk::CommandBuffer command_buffer, uint32_t index = 0);

	/* Returns the vulkan renderpass handle. */
	vk::RenderPass get_renderpass(uint32_t);

	/* Returns the vulkan depth prepass handle. */
	vk::RenderPass get_depth_prepass(uint32_t);

	/* Get the number of renderpasses for this camera */
	uint32_t get_num_renderpasses();

	/* Returns the vulkan command buffer handle. */
	vk::CommandBuffer get_command_buffer();

	/* TODO: Explain this */
	vk::QueryPool get_query_pool();

	/* TODO: */
	void reset_query_pool(vk::CommandBuffer command_buffer);

	/* TODO: Explain this */
	std::vector<uint64_t> & get_query_pool_results();

	void download_query_pool_results();

	/* TODO: Explain this */
	void begin_visibility_query(vk::CommandBuffer command_buffer, uint32_t entity_id, uint32_t drawIdx);
	
	/* TODO: Explain this */
	void end_visibility_query(vk::CommandBuffer command_buffer, uint32_t entity_id, uint32_t drawIdx);

	/* TODO: */
	bool is_entity_visible(uint32_t entity_id);

	/* TODO: Explain this */
	// vk::Semaphore get_semaphore(uint32_t frame_idx);
	
	/* TODO: Explain this */
	vk::Fence get_fence(uint32_t frame_idx);

	/* Sets the clear color to be used to reset the color image of this camera's
		texture component when beginning a renderpass. */
	void set_clear_color(float r, float g, float b, float a);

	/* Sets the clear stencil to be used to reset the depth/stencil image of this 
		camera's texture component when beginning a renderpass. */
	void set_clear_stencil(uint32_t stencil);

	/* Sets the clear depth to be used to reset the depth/stencil image of this 
		camera's texture component when beginning a renderpass. Note: If reverse Z projections are used, 
		this should always be 1.0 */
	void set_clear_depth(float depth);

	/* Sets the renderpass order of the current camera. This is used to handle dependencies between 
		renderpasses. Eg, rendering shadow maps or reflections. */
	void set_render_order(int32_t order);
	
	/* Gets the renderpass order of the current camera. This is used to handle dependencies between 
		renderpasses. Eg, rendering shadow maps or reflections. */
	int32_t get_render_order();

	/* Returns the minimum render order set in the camera list */
	static int32_t GetMinRenderOrder();
	
	/* Returns the maximum render order set in the camera list */
	static int32_t GetMaxRenderOrder();

	/* TODO: Explain this */
	void force_render_mode(RenderMode rendermode);

	/* TODO: Explain this */
	RenderMode get_rendermode_override();

	/* TODO: Explain this */
	bool should_record_depth_prepass();

	/* TODO: Explain this */
	bool should_use_multiview();

	/* TODO: Explain this */
	std::vector<std::vector<std::pair<float, Entity*>>> get_visible_entities(uint32_t camera_entity_id);

	/* Creates a vulkan commandbuffer handle used to record the renderpass. */
	void create_command_buffers();

	/* TODO: */
	bool needs_command_buffers();

	/* TODO: */
	void pause_visibility_testing();
	
	/* TODO: */
	void resume_visibility_testing();


  private:
  	/* TODO */
	static std::mutex creation_mutex;
	
	/* Determines whether this camera should use a depth prepass to reduce fragment complexity at the cost of 
	vertex shader complexity */
	bool use_depth_prepass;

	/* TODO */
	bool use_multiview;

	/* Marks the total number of multiviews being used by the current camera. */
	uint32_t usedViews = 1;

	/* Marks the maximum number of views this camera can support, which can be possibly less than MAX_MULTIVIEW. */
	uint32_t maxMultiview = MAX_MULTIVIEW;

	/* Marks when this camera should render during a frame. */
	int32_t renderOrder = 0;

	/* Marks the range of render orders, so that the render system can create the right number of semaphores. */
	static int32_t minRenderOrder;
	static int32_t maxRenderOrder;

	/* A struct containing all data to be uploaded to the GPU via an SSBO. */
	CameraStruct camera_struct;

	/* The vulkan renderpass handles, used to begin and end final renderpasses for the given camera. */
	std::vector<vk::RenderPass> renderpasses;

	/* TODO: Explain this */
	vk::QueryPool queryPool;

	/* TODO: */
	bool queryRecorded = false;

	bool queryDownloaded = true;

	bool visibilityTestingPaused = false;

	uint64_t max_queried = 0;

	std::map<uint32_t, uint32_t> previousEntityToDrawIdx;
	
	std::map<uint32_t, uint32_t> entityToDrawIdx;

	/* TODO: Explain this */
	std::vector<uint64_t> queryResults;

	std::vector<std::vector<std::pair<float, Entity*>>> frustum_culling_results;

	/* The vulkan depth prpass renderpass handles, used to begin and end depth prepasses for the given camera. */
	std::vector<vk::RenderPass> depthPrepasses;
	
	/* The vulkan framebuffer handles, which associates attachments with image views. */
	std::vector<vk::Framebuffer> framebuffers;

	/* The vulkan prepass framebuffer handles, which associates attachments with image views. */
	std::vector<vk::Framebuffer> depthPrepassFramebuffers;

	/* The vulkan command buffer handle, used to record the renderpass. */
	vk::CommandBuffer command_buffer;

	/* TODO: explain this */
	// std::vector<vk::Semaphore> semaphores;

	/* TODO: explain this */
	std::vector<vk::Fence> fences;

	/* The texture component attached to the framebuffer, which will be rendered to. */
	Texture *renderTexture = nullptr;
	
	/* If msaa_samples is more than one, this texture component is used to resolve MSAA samples. */
	Texture *resolveTexture = nullptr;
	
	/* The RGBA color used when clearing the color attachment at the beginning of a renderpass. */
	glm::vec4 clearColor = glm::vec4(0.0);
	
	/* The depth value used when clearing the depth/stencil attachment at the beginning of a renderpass. */
	float clearDepth = 0.0f;
	
	/* The stencil value used when clearing the depth/stencil attachment at the beginning of a renderpass. */
	uint32_t clearStencil = 0;
	
	/* An integer representation of the number of MSAA samples to take when rendering. Must be a power of 2.
		if 1, it's inferred that a resolve texture is not required.  */
	uint32_t msaa_samples = 1;

	/* A list of the camera components, allocated statically */
	static Camera cameras[MAX_CAMERAS];
	
	/* A lookup table of name to camera id */
	static std::map<std::string, uint32_t> lookupTable;
	
	/* A pointer to the mapped camera SSBO. This memory is shared between the GPU and CPU. */
	static CameraStruct *pinnedMemory;
	
	/* A vulkan buffer handle corresponding to the material SSBO */
	static vk::Buffer SSBO;
	
	/* The corresponding material SSBO memory. */
	static vk::DeviceMemory SSBOMemory;

	/* TODO */
	static vk::Buffer stagingSSBO;
	
	/* TODO */
	static vk::DeviceMemory stagingSSBOMemory;

	RenderMode renderModeOverride;

	/* Allocates (and possibly frees existing) textures, renderpass, and framebuffer required for rendering. */
	void setup(bool cubemap = false, uint32_t tex_width = 0, uint32_t tex_height = 0, uint32_t msaa_samples = 1, uint32_t layers = 1, bool use_depth_prepass = true, bool use_multiview = false);

	/* Creates a vulkan renderpass handle which will be used when recording from the current camera component. */
	void create_render_passes(uint32_t layers = 1, uint32_t sample_count = 1);

	/* Creates a vulkan framebuffer handle used by the renderpass, which binds image views to the framebuffer attachments. */
	void create_frame_buffers(uint32_t layers);

	/* TODO: Explain this */
	void create_query_pool();

	/* TODO: Explain this */
	// void create_semaphores();

	/* TODO: Explain this */
	void create_fences();

	/* Updates the usedViews field to account for a new multiview. This is fixed to the allocated texture layers 
		when recording is enabled. */
	void update_used_views(uint32_t multiview);

	/* Checks to see if a given multiview index is within the multiview bounds, bounded either by MAX_MULTIVIEW, or the 
		camera's texture layers */
	void check_multiview_index(uint32_t multiview);

	/* Releases any vulkan resources. */
	void cleanup();
};
