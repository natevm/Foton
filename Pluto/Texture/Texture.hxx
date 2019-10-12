#pragma once

#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <mutex>

#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Tools/StaticFactory.hxx"
#include "Pluto/Texture/TextureStruct.hxx"

#ifndef MAX_G_BUFFERS  
#define MAX_G_BUFFERS 40
#endif

class Texture : public StaticFactory
{
	friend class StaticFactory;
	public:
		/* This is public so that external libraries can easily create 
			textures while still conforming to the component interface */
		struct ImageData {
			vk::Image image;
			vk::Format format;
			vk::DeviceMemory imageMemory;
			vk::ImageView imageView;
			std::vector<vk::ImageView> imageViewLayers;
			vk::ImageLayout imageLayout;
			uint32_t mipLevels = 1;
			uint32_t samplerId = 0;
		};

		struct Data
		{
			ImageData colorBuffer;
			ImageData depthBuffer;
			std::array<ImageData, MAX_G_BUFFERS> gBuffers;

			uint32_t width = 1, height = 1, depth = 1, layers = 1;
			vk::ImageViewType viewType;
			vk::ImageType imageType;
			vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;
			std::vector<vk::Image> additionalColorImages;
		};

		/* Creates a texture from a khronos texture file (.ktx) */
		static Texture *CreateFromKTX(std::string name, std::string filepath, bool submit_immediately = false);

		/* Creates a 2D texture from a PNG image file */
		static Texture *CreateFromPNG(std::string name, std::string filepath, bool submit_immediately = false);

		static Texture *CreateFromBumpPNG(std::string name, std::string filepath, bool submit_immediately = false);

		/* Creates a texture from data allocated outside this class. Helpful for swapchains, external libraries, etc */
		static Texture *CreateFromExternalData(std::string name, Data data);

		/* Creates a texture from a flattened sequence of RGBA floats, whose shape was originally (width, height, 4) */
		static Texture *Create2DFromColorData(std::string name, uint32_t width, uint32_t height, std::vector<float> data, bool submit_immediately = false);

		/* Creates a cubemap texture of a given width and height, and with color and/or depth resources. */
		static Texture *CreateCubemap(std::string name, uint32_t width, uint32_t height, bool hasColor, bool hasDepth, bool submit_immediately = false);

		/* Creates a cubemap texture of a given width and height, and with several G buffer a depth buffer resources */
		static Texture *CreateCubemapGBuffers(std::string name, uint32_t width, uint32_t height, uint32_t sampleCount, bool submit_immediately = false);

		/* Creates a 2d texture of a given width and height, consisting of possibly several layers, and with color and/or depth resources. */
		static Texture *Create2D(std::string name, uint32_t width, uint32_t height, bool hasColor, bool hasDepth, uint32_t sampleCount, uint32_t layers, bool submit_immediately = false);

		/* Creates a 2d texture of a given width and height, consisting of possibly several layers, and with several G buffer a depth buffer resources */
		static Texture* Create2DGBuffers(std::string name, uint32_t width, uint32_t height, uint32_t sampleCount, uint32_t layers, bool submit_immediately = false);

		/* Creates a 3d texture of a given width, height, and depth consisting of possibly several layers. */
		static Texture *Create3D(std::string name, uint32_t width, uint32_t height, uint32_t depth, uint32_t layers, bool submit_immediately = false);

		/* Creates a procedural checker texture. */
		static Texture* CreateChecker(std::string name, bool submit_immediately = false);

		/* TODO: Explain this */
		static bool DoesItemExist(std::string name);

		/* Retrieves a texture component by name */
		static Texture *Get(std::string name);

		/* Retrieves a texture component by id */
		static Texture *Get(uint32_t id);

		/* Returns a pointer to the list of texture components */
		static Texture *GetFront();

		/* Returns the total number of reserved textures */
		static uint32_t GetCount();

		/* Deallocates a texture with the given name */
		static void Delete(std::string name);

