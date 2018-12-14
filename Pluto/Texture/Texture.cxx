#include "./Texture.hxx"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>


std::map<std::string, std::shared_ptr<Texture>> Texture::textures = std::map<std::string, std::shared_ptr<Texture>>();

bool Texture::does_component_exist(std::string name)
{
    auto it = textures.find(name);
    if (it != textures.end())
    {
        return true;
    }
    return false;
}

/* Constructors */
std::shared_ptr<Texture> Texture::CreateFromKTX(std::string name, std::string filepath)
{
    if (does_component_exist(name))
    {
        std::cout << "Error, Texture \"" << name << "\" already exists." << std::endl;
        return nullptr;
    }
    std::cout << "Adding Texture \"" << name << "\"" << std::endl;
    auto tex = std::make_shared<Texture>(name);
    if (!tex->loadKTX(filepath))
    {
        return nullptr;
    }
    textures[name] = tex;
    return tex;
}

std::shared_ptr<Texture> Texture::CreateCubemap(
    std::string name, uint32_t width, uint32_t height, bool hasColor, bool hasDepth) 
{
    if (does_component_exist(name))
    {
        std::cout << "Error, Texture \"" << name << "\" already exists." << std::endl;
        return nullptr;
    }
    std::cout << "Adding Texture \"" << name << "\"" << std::endl;
    auto tex = std::make_shared<Texture>(name);
    tex->data.width = width;
    tex->data.height = height;
    tex->data.layers = 6;
    tex->data.imageType = vk::ImageType::e2D;
    tex->data.viewType  = vk::ImageViewType::eCube;
    if (hasColor) tex->create_color_image_resources();
    if (hasDepth) tex->create_depth_stencil_resources();
    textures[name] = tex;
    return tex;
}

std::shared_ptr<Texture> Texture::Create2D(
    std::string name, uint32_t width, uint32_t height, bool hasColor, bool hasDepth)
{
    if (does_component_exist(name))
    {
        std::cout << "Error, Texture \"" << name << "\" already exists." << std::endl;
        return nullptr;
    }
    std::cout << "Adding Texture \"" << name << "\"" << std::endl;
    auto tex = std::make_shared<Texture>(name);
    tex->data.width = width;
    tex->data.height = height;
    tex->data.layers = 1;
    tex->data.viewType  = vk::ImageViewType::e2D;
    tex->data.imageType = vk::ImageType::e2D;
    if (hasColor) tex->create_color_image_resources();
    if (hasDepth) tex->create_depth_stencil_resources();
    textures[name] = tex;
    return tex;
}

std::shared_ptr<Texture> Texture::Create2DFromColorData (
    std::string name, uint32_t width, uint32_t height, std::vector<float> data)
{
    if (does_component_exist(name))
    {
        std::cout << "Error, Texture \"" << name << "\" already exists." << std::endl;
        return nullptr;
    }
    std::cout << "Adding Texture \"" << name << "\"" << std::endl;
    auto tex = std::make_shared<Texture>(name);
    tex->data.width = width;
    tex->data.height = height;
    tex->data.layers = 1;
    tex->data.viewType  = vk::ImageViewType::e2D;
    tex->data.imageType = vk::ImageType::e2D;
    tex->create_color_image_resources();
    tex->upload_color_data(width, height, 1, data);
    textures[name] = tex;
    return tex;
}

std::shared_ptr<Texture> Texture::CreateFromExternalData(std::string name, Data data)
{
    if (does_component_exist(name))
    {
        std::cout << "Error, Texture \"" << name << "\" already exists." << std::endl;
        return nullptr;
    }
    std::cout << "Adding Texture \"" << name << "\"" << std::endl;
    auto tex = std::make_shared<Texture>(name);
    textures[name] = tex;
    tex->setData(data);
    return tex;
}

std::shared_ptr<Texture> Texture::Get(std::string name)
{
    if (does_component_exist(name))
        return textures[name];

    std::cout << "Error: Texture \"" << name << "\" does not exist." << std::endl;
    return nullptr;
}