// #pragma optimize("", off)

#include "./Material.hxx"
#include "Pluto/Tools/Options.hxx"
#include "Pluto/Tools/FileReader.hxx"

#include "Pluto/Entity/Entity.hxx"
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Mesh/Mesh.hxx"
#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Light/Light.hxx"
#include "Pluto/Texture/Texture.hxx"

Material Material::materials[MAX_MATERIALS];
MaterialStruct* Material::pinnedMemory;
std::map<std::string, uint32_t> Material::lookupTable;
vk::Buffer Material::SSBO;
vk::DeviceMemory Material::SSBOMemory;
vk::Buffer Material::stagingSSBO;
vk::DeviceMemory Material::stagingSSBOMemory;

std::shared_ptr<std::mutex> Material::creation_mutex;
bool Material::Initialized = false;

Material::Material() {
	this->initialized = false;
}

Material::Material(std::string name, uint32_t id)
{
	this->initialized = true;
	this->name = name;
	this->id = id;

	/* Working off blender's principled BSDF */
	material_struct.base_color = vec4(.8, .8, .8, 1.0);
	material_struct.subsurface_radius = vec4(1.0, .2, .1, 1.0);
	material_struct.subsurface_color = vec4(.8, .8, .8, 1.0);
	material_struct.subsurface = 0.0;
	material_struct.metallic = 0.0;
	material_struct.specular = .5;
	material_struct.specular_tint = 0.0;
	material_struct.roughness = .5;
	material_struct.anisotropic = 0.0;
	material_struct.anisotropic_rotation = 0.0;
	material_struct.sheen = 0.0;
	material_struct.sheen_tint = 0.5;
	material_struct.clearcoat = 0.0;
	material_struct.clearcoat_roughness = .03f;
	material_struct.ior = 1.45f;
	material_struct.transmission = 0.0;
	material_struct.transmission_roughness = 0.0;
	material_struct.volume_texture_id = -1;
	material_struct.flags = 0;
	material_struct.base_color_texture_id = -1;
	material_struct.roughness_texture_id = -1;
	material_struct.occlusion_texture_id = -1;
	material_struct.transfer_function_texture_id = -1;
	material_struct.bump_texture_id = -1;
	material_struct.alpha_texture_id = -1;
}

std::string Material::to_string() {
	std::string output;
	output += "{\n";
	output += "\ttype: \"Material\",\n";
	output += "\tname: \"" + name + "\"\n";
	output += "\tbase_color: \"" + glm::to_string(material_struct.base_color) + "\"\n";
	output += "\tsubsurface: \"" + std::to_string(material_struct.subsurface) + "\"\n";
	output += "\tsubsurface_radius: \"" + glm::to_string(material_struct.subsurface_radius) + "\"\n";
	output += "\tsubsurface_color: \"" + glm::to_string(material_struct.subsurface_color) + "\"\n";
	output += "\tmetallic: \"" + std::to_string(material_struct.metallic) + "\"\n";
	output += "\tspecular: \"" + std::to_string(material_struct.specular) + "\"\n";
	output += "\tspecular_tint: \"" + std::to_string(material_struct.specular_tint) + "\"\n";
	output += "\troughness: \"" + std::to_string(material_struct.roughness) + "\"\n";
	output += "\tanisotropic: \"" + std::to_string(material_struct.anisotropic) + "\"\n";
	output += "\tanisotropic_rotation: \"" + std::to_string(material_struct.anisotropic_rotation) + "\"\n";
	output += "\tsheen: \"" + std::to_string(material_struct.sheen) + "\"\n";
	output += "\tsheen_tint: \"" + std::to_string(material_struct.sheen_tint) + "\"\n";
	output += "\tclearcoat: \"" + std::to_string(material_struct.clearcoat) + "\"\n";
	output += "\tclearcoat_roughness: \"" + std::to_string(material_struct.clearcoat_roughness) + "\"\n";
	output += "\tior: \"" + std::to_string(material_struct.ior) + "\"\n";
	output += "\ttransmission: \"" + std::to_string(material_struct.transmission) + "\"\n";
	output += "\ttransmission_roughness: \"" + std::to_string(material_struct.transmission_roughness) + "\"\n";
	output += "}";
	return output;
}

void Material::Initialize()
{
	if (IsInitialized()) return;

	creation_mutex = std::make_shared<std::mutex>();

	Material::CreateSSBO();
	
	Initialized = true;
}

bool Material::IsInitialized()
{
	return Initialized;
}

