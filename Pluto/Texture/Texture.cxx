#pragma optimize("", off)

#include "./Texture.hxx"
#include "Pluto/Tools/Options.hxx"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <sys/types.h>
#include <sys/stat.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include <gli/gli.hpp>

Texture Texture::textures[MAX_TEXTURES];
vk::Sampler Texture::samplers[MAX_SAMPLERS];
std::map<std::string, uint32_t> Texture::lookupTable;
TextureStruct* Texture::pinnedMemory;
vk::Buffer Texture::ssbo;
vk::DeviceMemory Texture::ssboMemory;

Texture::Texture()
{
    initialized = false;
}

Texture::Texture(std::string name, uint32_t id)
{
    initialized = true;
    this->name = name;
    this->id = id;
    texture_struct.type = 0;
    texture_struct.scale = .1f;
    texture_struct.mip_levels = 0;
    texture_struct.color1 = glm::vec4(1.0, 1.0, 1.0, 1.0);
    texture_struct.color2 = glm::vec4(0.0, 0.0, 0.0, 1.0);
}

std::string Texture::to_string()
{
    std::string output;
    output += "{\n";
    output += "\ttype: \"Texture\",\n";
    output += "\tname: \"" + name + "\",\n";
    output += "\twidth: " + std::to_string(data.width) + "\n";
    output += "\theight: " + std::to_string(data.height) + "\n";
    output += "\tdepth: " + std::to_string(data.depth) + "\n";
    output += "\tlayers: " + std::to_string(data.layers) + "\n";
    output += "\tview_type: " + vk::to_string(data.viewType) + "\n";
    output += "\tcolor_mip_levels: " + std::to_string(data.colorMipLevels) + "\n";
    output += "\thas_color: ";
    output += ((data.colorImage == vk::Image()) ? "false" : "true");
    output += "\n";
    output += "\tcolor_sampler_id: " + std::to_string(data.colorSamplerId) + "\n";
    output += "\tcolor_format: " + vk::to_string(data.colorFormat) + "\n";
    output += "\thas_depth: ";
    output += ((data.depthImage == vk::Image()) ? "false" : "true");
    output += "\n";
    output += "\tdepth_sampler_id: " + std::to_string(data.depthSamplerId) + "\n";
    output += "\tdepth_format: " + vk::to_string(data.depthFormat) + "\n";
    output += "}";
    return output;
}

