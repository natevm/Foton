// ┌──────────────────────────────────────────────────────────────────┐
// │  Camera                                                          │
// └──────────────────────────────────────────────────────────────────┘

#pragma once

#include <vulkan/vulkan.hpp>

#include "Pluto/Tools/StaticFactory.hxx"
#include "Pluto/Camera/CameraStruct.hxx"

class Texture;

class Camera : public StaticFactory
{
  public:
	/* Creates a camera, which can be used to record the scene. Can be used to render to several texture layers for use in cubemap rendering/VR renderpasses. */
	static Camera *Create(std::string name, bool allow_recording = false, bool cubemap = false, uint32_t tex_width = 0, uint32_t tex_height = 0, uint32_t msaa_samples = 1, uint32_t layers = 1);

	/* Retrieves a camera component by name. */
	static Camera *Get(std::string name);

	/* Retrieves a camera component by id. */
	static Camera *Get(uint32_t id);

	/* Returns a pointer to the list of camera components. */
	static Camera *GetFront();

	/* Returns the total number of reserved cameras. */
	static uint32_t GetCount();

	/* Deallocates a camera with the given name. */
	static bool Delete(std::string name);

	/* Deallocates a camera with the given id. */
	static bool Delete(uint32_t id);

	/* Initializes the Camera factory. Loads any default components. */
	static void Initialize();

	/* Transfers all camera components to an SSBO */
	static void UploadSSBO();

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
	bool set_perspective_projection(float fov_in_radians, float width, float height, float near_pos, uint32_t multiview = 0);

	/* Uses an external projection matrix for the given multiview. 
		Note, the projection must be a reversed Z projection. */
	bool set_custom_projection(glm::mat4 custom_projection, float near_pos, uint32_t multiview = 0);

	/* Returns the near position of the given multiview */
	float get_near_pos(uint32_t multiview = 0);

	/* Returns the entity transform to camera matrix for the given multiview. 
		This additional transform is applied on top of an entity transform during a renderpass
		to see a particular "view". */
	glm::mat4 get_view(uint32_t multiview = 0);

	/* Sets the entity transform to camera matrix for the given multiview. 
		This additional transform is applied on top of an entity transform during a renderpass
		to see a particular "view". */
	bool set_view(glm::mat4 view, uint32_t multiview = 0);

	/* Returns the camera to projection matrix for the given multiview.
		This transform can be used to achieve perspective (eg a vanishing point), or for scaling
		an orthographic view. */
	glm::mat4 get_projection(uint32_t multiview = 0);

	/* If recording is allowed, returns the texture component being rendered to. 
		Otherwise, returns None/nullptr. */
	Texture *get_texture();

	/* Returns a json string summarizing the camera. */
	std::string to_string();

	/* If recording is allowed, records vulkan commands to the given command buffer required to 
		start a renderpass for the current camera setup. This should only be called by the render 
		system, and only after the command buffer has begun recording commands. */
	bool begin_renderpass(vk::CommandBuffer command_buffer);

	/* If recording is allowed, returns the vulkan renderpass handle. 
		Note: This handle might change throughout the lifetime of this camera component. */
	vk::RenderPass get_renderpass();

	/* If recording is allowed, records vulkan commands to the given command buffer required to 
		end a renderpass for the current camera setup. */
	bool end_renderpass(vk::CommandBuffer command_buffer);

	/* If recording is allowed, sets the clear color to be used to reset the color image of this camera's
		texture component when beginning a renderpass. */
	void set_clear_color(float r, float g, float b, float a);

	/* If recording is allowed, sets the clear stencil to be used to reset the depth/stencil image of this 
		camera's texture component when beginning a renderpass. */
	void set_clear_stencil(uint32_t stencil);

	/* If recording is allowed, sets the clear depth to be used to reset the depth/stencil image of this 
		camera's texture component when beginning a renderpass. Note: If reverse Z projections are used, 
		this should always be 1.0 */
	void set_clear_depth(float depth);

	/* Sets the renderpass order of the current camera. This is used to render this camera first, so that
		other cameras later on can use the results. Eg, rendering shadow maps or reflections. */
	void set_render_order(uint32_t order);

  private:
	/* Marks the total number of multiviews being used by the current camera. */
	uint32_t usedViews = 1;

	/* Marks the maximum number of views this camera can support, which can be possibly less than MAX_MULTIVIEW. */
	uint32_t maxMultiview = MAX_MULTIVIEW;

	/* Marks when this camera should render during a frame. */
	uint32_t renderOrder = 0;

	/* A struct containing all data to be uploaded to the GPU via an SSBO. */
	CameraStruct camera_struct;

	/* The vulkan renderpass handle, used to begin and end renderpasses for the given camera. 
		Handles all multiviews at once. */
	vk::RenderPass renderpass;
	
	/* The vulkan framebuffer handle, which associates attachments with image views. 
		Handles all multiviews at once. */
	vk::Framebuffer framebuffer;

	/* The texture component attached to the framebuffer, which will be rendered to. */
	Texture *renderTexture = nullptr;
	
	/* If msaa_samples is more than one, this texture component is used to resolve MSAA samples. */
	Texture *resolveTexture = nullptr;
	
	/* The flag which indicates whether this camera can be used for rendering to textures. */
	bool allow_recording = false;

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
	static vk::Buffer ssbo;
	
	/* The corresponding material SSBO memory. */
	static vk::DeviceMemory ssboMemory;

	/* Allocates (and possibly frees existing) textures, renderpass, and framebuffer required for rendering. */
	bool setup(bool allow_recording = false, bool cubemap = false, uint32_t tex_width = 0, uint32_t tex_height = 0, uint32_t msaa_samples = 1, uint32_t layers = 1);

	/* Creates a vulkan renderpass handle which will be used when recording from the current camera component. */
	void create_render_pass(uint32_t framebufferWidth, uint32_t framebufferHeight, uint32_t layers = 1, uint32_t sample_count = 1);

	/* Creates a vulkan framebuffer handle used by the renderpass, which binds image views to the framebuffer attachments. */
	void create_frame_buffer();

	/* Updates the usedViews field to account for a new multiview. This is fixed to the allocated texture layers 
		when recording is enabled. */
	void update_used_views(uint32_t multiview);

	/* Checks to see if a given multiview index is within the multiview bounds, bounded either by MAX_MULTIVIEW, or the 
		camera's texture layers when recording is allowed..  */
	bool check_multiview_index(uint32_t multiview);
};