void Material::CreateSSBO() 
{
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();
	auto physical_device = vulkan->get_physical_device();

	{
		vk::BufferCreateInfo bufferInfo = {};
		bufferInfo.size = MAX_MATERIALS * sizeof(MaterialStruct);
		bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
		bufferInfo.sharingMode = vk::SharingMode::eExclusive;
		stagingSSBO = device.createBuffer(bufferInfo);

		vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(stagingSSBO);
		vk::MemoryAllocateInfo allocInfo = {};
		allocInfo.allocationSize = memReqs.size;

		vk::PhysicalDeviceMemoryProperties memProperties = physical_device.getMemoryProperties();
		vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
		allocInfo.memoryTypeIndex = vulkan->find_memory_type(memReqs.memoryTypeBits, properties);

		stagingSSBOMemory = device.allocateMemory(allocInfo);
		device.bindBufferMemory(stagingSSBO, stagingSSBOMemory, 0);
	}

	{
		vk::BufferCreateInfo bufferInfo = {};
		bufferInfo.size = MAX_MATERIALS * sizeof(MaterialStruct);
		bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
		bufferInfo.sharingMode = vk::SharingMode::eExclusive;
		SSBO = device.createBuffer(bufferInfo);

		vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(SSBO);
		vk::MemoryAllocateInfo allocInfo = {};
		allocInfo.allocationSize = memReqs.size;

		vk::PhysicalDeviceMemoryProperties memProperties = physical_device.getMemoryProperties();
		vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
		allocInfo.memoryTypeIndex = vulkan->find_memory_type(memReqs.memoryTypeBits, properties);

		SSBOMemory = device.allocateMemory(allocInfo);
		device.bindBufferMemory(SSBO, SSBOMemory, 0);
	}
}

void Material::UploadSSBO(vk::CommandBuffer command_buffer)
{
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	if (SSBOMemory == vk::DeviceMemory()) return;
	if (stagingSSBOMemory == vk::DeviceMemory()) return;

	auto bufferSize = MAX_MATERIALS * sizeof(MaterialStruct);

	/* Pin the buffer */
	pinnedMemory = (MaterialStruct*) device.mapMemory(stagingSSBOMemory, 0, bufferSize);

	if (pinnedMemory == nullptr) return;
	MaterialStruct material_structs[MAX_MATERIALS];
	
	/* TODO: remove this for loop */
	for (int i = 0; i < MAX_MATERIALS; ++i) {
		if (!materials[i].is_initialized()) continue;
		material_structs[i] = materials[i].material_struct;
	};

	/* Copy to GPU mapped memory */
	memcpy(pinnedMemory, material_structs, sizeof(material_structs));

	device.unmapMemory(stagingSSBOMemory);

	vk::BufferCopy copyRegion;
	copyRegion.size = bufferSize;
	command_buffer.copyBuffer(stagingSSBO, SSBO, copyRegion);
} 

vk::Buffer Material::GetSSBO()
{
	if ((SSBO != vk::Buffer()) && (SSBOMemory != vk::DeviceMemory()))
		return SSBO;
	else return vk::Buffer();
}

uint32_t Material::GetSSBOSize()
{
	return MAX_MATERIALS * sizeof(MaterialStruct);
}

void Material::CleanUp()
{
	if (!IsInitialized()) return;

	for (auto &material : materials) {
		if (material.initialized) {
			Material::Delete(material.id);
		}
	}

	auto vulkan = Libraries::Vulkan::Get();
	if (!vulkan->is_initialized())
		throw std::runtime_error( std::string("Vulkan library is not initialized"));
	auto device = vulkan->get_device();
	if (device == vk::Device())
		throw std::runtime_error( std::string("Invalid vulkan device"));

	device.destroyBuffer(SSBO);
	device.freeMemory(SSBOMemory);

	device.destroyBuffer(stagingSSBO);
	device.freeMemory(stagingSSBOMemory);

	SSBO = vk::Buffer();
	SSBOMemory = vk::DeviceMemory();
	stagingSSBO = vk::Buffer();
	stagingSSBOMemory = vk::DeviceMemory();

	Initialized = false;
}	

/* Static Factory Implementations */
Material* Material::Create(std::string name) {
	return StaticFactory::Create(creation_mutex, name, "Material", lookupTable, materials, MAX_MATERIALS);
}

Material* Material::Get(std::string name) {
	return StaticFactory::Get(creation_mutex, name, "Material", lookupTable, materials, MAX_MATERIALS);
}

Material* Material::Get(uint32_t id) {
	return StaticFactory::Get(creation_mutex, id, "Material", lookupTable, materials, MAX_MATERIALS);
}

void Material::Delete(std::string name) {
	StaticFactory::Delete(creation_mutex, name, "Material", lookupTable, materials, MAX_MATERIALS);
}

void Material::Delete(uint32_t id) {
	StaticFactory::Delete(creation_mutex, id, "Material", lookupTable, materials, MAX_MATERIALS);
}

Material* Material::GetFront() {
	return materials;
}

uint32_t Material::GetCount() {
	return MAX_MATERIALS;
}

void Material::set_base_color_texture(uint32_t texture_id) 
{
	if (texture_id > MAX_TEXTURES)
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.base_color_texture_id = texture_id;
}

void Material::set_base_color_texture(Texture *texture) 
{
	if (!texture) 
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.base_color_texture_id = texture->get_id();
}

void Material::clear_base_color_texture() {
	this->material_struct.base_color_texture_id = -1;
}

void Material::set_roughness_texture(uint32_t texture_id) 
{
	if (texture_id > MAX_TEXTURES)
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.roughness_texture_id = texture_id;
}

void Material::set_roughness_texture(Texture *texture) 
{
	if (!texture) 
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.roughness_texture_id = texture->get_id();
}