		/* Deallocates a texture with the given id */
		static void Delete(uint32_t id);

		/* Initializes the Mesh factory. Loads default meshes. */
		static void Initialize();

		/* TODO */
		static bool IsInitialized();

		/* Transfers all texture components to an SSBO */
        static void UploadSSBO(vk::CommandBuffer command_buffer);

		/* Returns the SSBO vulkan buffer handle */
        static vk::Buffer GetSSBO();

        /* Returns the size in bytes of the current texture SSBO */
        static uint32_t GetSSBOSize();

		/* Returns a list of samplers corresponding to the texture list, or defaults if the texture isn't usable. 
			Useful for updating descriptor sets. */
		static std::vector<vk::Sampler> GetSamplers();

		/* Returns a list of samplers corresponding to the texture list, or defaults if the texture isn't usable. 
			Useful for updating descriptor sets. */
		static std::vector<vk::ImageView> GetImageViews(vk::ImageViewType view_type);

		/* Returns a list of samplers corresponding to the texture list, or defaults if the texture isn't usable. 
			Useful for updating descriptor sets. */
		static std::vector<vk::ImageLayout> GetLayouts(vk::ImageViewType view_type);

		/* Releases vulkan resources */
		static void CleanUp();

		/* Accessors / Mutators */
		vk::ImageView get_g_buffer_image_view(uint32_t index);
		std::vector<vk::ImageView> get_g_buffer_image_view_layers(uint32_t index);
		vk::Sampler get_g_buffer_sampler(uint32_t index);
		vk::ImageLayout get_g_buffer_image_layout(uint32_t index);
		vk::Image get_g_buffer_image(uint32_t index);
		vk::Format get_g_buffer_format(uint32_t index);

		vk::Format get_color_format();
		vk::ImageLayout get_color_image_layout();
		vk::ImageView get_color_image_view();
		std::vector<vk::ImageView> get_color_image_view_layers();
		vk::Image get_color_image();
		uint32_t get_color_mip_levels();
		vk::Sampler get_color_sampler();
		vk::Format get_depth_format();
		vk::ImageLayout get_depth_image_layout();
		vk::ImageView get_depth_image_view();
        std::vector<vk::ImageView> get_depth_image_view_layers();
		vk::Image get_depth_image();
		vk::Sampler get_depth_sampler();
		uint32_t get_depth();
		uint32_t get_height();
		uint32_t get_total_layers();
		uint32_t get_width();
		vk::SampleCountFlagBits get_sample_count();		

		/* Blits the texture to an image of the given width, height, and depth, then downloads from the GPU to the CPU. */
		std::vector<float> download_color_data(uint32_t width, uint32_t height, uint32_t depth, bool submit_immediately = false);

		/* Blits the provided image of shape (width, height, depth, 4) to the current texture. */
		void upload_color_data(uint32_t width, uint32_t height, uint32_t depth, std::vector<float> color_data, bool submit_immediately = false);

		/* Records a blit of this texture onto another. */
		void record_blit_to(vk::CommandBuffer command_buffer, Texture *other, uint32_t layer = 0);

		/* Replace the current image resources with external resources. Note, these resources must be freed externally. */
		void setData(Data data);

		/* Sets the first color to be used on a procedural texture type */
		void set_procedural_color_1(float r, float g, float b, float a);
		
		/* Sets the second color to be used on a procedural texture type */
		void set_procedural_color_2(float r, float g, float b, float a);

		/* Sets the scale to be used on a procedural texture type */
		void set_procedural_scale(float scale);

		/* Returns a json string summarizing the texture */
		std::string to_string();

		// Create an image memory barrier for changing the layout of
		// an image and put it into an active command buffer
		void setImageLayout(
			vk::CommandBuffer cmdbuffer,
			vk::Image image,
			vk::ImageLayout oldImageLayout,
			vk::ImageLayout newImageLayout,
			vk::ImageSubresourceRange subresourceRange,
			vk::PipelineStageFlags srcStageMask = vk::PipelineStageFlagBits::eAllCommands,
			vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eAllCommands);