std::vector<float> Texture::download_color_data(uint32_t width, uint32_t height, uint32_t depth, bool submit_immediately)
{
    /* I'm assuming an image was already loaded for now */
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error( std::string("Vulkan library is not initialized"));
    auto device = vulkan->get_device();
    if (device == vk::Device())
        throw std::runtime_error( std::string("Invalid vulkan device"));
    auto physicalDevice = vulkan->get_physical_device();
    if (physicalDevice == vk::PhysicalDevice())
        throw std::runtime_error( std::string("Invalid vulkan physical device"));

    /* Since the image memory is device local and tiled, we need to 
        make a copy, transform to an untiled format. */

    /* Create staging buffer */
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = width * height * depth * 4 * sizeof(float);
    bufferInfo.usage = vk::BufferUsageFlagBits::eTransferDst;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    vk::Buffer stagingBuffer = device.createBuffer(bufferInfo);

    vk::MemoryRequirements stagingMemRequirements = device.getBufferMemoryRequirements(stagingBuffer);
    vk::MemoryAllocateInfo stagingAllocInfo;
    stagingAllocInfo.allocationSize = (uint32_t)stagingMemRequirements.size;
    stagingAllocInfo.memoryTypeIndex = vulkan->find_memory_type(stagingMemRequirements.memoryTypeBits,
                                                                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    vk::DeviceMemory stagingBufferMemory = device.allocateMemory(stagingAllocInfo);

    device.bindBufferMemory(stagingBuffer, stagingBufferMemory, 0);

    /* Create a copy of image */
    vk::ImageCreateInfo imInfo;
    // imInfo.flags; // May need this later for cubemaps, texture arrays, etc
    imInfo.imageType = data.imageType;
    imInfo.format = vk::Format::eR32G32B32A32Sfloat;
    imInfo.extent = vk::Extent3D{width, height, depth};
    imInfo.mipLevels = 1;
    imInfo.arrayLayers = 1;
    imInfo.samples = vk::SampleCountFlagBits::e1;
    imInfo.tiling = vk::ImageTiling::eOptimal;
    imInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
    imInfo.sharingMode = vk::SharingMode::eExclusive;
    imInfo.initialLayout = vk::ImageLayout::eUndefined;
    vk::Image blitImage = device.createImage(imInfo);

    /* Create memory for that image */
    vk::MemoryRequirements imageMemReqs = device.getImageMemoryRequirements(blitImage);
    vk::MemoryAllocateInfo imageAllocInfo;
    imageAllocInfo.allocationSize = imageMemReqs.size;
    imageAllocInfo.memoryTypeIndex = vulkan->find_memory_type(
        imageMemReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    vk::DeviceMemory blitImageMemory = device.allocateMemory(imageAllocInfo);

    /* Bind the image with its memory */
    device.bindImageMemory(blitImage, blitImageMemory, 0);

    /* Now, we need to blit the texture to this host image */

    /* First, specify the region we'd like to copy */
    vk::ImageSubresourceLayers srcSubresourceLayers;
    srcSubresourceLayers.aspectMask = vk::ImageAspectFlagBits::eColor;
    srcSubresourceLayers.mipLevel = 0;
    srcSubresourceLayers.baseArrayLayer = 0;
    srcSubresourceLayers.layerCount = 1; // TODO

    vk::ImageSubresourceLayers dstSubresourceLayers;
    dstSubresourceLayers.aspectMask = vk::ImageAspectFlagBits::eColor;
    dstSubresourceLayers.mipLevel = 0;
    dstSubresourceLayers.baseArrayLayer = 0;
    dstSubresourceLayers.layerCount = 1; // TODO

    vk::ImageBlit region;
    region.srcSubresource = srcSubresourceLayers;
    region.srcOffsets[0] = vk::Offset3D{0, 0, 0};
    region.srcOffsets[1] = vk::Offset3D{(int32_t)this->data.width, (int32_t)this->data.height, (int32_t)this->data.depth};
    region.dstSubresource = dstSubresourceLayers;
    region.dstOffsets[0] = vk::Offset3D{0, 0, 0};
    region.dstOffsets[1] = vk::Offset3D{(int32_t)width, (int32_t)height, (int32_t)depth};

    /* Next, specify the filter we'd like to use */
    vk::Filter filter = vk::Filter::eLinear;

    /* Now, create a command buffer */
    vk::CommandBuffer cmdBuffer = vulkan->begin_one_time_graphics_command();

    /* Transition destination image to transfer destination optimal */
    vk::ImageSubresourceRange dstSubresourceRange;
    dstSubresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    dstSubresourceRange.baseMipLevel = 0;
    dstSubresourceRange.levelCount = 1;
    dstSubresourceRange.layerCount = 1; // TODO

    vk::ImageSubresourceRange srcSubresourceRange;
    srcSubresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    srcSubresourceRange.baseMipLevel = 0;
    srcSubresourceRange.levelCount = data.colorMipLevels;
    srcSubresourceRange.layerCount = 1; //.............

    setImageLayout(
        cmdBuffer,
        blitImage,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        dstSubresourceRange);

    /* Transition source to shader read optimal */
    auto originalLayout = data.colorImageLayout;
    setImageLayout(
        cmdBuffer,
        data.colorImage,
        data.colorImageLayout,
        vk::ImageLayout::eTransferSrcOptimal,
        srcSubresourceRange);
    data.colorImageLayout = vk::ImageLayout::eTransferSrcOptimal;

    /* Blit the image... */
    cmdBuffer.blitImage(
        data.colorImage,
        data.colorImageLayout,
        blitImage,
        vk::ImageLayout::eTransferDstOptimal,
        region,
        filter);

    setImageLayout(
        cmdBuffer,
        blitImage,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eTransferSrcOptimal,
        dstSubresourceRange);

    /* Copy the blit image to a host visable staging buffer */
    vk::BufferImageCopy imgCopyRegion;
    // imgCopyRegion.bufferOffset = 0;
    // imgCopyRegion.bufferRowLength =
    // imgCopyRegion.bufferImageHeight = 0;
    imgCopyRegion.imageSubresource = dstSubresourceLayers;
    imgCopyRegion.imageOffset = vk::Offset3D{0, 0, 0};
    imgCopyRegion.imageExtent = vk::Extent3D{(uint32_t)width, (uint32_t)height, (uint32_t)depth};

    cmdBuffer.copyImageToBuffer(blitImage, vk::ImageLayout::eTransferSrcOptimal, stagingBuffer, imgCopyRegion);

    /* Transition source back to previous layout */
    setImageLayout(
        cmdBuffer,
        data.colorImage,
        data.colorImageLayout,
        originalLayout,
        srcSubresourceRange);
    data.colorImageLayout = originalLayout;
    vulkan->end_one_time_graphics_command(cmdBuffer, "download color data", true, submit_immediately);

    /* Memcpy from host visable image here... */
    /* Copy texture data into staging buffer */
    std::vector<float> result(width * height * depth * 4, 0.0);
    void *data = device.mapMemory(stagingBufferMemory, 0, width * height * depth * 4 * sizeof(float), vk::MemoryMapFlags());
    memcpy(result.data(), data, width * height * depth * 4 * sizeof(float));
    device.unmapMemory(stagingBufferMemory);

    /* Clean up */
    device.destroyBuffer(stagingBuffer);
    device.freeMemory(stagingBufferMemory);
    device.destroyImage(blitImage);
    device.freeMemory(blitImageMemory);

    return result;
}

void Texture::setData(Data data)
{
    this->madeExternally = true;
    this->data = data;
}

void Texture::set_procedural_color_1(float r, float g, float b, float a)
{
    if (texture_struct.type == 0)
        throw std::runtime_error("Error: texture must be procedural");
    texture_struct.color1 = glm::vec4(r, g, b, a);
}

void Texture::set_procedural_color_2(float r, float g, float b, float a)
{
    if (texture_struct.type == 0)
        throw std::runtime_error("Error: texture must be procedural");
    texture_struct.color2 = glm::vec4(r, g, b, a);
}

void Texture::set_procedural_scale(float scale)
{
    if (texture_struct.type == 0)
        throw std::runtime_error("Error: texture must be procedural");
    texture_struct.scale = scale;
}

void Texture::upload_color_data(uint32_t width, uint32_t height, uint32_t depth, std::vector<float> color_data, bool submit_immediately)
{
    /* I'm assuming an image was already loaded for now */
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error( std::string("Vulkan library is not initialized"));
    auto device = vulkan->get_device();
    if (device == vk::Device())
        throw std::runtime_error( std::string("Invalid vulkan device"));
    auto physicalDevice = vulkan->get_physical_device();
    if (physicalDevice == vk::PhysicalDevice())
        throw std::runtime_error( std::string("Invalid vulkan physical device"));

    uint32_t textureSize = width * height * depth * 4 * sizeof(float);
    if (color_data.size() < width * height * depth)
        throw std::runtime_error( std::string("Not enough data for provided image dimensions"));


    /* Create staging buffer */
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = textureSize;
    bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    vk::Buffer stagingBuffer = device.createBuffer(bufferInfo);

    vk::MemoryRequirements stagingMemRequirements = device.getBufferMemoryRequirements(stagingBuffer);
    vk::MemoryAllocateInfo stagingAllocInfo;
    stagingAllocInfo.allocationSize = (uint32_t)stagingMemRequirements.size;
    stagingAllocInfo.memoryTypeIndex = vulkan->find_memory_type(stagingMemRequirements.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    vk::DeviceMemory stagingBufferMemory = device.allocateMemory(stagingAllocInfo);

    device.bindBufferMemory(stagingBuffer, stagingBufferMemory, 0);

    /* Copy texture data into staging buffer */
    void *dataptr = device.mapMemory(stagingBufferMemory, 0, textureSize, vk::MemoryMapFlags());
    memcpy(dataptr, color_data.data(), textureSize);
    device.unmapMemory(stagingBufferMemory);

    /* Setup buffer copy regions for one mip level */
    vk::BufferImageCopy bufferCopyRegion;
    bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = width;
    bufferCopyRegion.imageExtent.height = height;
    bufferCopyRegion.imageExtent.depth = depth;
    bufferCopyRegion.bufferOffset = 0;

    /* Create optimal tiled target image */
    vk::ImageCreateInfo imageCreateInfo;
    imageCreateInfo.imageType = data.imageType; // src and dst types must match. Deal with this in error check
    imageCreateInfo.format = vk::Format::eR32G32B32A32Sfloat;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
    imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
    imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;
    imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageCreateInfo.extent = vk::Extent3D{width, height, depth};
    imageCreateInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
    vk::Image src_image = device.createImage(imageCreateInfo);

    /* Allocate and bind memory for the texture */
    vk::MemoryRequirements imageMemReqs = device.getImageMemoryRequirements(src_image);
    vk::MemoryAllocateInfo imageAllocInfo;
    imageAllocInfo.allocationSize = imageMemReqs.size;
    imageAllocInfo.memoryTypeIndex = vulkan->find_memory_type(
        imageMemReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    vk::DeviceMemory src_image_memory = device.allocateMemory(imageAllocInfo);

    device.bindImageMemory(src_image, src_image_memory, 0);

    /* END CREATE IMAGE */

    vk::CommandBuffer command_buffer = vulkan->begin_one_time_graphics_command();

    /* Which mip level, array layer, layer count, access mask to use */
    vk::ImageSubresourceLayers srcSubresourceLayers;
    srcSubresourceLayers.aspectMask = vk::ImageAspectFlagBits::eColor;
    srcSubresourceLayers.mipLevel = 0;
    srcSubresourceLayers.baseArrayLayer = 0;
    srcSubresourceLayers.layerCount = 1; // TODO

    vk::ImageSubresourceLayers dstSubresourceLayers;
    dstSubresourceLayers.aspectMask = vk::ImageAspectFlagBits::eColor;
    dstSubresourceLayers.mipLevel = 0;
    dstSubresourceLayers.baseArrayLayer = 0;
    dstSubresourceLayers.layerCount = 1; // TODO

    /* For layout transitions */
    vk::ImageSubresourceRange srcSubresourceRange;
    srcSubresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    srcSubresourceRange.baseMipLevel = 0;
    srcSubresourceRange.levelCount = 1;
    srcSubresourceRange.layerCount = 1;

    vk::ImageSubresourceRange dstSubresourceRange;
    dstSubresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    dstSubresourceRange.baseMipLevel = 0;
    dstSubresourceRange.levelCount = data.colorMipLevels;
    dstSubresourceRange.layerCount = 1;

    /* First, copy the staging buffer into our temporary source image. */
    setImageLayout(command_buffer, src_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, srcSubresourceRange);

    /* Copy mip levels from staging buffer */
    command_buffer.copyBufferToImage(stagingBuffer, src_image, vk::ImageLayout::eTransferDstOptimal, 1, &bufferCopyRegion);

    /* Region to copy (Possibly multiple in the future) */
    vk::ImageBlit region;
    region.dstSubresource = dstSubresourceLayers;
    region.dstOffsets[0] = vk::Offset3D{0, 0, 0};
    region.dstOffsets[1] = vk::Offset3D{(int32_t)this->data.width, (int32_t)this->data.height, (int32_t)this->data.depth};
    region.srcSubresource = srcSubresourceLayers;
    region.srcOffsets[0] = vk::Offset3D{0, 0, 0};
    region.srcOffsets[1] = vk::Offset3D{(int32_t)width, (int32_t)height, (int32_t)depth};

    /* Next, specify the filter we'd like to use */
    vk::Filter filter = vk::Filter::eLinear;

    /* transition source to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL  */
    setImageLayout(command_buffer, src_image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, srcSubresourceRange);

    /* transition source to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL  */
    setImageLayout(command_buffer, data.colorImage, data.colorImageLayout, vk::ImageLayout::eTransferDstOptimal, dstSubresourceRange);

    /* Blit the uploaded image to this texture... */
    command_buffer.blitImage(src_image, vk::ImageLayout::eTransferSrcOptimal, data.colorImage, vk::ImageLayout::eTransferDstOptimal, region, filter);

    /* transition source back VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL  */
    //setImageLayout( command_buffer, src_image, vk::ImageLayout::eTransferSrcOptimal, src_layout, srcSubresourceRange);

    /* transition source back VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL  */
    setImageLayout(command_buffer, data.colorImage, vk::ImageLayout::eTransferDstOptimal, data.colorImageLayout, dstSubresourceRange);

    vulkan->end_one_time_graphics_command(command_buffer, "upload color data", true, submit_immediately);

    device.destroyImage(src_image);
    device.freeMemory(src_image_memory);
    device.destroyBuffer(stagingBuffer);
    device.freeMemory(stagingBufferMemory);
}

void Texture::record_blit_to(vk::CommandBuffer command_buffer, Texture * other, uint32_t layer)
{
    if (!other)
        throw std::runtime_error( std::string("Invalid target texture"));

    auto src_image = data.colorImage;
    auto dst_image = other->get_color_image();

    auto src_layout = data.colorImageLayout;
    auto dst_layout = other->get_color_image_layout();

    /* First, specify the region we'd like to copy */
    vk::ImageSubresourceLayers srcSubresourceLayers;
    srcSubresourceLayers.aspectMask = vk::ImageAspectFlagBits::eColor;
    srcSubresourceLayers.mipLevel = 0;
    srcSubresourceLayers.baseArrayLayer = layer;
    srcSubresourceLayers.layerCount = 1; // TODO

    vk::ImageSubresourceLayers dstSubresourceLayers;
    dstSubresourceLayers.aspectMask = vk::ImageAspectFlagBits::eColor;
    dstSubresourceLayers.mipLevel = 0;
    dstSubresourceLayers.baseArrayLayer = 0;
    dstSubresourceLayers.layerCount = 1; // TODO

    vk::ImageBlit region;
    region.srcSubresource = srcSubresourceLayers;
    region.srcOffsets[0] = vk::Offset3D{0, 0, 0};
    region.srcOffsets[1] = vk::Offset3D{(int32_t)this->data.width, (int32_t)this->data.height, (int32_t)this->data.depth};
    region.dstSubresource = dstSubresourceLayers;
    region.dstOffsets[0] = vk::Offset3D{0, 0, 0};
    region.dstOffsets[1] = vk::Offset3D{(int32_t)other->get_width(), (int32_t)other->get_height(), (int32_t)other->get_depth()};

    /* Next, specify the filter we'd like to use */
    vk::Filter filter = vk::Filter::eLinear;

    /* For layout transitions */
    vk::ImageSubresourceRange srcSubresourceRange;
    srcSubresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    srcSubresourceRange.baseMipLevel = 0;
    srcSubresourceRange.levelCount = data.colorMipLevels;
    srcSubresourceRange.layerCount = 1;

    vk::ImageSubresourceRange dstSubresourceRange;
    dstSubresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    dstSubresourceRange.baseMipLevel = 0;
    dstSubresourceRange.levelCount = 1;
    dstSubresourceRange.layerCount = 1;

    /* transition source to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL  */
    setImageLayout(command_buffer, src_image, src_layout, vk::ImageLayout::eTransferSrcOptimal, srcSubresourceRange);

    /* transition source to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL  */
    setImageLayout(command_buffer, dst_image, dst_layout, vk::ImageLayout::eTransferDstOptimal, dstSubresourceRange);

    /* Blit the image... */
    command_buffer.blitImage(src_image, vk::ImageLayout::eTransferSrcOptimal, dst_image, vk::ImageLayout::eTransferDstOptimal, region, filter);

    /* transition source back VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL  */
    setImageLayout(command_buffer, src_image, vk::ImageLayout::eTransferSrcOptimal, src_layout, srcSubresourceRange);

    /* transition source back VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL  */
    setImageLayout(command_buffer, dst_image, vk::ImageLayout::eTransferDstOptimal, dst_layout, dstSubresourceRange);
}

// TODO
void Texture::Initialize()
{
    // Create the default texture here
    std::string resource_path = Options::GetResourcePath();
    CreateFromKTX("BRDF", resource_path + "/Defaults/brdf-lut.ktx");
    CreateFromKTX("DefaultTex2D", resource_path + "/Defaults/missing-texture.ktx");
    CreateFromKTX("DefaultTexCube", resource_path + "/Defaults/missing-texcube.ktx");
    CreateFromKTX("DefaultTex3D", resource_path + "/Defaults/missing-volume.ktx");    
    // fatal error here if result is nullptr...

    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();
    if (device == vk::Device())
        throw std::runtime_error( std::string("Invalid vulkan device"));

    auto physical_device = vulkan->get_physical_device();
    if (physical_device == vk::PhysicalDevice())
        throw std::runtime_error( std::string("Invalid vulkan physical device"));

    vk::BufferCreateInfo bufferInfo = {};
    bufferInfo.size = MAX_TEXTURES * sizeof(TextureStruct);
    bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    ssbo = device.createBuffer(bufferInfo);

    vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(ssbo);
    vk::MemoryAllocateInfo allocInfo = {};
    allocInfo.allocationSize = memReqs.size;

    vk::PhysicalDeviceMemoryProperties memProperties = physical_device.getMemoryProperties();
    vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    allocInfo.memoryTypeIndex = vulkan->find_memory_type(memReqs.memoryTypeBits, properties);

    ssboMemory = device.allocateMemory(allocInfo);
    device.bindBufferMemory(ssbo, ssboMemory, 0);

    /* Pin the buffer */
    pinnedMemory = (TextureStruct*) device.mapMemory(ssboMemory, 0, MAX_TEXTURES * sizeof(TextureStruct));

    /* Create a sampler to sample from the attachment in the fragment shader */
    vk::SamplerCreateInfo sInfo;
    sInfo.magFilter = vk::Filter::eLinear;
    sInfo.minFilter = vk::Filter::eLinear;
    sInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    sInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    sInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    sInfo.mipLodBias = 0.0;
    sInfo.maxAnisotropy = 16.0; // todo, get limit here.
    sInfo.anisotropyEnable = VK_TRUE;
    sInfo.minLod = 0.0;
    sInfo.maxLod = 12.0;
    sInfo.borderColor = vk::BorderColor::eFloatTransparentBlack;
    samplers[0] = device.createSampler(sInfo);
}


void Texture::UploadSSBO()
{
    if (pinnedMemory == nullptr) return;
    TextureStruct texture_structs[MAX_TEXTURES];
    
    /* TODO: remove this for loop */
    for (int i = 0; i < MAX_TEXTURES; ++i) {
        if (!textures[i].is_initialized()) continue;
        texture_structs[i] = textures[i].texture_struct;
    };

    /* Copy to GPU mapped memory */
    memcpy(pinnedMemory, texture_structs, sizeof(texture_structs));
}

vk::Buffer Texture::GetSSBO()
{
    return ssbo;
}

uint32_t Texture::GetSSBOSize()
{
    return MAX_TEXTURES * sizeof(TextureStruct);
}

void Texture::CleanUp()
{
    for (int i = 0; i < MAX_TEXTURES; ++i)
        textures[i].cleanup();

    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error( std::string("Vulkan library is not initialized"));
    auto device = vulkan->get_device();
    if (device == vk::Device())
        throw std::runtime_error( std::string("Invalid vulkan device"));

    for (int i = 0; i < MAX_SAMPLERS; ++i) {
        if (samplers[i] != vk::Sampler()) {
            device.destroySampler(samplers[i]);
        }
    }

    device.destroyBuffer(ssbo);
    device.unmapMemory(ssboMemory);
    device.freeMemory(ssboMemory);
}

std::vector<vk::ImageView> Texture::GetImageViews(vk::ImageViewType view_type) 
{
    // Get the default texture
    Texture *DefaultTex;
    if (view_type == vk::ImageViewType::e2D) DefaultTex = Get("DefaultTex2D");
    else if (view_type == vk::ImageViewType::e3D) DefaultTex = Get("DefaultTex3D");
    else if (view_type == vk::ImageViewType::eCube) DefaultTex = Get("DefaultTexCube");
    else return {};

    std::vector<vk::ImageView> image_views(MAX_TEXTURES);

    // For each texture
    for (int i = 0; i < MAX_TEXTURES; ++i) {
        if (textures[i].initialized 
            && (textures[i].data.colorImageView != vk::ImageView())
            && (textures[i].data.colorImageLayout == vk::ImageLayout::eShaderReadOnlyOptimal) 
            && (textures[i].data.viewType == view_type)) {
            // then add it's image view to the vector
            image_views[i] = textures[i].data.colorImageView;
        }
        // otherwise, add the default texture image view
        else {
            image_views[i] = DefaultTex->data.colorImageView;
        }
    }
    
    // finally, return the image view vector
    return image_views;
}

vk::ImageView Texture::get_depth_image_view() { return data.depthImageView; };
vk::ImageView Texture::get_color_image_view() { return data.colorImageView; };
std::vector<vk::ImageView> Texture::get_depth_image_view_layers() { return data.depthImageViewLayers; };
std::vector<vk::ImageView> Texture::get_color_image_view_layers() { return data.colorImageViewLayers; };
vk::Sampler Texture::get_color_sampler() { return samplers[data.colorSamplerId]; };
vk::Sampler Texture::get_depth_sampler() { return samplers[data.depthSamplerId]; };
vk::ImageLayout Texture::get_color_image_layout() { return data.colorImageLayout; };
vk::ImageLayout Texture::get_depth_image_layout() { return data.depthImageLayout; };
vk::Image Texture::get_color_image() { return data.colorImage; };
vk::Image Texture::get_depth_image() { return data.depthImage; };
uint32_t Texture::get_color_mip_levels() { return data.colorMipLevels; };
vk::Format Texture::get_color_format() { return data.colorFormat; };
vk::Format Texture::get_depth_format() { return data.depthFormat; };
uint32_t Texture::get_width() { return data.width; }
uint32_t Texture::get_height() { return data.height; }
uint32_t Texture::get_depth() { return data.depth; }
uint32_t Texture::get_total_layers() { return data.layers; }
vk::SampleCountFlagBits Texture::get_sample_count() { return data.sampleCount; }
void Texture::setImageLayout(
        vk::CommandBuffer cmdbuffer,
        vk::Image image,
        vk::ImageLayout oldImageLayout,
        vk::ImageLayout newImageLayout,
        vk::ImageSubresourceRange subresourceRange,
        vk::PipelineStageFlags srcStageMask,
        vk::PipelineStageFlags dstStageMask)
{
    // Create an image barrier object
    vk::ImageMemoryBarrier imageMemoryBarrier;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.oldLayout = oldImageLayout;
    imageMemoryBarrier.newLayout = newImageLayout;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = subresourceRange;

    // Source layouts (old)
    // Source access mask controls actions that have to be finished on the old layout
    // before it will be transitioned to the new layout
    switch (oldImageLayout)
    {
    case vk::ImageLayout::eUndefined:
        // Image layout is undefined (or does not matter)
        // Only valid as initial layout
        // No flags required, listed only for completeness
        imageMemoryBarrier.srcAccessMask = vk::AccessFlags();
        break;

    case vk::ImageLayout::ePreinitialized:
        // Image is preinitialized
        // Only valid as initial layout for linear images, preserves memory contents
        // Make sure host writes have been finished
        imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
        break;

    case vk::ImageLayout::eColorAttachmentOptimal:
        // Image is a color attachment
        // Make sure any writes to the color buffer have been finished
        imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        break;

    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        // Image is a depth/stencil attachment
        // Make sure any writes to the depth/stencil buffer have been finished
        imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        break;

    case vk::ImageLayout::eTransferSrcOptimal:
        // Image is a transfer source
        // Make sure any reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        break;

    case vk::ImageLayout::eTransferDstOptimal:
        // Image is a transfer destination
        // Make sure any writes to the image have been finished
        imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        break;

    case vk::ImageLayout::eShaderReadOnlyOptimal:
        // Image is read by a shader
        // Make sure any shader reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    // Target layouts (new)
    // Destination access mask controls the dependency for the new image layout
    switch (newImageLayout)
    {
    case vk::ImageLayout::eTransferDstOptimal:
        // Image will be used as a transfer destination
        // Make sure any writes to the image have been finished
        imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        break;

    case vk::ImageLayout::eTransferSrcOptimal:
        // Image will be used as a transfer source
        // Make sure any reads from the image have been finished
        imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        break;

    case vk::ImageLayout::eColorAttachmentOptimal:
        // Image will be used as a color attachment
        // Make sure any writes to the color buffer have been finished
        imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        break;

    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        // Image layout will be used as a depth/stencil attachment
        // Make sure any writes to depth/stencil buffer have been finished
        imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        break;

    case vk::ImageLayout::eShaderReadOnlyOptimal:
        // Image will be read in a shader (sampler, input attachment)
        // Make sure any writes to the image have been finished
        if (imageMemoryBarrier.srcAccessMask == vk::AccessFlags())
        {
            imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eHostWrite | vk::AccessFlagBits::eTransferWrite;
        }
        imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    // Put barrier inside setup command buffer
    cmdbuffer.pipelineBarrier(
        srcStageMask,
        dstStageMask,
        vk::DependencyFlags(),
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);
}

void Texture::loadKTX(std::string imagePath, bool submit_immediately)
{
    /* First, check if file exists. */
    struct stat st;
    if (stat(imagePath.c_str(), &st) != 0)
        throw std::runtime_error( std::string("Error: image " + imagePath + " does not exist!"));

    /* Load the texture */
    auto texture = gli::load(imagePath);
    uint32_t textureSize = 0;
    void *textureData = nullptr;

    gli::texture_cube texCube;
    gli::texture2d tex2D;
    gli::texture3d tex3D;

    std::vector<uint32_t> textureMipSizes;
    std::vector<uint32_t> textureMipWidths;
    std::vector<uint32_t> textureMipHeights;
    std::vector<uint32_t> textureMipDepths;

    if (texture.target() == gli::target::TARGET_2D)
    {
        tex2D = gli::texture2d(texture);

        if (tex2D.empty())
            throw std::runtime_error( std::string("Error: image " + imagePath + " is empty"));

        data.width = (uint32_t)(tex2D.extent().x);
        data.height = (uint32_t)(tex2D.extent().y);
        data.depth = 1;

        data.colorMipLevels = (uint32_t)(tex2D.levels());
        data.colorFormat = (vk::Format)tex2D.format();
        data.viewType = vk::ImageViewType::e2D;
        textureSize = (uint32_t)tex2D.size();
        textureData = tex2D.data();

        for (uint32_t i = 0; i < data.colorMipLevels; ++i)
        {
            textureMipSizes.push_back((uint32_t)tex2D[i].size());
            textureMipWidths.push_back(tex2D[i].extent().x);
            textureMipHeights.push_back(tex2D[i].extent().y);
            textureMipDepths.push_back(1);
        }

        data.imageType = vk::ImageType::e2D;
    }
    else if (texture.target() == gli::target::TARGET_3D)
    {
        tex3D = gli::texture3d(texture);

        if (tex3D.empty())
            throw std::runtime_error( std::string("Error: image " + imagePath + " is empty"));

        data.width = (uint32_t)(tex3D.extent().x);
        data.height = (uint32_t)(tex3D.extent().y);
        data.depth = (uint32_t)(tex3D.extent().z);

        data.colorMipLevels = (uint32_t)(tex3D.levels());
        //data.colorFormat = (vk::Format)tex3D.format();
        data.colorFormat = vk::Format::eR8G8B8A8Srgb;
        data.viewType = vk::ImageViewType::e3D;
        textureSize = (uint32_t)tex3D.size();
        textureData = tex3D.data();

        for (uint32_t i = 0; i < data.colorMipLevels; ++i)
        {
            textureMipSizes.push_back((uint32_t)tex3D[i].size());
            textureMipWidths.push_back(tex3D[i].extent().x);
            textureMipHeights.push_back(tex3D[i].extent().y);
            textureMipDepths.push_back(tex3D[i].extent().z);
        }
        data.imageType = vk::ImageType::e3D;
    }
    else if (texture.target() == gli::target::TARGET_CUBE)
    {
        texCube = gli::texture_cube(texture);

        if (texCube.empty())
            throw std::runtime_error( std::string("Error: image " + imagePath + " is empty"));

        data.width = (uint32_t)(texCube.extent().x);
        data.height = (uint32_t)(texCube.extent().y);
        data.depth = 1;
        data.viewType = vk::ImageViewType::eCube;
        data.layers = 6;

        data.colorMipLevels = (uint32_t)(texCube.levels());
        data.colorFormat = (vk::Format)texCube.format();
        textureSize = (uint32_t)texCube.size();
        textureData = texCube.data();

        for (int face = 0; face < 6; ++face) {
            for (uint32_t i = 0; i < data.colorMipLevels; ++i)
            {
                // Assuming all faces have the same extent...
                textureMipSizes.push_back((uint32_t)texCube[face][i].size());
                textureMipWidths.push_back(texCube[face][i].extent().x);
                textureMipHeights.push_back(texCube[face][i].extent().y);
                textureMipDepths.push_back(1);
            }
        }
        
        data.imageType = vk::ImageType::e2D;
    }
    else
        throw std::runtime_error( std::string("Error: image " + imagePath + " uses an unsupported target type. "));

    /* Clean up any existing vulkan stuff */
    cleanup();

    texture_struct.mip_levels = data.colorMipLevels;

    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error( std::string("Vulkan library is not initialized"));
    auto device = vulkan->get_device();
    if (device == vk::Device())
        throw std::runtime_error( std::string("Invalid vulkan device"));
    auto physicalDevice = vulkan->get_physical_device();
    if (physicalDevice == vk::PhysicalDevice())
        throw std::runtime_error( std::string("Invalid vulkan physical device"));

    /* Get device properties for the requested texture format. */
    vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(data.colorFormat);

    if (formatProperties.bufferFeatures == vk::FormatFeatureFlags() 
        && formatProperties.linearTilingFeatures == vk::FormatFeatureFlags() 
        && formatProperties.optimalTilingFeatures == vk::FormatFeatureFlags())
        throw std::runtime_error( std::string("Error: Unsupported image format used in " + imagePath));

    /* Create staging buffer */
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = textureSize;
    bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    vk::Buffer stagingBuffer = device.createBuffer(bufferInfo);

    vk::MemoryRequirements stagingMemRequirements = device.getBufferMemoryRequirements(stagingBuffer);
    vk::MemoryAllocateInfo stagingAllocInfo;
    stagingAllocInfo.allocationSize = (uint32_t)stagingMemRequirements.size;
    stagingAllocInfo.memoryTypeIndex = vulkan->find_memory_type(stagingMemRequirements.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    vk::DeviceMemory stagingBufferMemory = device.allocateMemory(stagingAllocInfo);

    device.bindBufferMemory(stagingBuffer, stagingBufferMemory, 0);

    /* Copy texture data into staging buffer */
    void *dataptr = device.mapMemory(stagingBufferMemory, 0, textureSize, vk::MemoryMapFlags());
    memcpy(dataptr, textureData, textureSize);
    device.unmapMemory(stagingBufferMemory);

    /* Setup buffer copy regions for each mip level */
    std::vector<vk::BufferImageCopy> bufferCopyRegions;
    uint32_t offset = 0;

    if (data.viewType == vk::ImageViewType::eCube) {
        for (uint32_t face = 0; face < 6; ++face) {
            for (uint32_t level = 0; level < data.colorMipLevels; level++) {
                vk::BufferImageCopy bufferCopyRegion;
                bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                bufferCopyRegion.imageSubresource.mipLevel = level;
                bufferCopyRegion.imageSubresource.baseArrayLayer = face;
                bufferCopyRegion.imageSubresource.layerCount = 1;                
                bufferCopyRegion.imageExtent.width = texCube[face][level].extent().x;
                bufferCopyRegion.imageExtent.height = texCube[face][level].extent().y;
                bufferCopyRegion.imageExtent.depth = 1;
                bufferCopyRegion.bufferOffset = offset;
                bufferCopyRegions.push_back(bufferCopyRegion);
                offset += (uint32_t)texCube[face][level].size();
            }
        }
    }
    else {
        for (uint32_t i = 0; i < data.colorMipLevels; i++)
        {
            vk::BufferImageCopy bufferCopyRegion;
            bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            bufferCopyRegion.imageSubresource.mipLevel = i;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = textureMipWidths[i];
            bufferCopyRegion.imageExtent.height = textureMipHeights[i];
            bufferCopyRegion.imageExtent.depth = textureMipDepths[i];
            bufferCopyRegion.bufferOffset = offset;
            bufferCopyRegions.push_back(bufferCopyRegion);
            offset += textureMipSizes[i];
        }
    }

    /* Create optimal tiled target image */
    vk::ImageCreateInfo imageCreateInfo;
    imageCreateInfo.imageType = data.imageType;
    imageCreateInfo.format = data.colorFormat;
    imageCreateInfo.mipLevels = data.colorMipLevels;
    imageCreateInfo.arrayLayers = data.layers;
    imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
    imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
    imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;
    imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageCreateInfo.extent = vk::Extent3D{data.width, data.height, data.depth};
    imageCreateInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled;
    if (data.viewType == vk::ImageViewType::eCube) {
        imageCreateInfo.flags |= vk::ImageCreateFlagBits::eCubeCompatible;
    }
    data.colorImage = device.createImage(imageCreateInfo);

    /* Allocate and bind memory for the texture */
    vk::MemoryRequirements imageMemReqs = device.getImageMemoryRequirements(data.colorImage);
    vk::MemoryAllocateInfo imageAllocInfo;
    imageAllocInfo.allocationSize = imageMemReqs.size;
    imageAllocInfo.memoryTypeIndex = vulkan->find_memory_type(
        imageMemReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    data.colorImageMemory = device.allocateMemory(imageAllocInfo);

    device.bindImageMemory(data.colorImage, data.colorImageMemory, 0);

    /* Create a command buffer for changing layouts and copying */
    vk::CommandBuffer copyCmd = vulkan->begin_one_time_graphics_command();

    /* Transition between formats */
    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = data.colorMipLevels;
    subresourceRange.layerCount = data.layers;

    /* Transition to transfer destination optimal */
    setImageLayout(
        copyCmd,
        data.colorImage,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        subresourceRange);

    /* Copy mip levels from staging buffer */
    copyCmd.copyBufferToImage(
        stagingBuffer,
        data.colorImage,
        vk::ImageLayout::eTransferDstOptimal,
        (uint32_t)bufferCopyRegions.size(),
        bufferCopyRegions.data());

    /* Transition to shader read optimal */
    data.colorImageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    setImageLayout(
        copyCmd,
        data.colorImage,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        subresourceRange);

    vulkan->end_one_time_graphics_command(copyCmd, "transition new ktx image", true, submit_immediately);

    /* Clean up staging resources */
    device.destroyBuffer(stagingBuffer);
    device.freeMemory(stagingBufferMemory);
    
    /* Create the image view */
    vk::ImageViewCreateInfo vInfo;
    vInfo.viewType = data.viewType;
    vInfo.format = data.colorFormat;
    vInfo.subresourceRange = subresourceRange;
    vInfo.image = data.colorImage;
    data.colorImageView = device.createImageView(vInfo);
}

void Texture::create_color_image_resources(bool submit_immediately)
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error( std::string("Vulkan library is not initialized"));
    auto device = vulkan->get_device();
    if (device == vk::Device())
        throw std::runtime_error( std::string("Invalid vulkan device"));
    auto physicalDevice = vulkan->get_physical_device();
    if (physicalDevice == vk::PhysicalDevice())
        throw std::runtime_error( std::string("Invalid vulkan physical device"));

    
    // /* Destroy samplers */
    // if (data.colorSampler)
    //     device.destroySampler(data.colorSampler);

    /* Destroy Image Views */
    if (data.colorImageView)
        device.destroyImageView(data.colorImageView);

    /* Destroy Images */
    if (data.colorImage)
        device.destroyImage(data.colorImage);

    /* Free Memory */
    if (data.colorImageMemory)
        device.freeMemory(data.colorImageMemory);

    /* For now, assume the following format: */
    data.colorFormat = vk::Format::eR16G16B16A16Sfloat;

    data.colorImageLayout = vk::ImageLayout::eUndefined;

    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = data.colorFormat;
    imageInfo.extent.width = data.width;
    imageInfo.extent.height = data.height;
    imageInfo.extent.depth = data.depth;
    imageInfo.mipLevels = data.colorMipLevels;
    imageInfo.arrayLayers = data.layers;
    imageInfo.samples = data.sampleCount;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
    imageInfo.initialLayout = data.colorImageLayout;
    if (data.viewType == vk::ImageViewType::eCube)
    {
        imageInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;
    }
    data.colorImage = device.createImage(imageInfo);

    vk::MemoryRequirements memReqs = device.getImageMemoryRequirements(data.colorImage);
    vk::MemoryAllocateInfo memAlloc;
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = vulkan->find_memory_type(memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    data.colorImageMemory = device.allocateMemory(memAlloc);
    device.bindImageMemory(data.colorImage, data.colorImageMemory, 0);

    /* Transition to a usable format */
    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.levelCount = data.colorMipLevels;
    subresourceRange.layerCount = data.layers;

    vk::CommandBuffer cmdBuffer = vulkan->begin_one_time_graphics_command();
    setImageLayout(cmdBuffer, data.colorImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, subresourceRange);
    data.colorImageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    vulkan->end_one_time_graphics_command(cmdBuffer, "transition new color image", true, submit_immediately);

    /* Create the image view */
    vk::ImageViewCreateInfo vInfo;
    vInfo.viewType = data.viewType;
    vInfo.format = data.colorFormat;
    vInfo.subresourceRange = subresourceRange;
    vInfo.image = data.colorImage;
    data.colorImageView = device.createImageView(vInfo);
    
    /* Create more image views */
    data.colorImageViewLayers.clear();
    for(uint32_t i = 0; i < data.layers; i++) {
        subresourceRange.baseMipLevel = 0;
        subresourceRange.baseArrayLayer = i;
        subresourceRange.levelCount = data.colorMipLevels;
        subresourceRange.layerCount = 1;
        
        vInfo.viewType = data.viewType;
        vInfo.format = data.colorFormat;
        vInfo.subresourceRange = subresourceRange;
        vInfo.image = data.colorImage;
        data.colorImageViewLayers.push_back(device.createImageView(vInfo));
    }
}

void Texture::create_depth_stencil_resources(bool submit_immediately)
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error( std::string("Vulkan library is not initialized"));
    auto device = vulkan->get_device();
    if (device == vk::Device())
        throw std::runtime_error( std::string("Invalid vulkan device"));
    auto physicalDevice = vulkan->get_physical_device();
    if (physicalDevice == vk::PhysicalDevice())
        throw std::runtime_error( std::string("Invalid vulkan physical device"));

    // /* Destroy samplers */
    // if (data.depthSampler)
    //     device.destroySampler(data.depthSampler);

    /* Destroy Image Views */
    if (data.depthImageView)
        device.destroyImageView(data.depthImageView);

    /* Destroy Images */
    if (data.depthImage)
        device.destroyImage(data.depthImage);

    /* Free Memory */
    if (data.depthImageMemory)
        device.freeMemory(data.depthImageMemory);

    bool result = get_supported_depth_format(physicalDevice, &data.depthFormat);
    if (!result)
        throw std::runtime_error( std::string("Error: Unable to find a suitable depth format"));

    data.depthImageLayout = vk::ImageLayout::eUndefined;
    
    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = data.depthFormat;
    imageInfo.extent.width = data.width;
    imageInfo.extent.height = data.height;
    imageInfo.extent.depth = data.depth;
    imageInfo.mipLevels = 1; // Doesn't make much sense for a depth map to be mipped... Could change later...
    imageInfo.arrayLayers = data.layers;
    imageInfo.samples = data.sampleCount;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
    imageInfo.initialLayout = data.depthImageLayout;
    if (data.viewType == vk::ImageViewType::eCube)
    {
        imageInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;
    }
    data.depthImage = device.createImage(imageInfo);

    vk::MemoryRequirements memReqs = device.getImageMemoryRequirements(data.depthImage);
    vk::MemoryAllocateInfo memAlloc;
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = vulkan->find_memory_type(memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    data.depthImageMemory = device.allocateMemory(memAlloc);
    device.bindImageMemory(data.depthImage, data.depthImageMemory, 0);

    /* Transition to a usable format */
    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = data.layers;

    vk::CommandBuffer cmdBuffer = vulkan->begin_one_time_graphics_command();
    setImageLayout(cmdBuffer, data.depthImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, subresourceRange);
    data.depthImageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    vulkan->end_one_time_graphics_command(cmdBuffer, "transition new depth image", true, submit_immediately);

    /* Create the image view */
    vk::ImageViewCreateInfo vInfo;
    vInfo.viewType = data.viewType;
    vInfo.format = data.depthFormat;
    vInfo.subresourceRange = subresourceRange;
    vInfo.image = data.depthImage;
    data.depthImageView = device.createImageView(vInfo);
    
    /* Create more image views */
    data.depthImageViewLayers.clear();
    for(uint32_t i = 0; i < data.layers; i++) {
        subresourceRange.baseMipLevel = 0;
        subresourceRange.baseArrayLayer = i;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;
        
        vInfo.viewType = data.viewType;
        vInfo.format = data.depthFormat;
        vInfo.subresourceRange = subresourceRange;
        vInfo.image = data.depthImage;
        data.depthImageViewLayers.push_back(device.createImageView(vInfo));
    }
}

void Texture::createColorImageView()
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error( std::string("Vulkan library is not initialized"));
    auto device = vulkan->get_device();
    if (device == vk::Device())
        throw std::runtime_error( std::string("Invalid vulkan device"));

    // Create image view
    vk::ImageViewCreateInfo info;
    // Cube map view type
    info.viewType = data.viewType;
    info.format = data.colorFormat;
    info.components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
    info.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    // 6 array layers (faces)
    info.subresourceRange.layerCount = data.layers;
    // Set number of mip levels
    info.subresourceRange.levelCount = data.colorMipLevels;
    info.image = data.colorImage;
    data.colorImageView = device.createImageView(info);
}

bool Texture::get_supported_depth_format(vk::PhysicalDevice physicalDevice, vk::Format *depthFormat)
{
    // Since all depth formats may be optional, we need to find a suitable depth format to use
    // Start with the highest precision packed format
    std::vector<vk::Format> depthFormats = {
        vk::Format::eD32SfloatS8Uint,
        vk::Format::eD32Sfloat,
        vk::Format::eD24UnormS8Uint,
        vk::Format::eD16UnormS8Uint,
        vk::Format::eD16Unorm};

    for (auto &format : depthFormats)
    {
        vk::FormatProperties formatProps = physicalDevice.getFormatProperties(format);
        // Format must support depth stencil attachment for optimal tiling
        if (formatProps.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
        {
            *depthFormat = format;
            return true;
        }
    }

    return false;
}

void Texture::cleanup()
{
    if (madeExternally)
        return;

    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error( std::string("Vulkan library is not initialized"));
    auto device = vulkan->get_device();
    if (device == vk::Device())
        throw std::runtime_error( std::string("Invalid vulkan device"));

    /* Destroy samplers */
    // if (data.colorSampler)
    //     device.destroySampler(data.colorSampler);
    // if (data.depthSampler)
    //     device.destroySampler(data.depthSampler);

    /* Destroy Image Views */
    if (data.colorImageView)
        device.destroyImageView(data.colorImageView);
    if (data.depthImageView)
        device.destroyImageView(data.depthImageView);

    /* Destroy Images */
    if (data.colorImage)
        device.destroyImage(data.colorImage);
    if (data.depthImage)
        device.destroyImage(data.depthImage);

    /* Free Memory */
    if (data.colorImageMemory)
        device.freeMemory(data.colorImageMemory);
    if (data.depthImageMemory)
        device.freeMemory(data.depthImageMemory);
}

std::vector<vk::Sampler> Texture::GetSamplers() 
{
    // Get the default texture (for now, just use the default 2D texture)
    auto DefaultTex = Get("DefaultTex2D");
    
    std::vector<vk::Sampler> samplers_(MAX_SAMPLERS);

    // For each texture
    for (int i = 0; i < MAX_SAMPLERS; ++i) {
        if (samplers[i] == vk::Sampler())
            samplers_[i] = samplers[0]; //  Sampler 0 is always defined. (this might change)
        else
            samplers_[i] = samplers[i];
    }
    
    // finally, return the sampler vector
    return samplers_;
}

std::vector<vk::ImageLayout> Texture::GetLayouts(vk::ImageViewType view_type) 
{
    // Get the default texture
    Texture *DefaultTex;
    if (view_type == vk::ImageViewType::e2D) DefaultTex = Get("DefaultTex2D");
    else if (view_type == vk::ImageViewType::e3D) DefaultTex = Get("DefaultTex3D");
    else if (view_type == vk::ImageViewType::eCube) DefaultTex = Get("DefaultTexCube");
    else return {};

    std::vector<vk::ImageLayout> layouts(MAX_TEXTURES);

    // For each texture
    for (int i = 0; i < MAX_TEXTURES; ++i) {
        if (textures[i].initialized 
            && (textures[i].data.colorImageView != vk::ImageView())
            && (textures[i].data.colorImageLayout == vk::ImageLayout::eShaderReadOnlyOptimal) 
            && (textures[i].data.viewType == view_type)) {
            // then add it's layout to the vector
            layouts[i] = textures[i].data.colorImageLayout;
        }
        // otherwise, add the default 2D texture layout
        else {
            layouts[i] = DefaultTex->data.colorImageLayout;
        }
    }
    
    // finally, return the layout vector
    return layouts;
}

/* Static Factory Implementations */
Texture *Texture::CreateFromKTX(std::string name, std::string filepath, bool submit_immediately)
{
    auto tex = StaticFactory::Create(name, "Texture", lookupTable, textures, MAX_TEXTURES);
    if (!tex) return nullptr;
    tex->loadKTX(filepath, submit_immediately);
    return tex;
}

Texture* Texture::CreateCubemap(
    std::string name, uint32_t width, uint32_t height, bool hasColor, bool hasDepth, bool submit_immediately) 
{
    auto tex = StaticFactory::Create(name, "Texture", lookupTable, textures, MAX_TEXTURES);
    if (!tex) return nullptr;

    tex->data.width = width;
    tex->data.height = height;
    tex->data.layers = 6;
    tex->data.imageType = vk::ImageType::e2D;
    tex->data.viewType  = vk::ImageViewType::eCube;
    if (hasColor) tex->create_color_image_resources(submit_immediately);
    if (hasDepth) tex->create_depth_stencil_resources(submit_immediately);
    return tex;
}

Texture* Texture::CreateChecker(std::string name, bool submit_immediately)
{
    // This is just temporary. Planning on using a procedural texture
    std::string ResourcePath = Options::GetResourcePath();
    auto checkerTexturePath = ResourcePath + std::string("/Defaults/checker.ktx");

    auto tex = StaticFactory::Create(name, "Texture", lookupTable, textures, MAX_TEXTURES);
    if (!tex) return nullptr;
    tex->texture_struct.type = 1;
    tex->texture_struct.mip_levels = 0;
    return tex;
}

Texture* Texture::Create2D(
    std::string name, uint32_t width, uint32_t height, 
    bool hasColor, bool hasDepth, uint32_t sampleCount, uint32_t layers, 
    bool submit_immediately)
{
    auto tex = StaticFactory::Create(name, "Texture", lookupTable, textures, MAX_TEXTURES);
    if (!tex) return nullptr;

    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error( std::string("Vulkan library is not initialized"));
    auto sampleFlag = vulkan->highest(vulkan->min(vulkan->get_closest_sample_count_flag(sampleCount), vulkan->get_msaa_sample_flags()));

    tex->data.width = width;
    tex->data.height = height;
    tex->data.layers = layers;
    tex->data.viewType  = vk::ImageViewType::e2D;
    tex->data.imageType = vk::ImageType::e2D;
    tex->data.sampleCount = vulkan->highest(vulkan->min(sampleFlag, vulkan->get_msaa_sample_flags()));

    if (tex->data.sampleCount != sampleFlag)
        std::cout<<"Warning: provided sample count is larger than max supported sample count on the device. Using " 
            << vk::to_string(tex->data.sampleCount) << " instead."<<std::endl;
    if (hasColor) tex->create_color_image_resources(submit_immediately);
    if (hasDepth) tex->create_depth_stencil_resources(submit_immediately);
    return tex;
}

Texture* Texture::Create2DFromColorData (
    std::string name, uint32_t width, uint32_t height, std::vector<float> data, bool submit_immediately)
{
    auto tex = StaticFactory::Create(name, "Texture", lookupTable, textures, MAX_TEXTURES);
    if (!tex) return nullptr;
    tex->data.width = width;
    tex->data.height = height;
    tex->data.layers = 1;
    tex->data.viewType  = vk::ImageViewType::e2D;
    tex->data.imageType = vk::ImageType::e2D;
    tex->create_color_image_resources(submit_immediately);
    tex->upload_color_data(width, height, 1, data);
    return tex;
}

Texture* Texture::CreateFromExternalData(std::string name, Data data)
{
    auto tex = StaticFactory::Create(name, "Texture", lookupTable, textures, MAX_TEXTURES);
    if (!tex) return nullptr;
    tex->setData(data);
    return tex;
}

Texture* Texture::Get(std::string name) {
    return StaticFactory::Get(name, "Texture", lookupTable, textures, MAX_TEXTURES);
}

Texture* Texture::Get(uint32_t id) {
    return StaticFactory::Get(id, "Texture", lookupTable, textures, MAX_TEXTURES);
}

void Texture::Delete(std::string name) {
    StaticFactory::Delete(name, "Texture", lookupTable, textures, MAX_TEXTURES);
}

void Texture::Delete(uint32_t id) {
    StaticFactory::Delete(id, "Texture", lookupTable, textures, MAX_TEXTURES);
}

Texture* Texture::GetFront() {
    return textures;
}

uint32_t Texture::GetCount() {
    return MAX_TEXTURES;
}