void Material::set_bump_texture(uint32_t texture_id)
{
	if (texture_id > MAX_TEXTURES)
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.bump_texture_id = texture_id;
}

void Material::set_bump_texture(Texture *texture)
{
	if (!texture)
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.bump_texture_id = texture->get_id();
}

void Material::clear_bump_texture()
{
	this->material_struct.bump_texture_id = -1;
}

void Material::set_alpha_texture(uint32_t texture_id)
{
	if (texture_id > MAX_TEXTURES)
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.alpha_texture_id = texture_id;
}

void Material::set_alpha_texture(Texture *texture)
{
	if (!texture)
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.alpha_texture_id = texture->get_id();
}

void Material::clear_alpha_texture()
{
	this->material_struct.alpha_texture_id = -1;
}

void Material::use_vertex_colors(bool use)
{
	if (use) {
		this->material_struct.flags |= (1 << 0);
	} else {
		this->material_struct.flags &= ~(1 << 0);
	}
}

void Material::set_volume_texture(uint32_t texture_id)
{
	this->material_struct.volume_texture_id = texture_id;
}

void Material::set_volume_texture(Texture *texture)
{
	if (!texture) 
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.volume_texture_id = texture->get_id();
}

void Material::clear_roughness_texture() {
	this->material_struct.roughness_texture_id = -1;
}

void Material::set_transfer_function_texture(uint32_t texture_id)
{
	this->material_struct.transfer_function_texture_id = texture_id;
}

void Material::set_transfer_function_texture(Texture *texture)
{
	if (!texture) 
		throw std::runtime_error( std::string("Invalid texture handle"));
	this->material_struct.transfer_function_texture_id = texture->get_id();
}

void Material::clear_transfer_function_texture()
{
	this->material_struct.transfer_function_texture_id = -1;
}


void Material::show_pbr() {
	renderMode = RENDER_MODE_PBR;
	this->material_struct.flags &= ~(1 << 1);
}

void Material::show_normals () {
	renderMode = RENDER_MODE_NORMAL;
	this->material_struct.flags &= ~(1 << 1);
}

void Material::show_base_color() {
	renderMode = RENDER_MODE_BASECOLOR;
	this->material_struct.flags &= ~(1 << 1);
}

void Material::show_texcoords() {
	renderMode = RENDER_MODE_TEXCOORD;
	this->material_struct.flags &= ~(1 << 1);
}

void Material::show_blinn() {
	renderMode = RENDER_MODE_BLINN;
	this->material_struct.flags &= ~(1 << 1);
}

void Material::show_depth() {
	renderMode = RENDER_MODE_DEPTH;
	this->material_struct.flags &= ~(1 << 1);
}

void Material::show_fragment_depth() {
	renderMode = RENDER_MODE_FRAGMENTDEPTH;
	this->material_struct.flags &= ~(1 << 1);
}

void Material::show_vr_mask() {
	renderMode = RENDER_MODE_VRMASK;
	this->material_struct.flags &= ~(1 << 1);
}

void Material::show_position() {
	renderMode = RENDER_MODE_FRAGMENTPOSITION;
	this->material_struct.flags &= ~(1 << 1);
}

void Material::show_volume() {
	renderMode = RENDER_MODE_VOLUME;
	this->material_struct.flags &= ~(1 << 1);
}

void Material::show_environment() {
	renderMode = RENDER_MODE_SKYBOX;
	this->material_struct.flags &= ~(1 << 1);

}

void Material::hide() {
	this->material_struct.flags |= (1 << 1);
	renderMode = RENDER_MODE_HIDDEN;
}

void Material::set_base_color(glm::vec4 color) {
	this->material_struct.base_color = color;
}

void Material::set_base_color(float r, float g, float b, float a) {
	this->material_struct.base_color = glm::vec4(r, g, b, a);
}

glm::vec4 Material::get_base_color() {
	return this->material_struct.base_color;
}

void Material::set_roughness(float roughness) {
	this->material_struct.roughness = roughness;
}

float Material::get_roughness() {
	return this->material_struct.roughness;
}

void Material::set_metallic(float metallic) {
	this->material_struct.metallic = metallic;
}

float Material::get_metallic() {
	return this->material_struct.metallic;
}

void Material::set_transmission(float transmission) {
	this->material_struct.transmission = transmission;
}

float Material::get_transmission() {
	return this->material_struct.transmission;
}

void Material::set_transmission_roughness(float transmission_roughness) {
	this->material_struct.transmission_roughness = transmission_roughness;
}

float Material::get_transmission_roughness() {
	return this->material_struct.transmission_roughness;
}

void Material::set_ior(float ior) {
	this->material_struct.ior = ior;
}

bool Material::contains_transparency() {
	/* We can expand this to other transparency cases if needed */
	if ((this->material_struct.flags & (1 << 1)) != 0) return true;
	if (this->material_struct.alpha_texture_id != -1) return true;
	if (this->material_struct.base_color.a < 1.0f) return true;
	if (this->renderMode == RENDER_MODE_VOLUME) return true;
	return false;
}