		// Kinda kludgy. Need to go back and forth between renderpasses
		void make_renderable(vk::CommandBuffer commandBuffer, 
			vk::PipelineStageFlags srcStageMask = vk::PipelineStageFlagBits::eAllCommands,
			vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eAllCommands);
		void make_samplable(vk::CommandBuffer commandBuffer, 
			vk::PipelineStageFlags srcStageMask = vk::PipelineStageFlagBits::eAllCommands,
			vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eAllCommands);
		void make_general(vk::CommandBuffer commandBuffer, 
			vk::PipelineStageFlags srcStageMask = vk::PipelineStageFlagBits::eAllCommands,
			vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eAllCommands);

		void overrideColorImageLayout(vk::ImageLayout layout);
		void overrideDepthImageLayout(vk::ImageLayout layout);

		void save_as_ktx(std::string path);

	private:
		/* Creates an uninitialized texture. Useful for preallocation. */
		Texture();

		/* Creates a texture with the given name and id. */
		Texture(std::string name, uint32_t id);

		/* TODO */
		static std::shared_ptr<std::mutex> creation_mutex;
		
		/* TODO */
		static bool Initialized;

		/* The list of texture components, allocated statically */
		static Texture textures[MAX_TEXTURES];
		
		/* The list of texture samplers, which a texture refers to in a shader for sampling. */
		static vk::Sampler samplers[MAX_SAMPLERS];
	
		/* A lookup table of name to texture id */
		static std::map<std::string, uint32_t> lookupTable;

		/* A pointer to the mapped texture SSBO. This memory is shared between the GPU and CPU. */
        static TextureStruct* pinnedMemory;

        /* A vulkan buffer handle corresponding to the texture SSBO  */
        static vk::Buffer SSBO;

        /* The corresponding texture SSBO memory */
        static vk::DeviceMemory SSBOMemory;

		/* TODO  */
        static vk::Buffer stagingSSBO;

        /* TODO */
        static vk::DeviceMemory stagingSSBOMemory;

		/* The struct of texture data, aggregating vulkan resources */
		Data data;

		/* The structure containing all shader texture properties. This is what's coppied into the SSBO per instance */
        TextureStruct texture_struct;

		/* Indicates that the texture was made externally (eg, setData was used), and that the resources should not
			be freed internally. */
		bool madeExternally = false;

		/* Textures might not be immediately ready after creation, as resources are allocated asyncronously.
		This flag indicates the texture is ready for use. */
		bool ready = false;

		/* Returns the flag indicating that the texture is ready for use */
		bool is_ready();

		/* Frees the current texture's vulkan resources*/
		void cleanup();

		/* Allocates vulkan resources required for a colored image */
		void create_color_image_resources(ImageData &imageData, bool submit_immediately = false, bool attachment_optimal = true);

		/* Allocates vulkan resources required for a depth/stencil image */
		void create_depth_stencil_resources(ImageData &imageData, bool submit_immediately = false);

		/* Creates a color ImageView, which allows the color image resources to be interpreted for use in shaders, blits, etc */
		void createColorImageView();

		/* Queries the physical device for which depth formats are supported, and returns by reference the optimal depth format. */
		bool get_supported_depth_format(vk::PhysicalDevice physicalDevice, vk::Format *depthFormat);

		/* Creates a texture from a khronos texture file, replacing any vulkan resources with new ones containing the ktx data. */
		void loadKTX(std::string imagePath, bool submit_immediately = false);

		/* Creates a texture from a png texture file, replacing any vulkan resources with new ones containing the png data. */
		void loadPNG(std::string imagePath, bool convert_bump = false, bool submit_immediately = false);

	    /* Indicates that one of the components has been edited */
		static bool Dirty;

		/* Indicates this component has been edited */
		bool dirty = true;

		void mark_dirty() {
			Dirty = true;
			dirty = true;
		};
};
