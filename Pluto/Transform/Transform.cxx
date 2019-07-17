#include "./Transform.hxx"

Transform Transform::transforms[MAX_TRANSFORMS];
TransformStruct* Transform::pinnedMemory;
std::map<std::string, uint32_t> Transform::lookupTable;
vk::Buffer Transform::stagingSSBO;
vk::Buffer Transform::SSBO;
vk::DeviceMemory Transform::stagingSSBOMemory;
vk::DeviceMemory Transform::SSBOMemory;
std::shared_ptr<std::mutex> Transform::creation_mutex;
bool Transform::Initialized = false;

void Transform::Initialize()
{
	if (IsInitialized()) return;

	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();
	auto physical_device = vulkan->get_physical_device();

	{
		vk::BufferCreateInfo bufferInfo = {};
		bufferInfo.size = MAX_TRANSFORMS * sizeof(TransformStruct);
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
		bufferInfo.size = MAX_TRANSFORMS * sizeof(TransformStruct);
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

	creation_mutex = std::make_shared<std::mutex>();

	Initialized = true;
}

bool Transform::IsInitialized()
{
	return Initialized;
}

void Transform::UploadSSBO(vk::CommandBuffer command_buffer) 
{
	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();

	if (SSBOMemory == vk::DeviceMemory()) return;
	if (stagingSSBOMemory == vk::DeviceMemory()) return;
	
	auto bufferSize = MAX_TRANSFORMS * sizeof(TransformStruct);

	pinnedMemory = (TransformStruct*) device.mapMemory(stagingSSBOMemory, 0, bufferSize);
	if (pinnedMemory == nullptr) return;

	TransformStruct transformObjects[MAX_TRANSFORMS];
	
	/* TODO: remove this for loop */
	for (int i = 0; i < MAX_TRANSFORMS; ++i) {
		if (!transforms[i].is_initialized()) continue;
		transformObjects[i].worldToLocal = transforms[i].get_world_to_local_matrix();
		transformObjects[i].localToWorld = transforms[i].get_local_to_world_matrix();
		transformObjects[i].worldToLocalRotation = transforms[i].get_world_to_local_rotation_matrix();
		transformObjects[i].localToWorldRotation = transforms[i].get_local_to_world_rotation_matrix();
		transformObjects[i].worldToLocalTranslation = transforms[i].get_world_to_local_translation_matrix();
		transformObjects[i].localToWorldTranslation = transforms[i].get_local_to_world_translation_matrix();
		transformObjects[i].worldToLocalScale = transforms[i].get_world_to_local_scale_matrix();
		transformObjects[i].localToWorldScale = transforms[i].get_local_to_world_scale_matrix();
	};

	/* Copy to GPU mapped memory */
	memcpy(pinnedMemory, transformObjects, sizeof(transformObjects));

	device.unmapMemory(stagingSSBOMemory);

	vk::BufferCopy copyRegion;
	copyRegion.size = bufferSize;
	command_buffer.copyBuffer(stagingSSBO, SSBO, copyRegion);

}

vk::Buffer Transform::GetSSBO() 
{
	if ((SSBO != vk::Buffer()) && (SSBOMemory != vk::DeviceMemory()))
		return SSBO;
	else return vk::Buffer();
}

uint32_t Transform::GetSSBOSize()
{
	return MAX_TRANSFORMS * sizeof(TransformStruct);
}

void Transform::CleanUp() 
{
	if (!IsInitialized()) return;

	for (auto &transform : transforms) {
		if (transform.initialized) {
			Transform::Delete(transform.id);
		}
	}

	auto vulkan = Libraries::Vulkan::Get();
	auto device = vulkan->get_device();
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
Transform* Transform::Create(std::string name) {
	return StaticFactory::Create(creation_mutex, name, "Transform", lookupTable, transforms, MAX_TRANSFORMS);
}

Transform* Transform::Get(std::string name) {
	return StaticFactory::Get(creation_mutex, name, "Transform", lookupTable, transforms, MAX_TRANSFORMS);
}

Transform* Transform::Get(uint32_t id) {
	return StaticFactory::Get(creation_mutex, id, "Transform", lookupTable, transforms, MAX_TRANSFORMS);
}

void Transform::Delete(std::string name) {
	StaticFactory::Delete(creation_mutex, name, "Transform", lookupTable, transforms, MAX_TRANSFORMS);
}

void Transform::Delete(uint32_t id) {
	StaticFactory::Delete(creation_mutex, id, "Transform", lookupTable, transforms, MAX_TRANSFORMS);
}

Transform* Transform::GetFront() {
	return transforms;
}

uint32_t Transform::GetCount() {
	return MAX_TRANSFORMS;
}

Transform::Transform() { 
	initialized = false;
}

Transform::Transform(std::string name, uint32_t id) {
	initialized = true; this->name = name; this->id = id;
}

std::string Transform::to_string()
{
	std::string output;
	output += "{\n";
	output += "\ttype: \"Transform\",\n";
	output += "\tname: \"" + name + "\",\n";
	output += "\tid: \"" + std::to_string(id) + "\",\n";
	output += "\tscale: " + glm::to_string(get_scale()) + "\n";
	output += "\tposition: " + glm::to_string(get_position()) + "\n";
	output += "\trotation: " + glm::to_string(get_rotation()) + "\n";
	output += "\tright: " + glm::to_string(right) + "\n";
	output += "\tup: " + glm::to_string(up) + "\n";
	output += "\tforward: " + glm::to_string(forward) + "\n";
	output += "\tlocal_to_parent_matrix: " + glm::to_string(get_local_to_parent_matrix()) + "\n";
	output += "\tparent_to_local_matrix: " + glm::to_string(get_parent_to_local_matrix()) + "\n";
	output += "}";
	return output;
}

vec3 Transform::transform_direction(vec3 direction)
{
	return vec3(localToParentRotation * vec4(direction, 0.0));
}

vec3 Transform::transform_point(vec3 point)
{
	return vec3(localToParentMatrix * vec4(point, 1.0));
}

vec3 Transform::transform_vector(vec3 vector)
{
	return vec3(localToParentMatrix * vec4(vector, 0.0));
}

vec3 Transform::inverse_transform_direction(vec3 direction)
{
	return vec3(parentToLocalRotation * vec4(direction, 0.0));
}

vec3 Transform::inverse_transform_point(vec3 point)
{
	return vec3(parentToLocalMatrix * vec4(point, 1.0));
}

vec3 Transform::inverse_transform_vector(vec3 vector)
{
	return vec3(localToParentMatrix * vec4(vector, 0.0));
}

void Transform::rotate_around(vec3 point, float angle, vec3 axis)
{
	glm::vec3 direction = point - get_position();
	glm::vec3 newPosition = get_position() + direction;
	glm::quat newRotation = glm::angleAxis(radians(angle), axis) * get_rotation();
	newPosition = newPosition - direction * glm::angleAxis(radians(-angle), axis);

	rotation = glm::normalize(newRotation);
	localToParentRotation = glm::toMat4(rotation);
	parentToLocalRotation = glm::inverse(localToParentRotation);

	position = newPosition;
	localToParentTranslation = glm::translate(glm::mat4(1.0), position);
	parentToLocalTranslation = glm::translate(glm::mat4(1.0), -position);

	update_matrix();
}

void Transform::rotate_around(vec3 point, glm::quat rot)
{
	glm::vec3 direction = point - get_position();
	glm::vec3 newPosition = get_position() + direction;
	glm::quat newRotation = rot * get_rotation();
	newPosition = newPosition - direction * glm::inverse(rot);

	rotation = glm::normalize(newRotation);
	localToParentRotation = glm::toMat4(rotation);
	parentToLocalRotation = glm::inverse(localToParentRotation);

	position = newPosition;
	localToParentTranslation = glm::translate(glm::mat4(1.0), position);
	parentToLocalTranslation = glm::translate(glm::mat4(1.0), -position);

	update_matrix();
}

void Transform::set_transform(glm::mat4 transformation, bool decompose)
{
	if (decompose)
	{
		glm::vec3 scale;
		glm::quat rotation;
		glm::vec3 translation;
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(transformation, scale, rotation, translation, skew, perspective);

		/* Decomposition can return negative scales. We make the assumption this is impossible.
		*/

		if (scale.x < 0.0) scale.x *= -1;
		if (scale.y < 0.0) scale.y *= -1;
		if (scale.z < 0.0) scale.z *= -1;
		
		set_position(translation);
		set_scale(scale);
		set_rotation(rotation);
	}
	else {
		this->localToParentTransform = transformation;
		this->parentToLocalTransform = glm::inverse(transformation);
		update_matrix();
	}
}

quat Transform::get_rotation()
{
	return rotation;
}

void Transform::set_rotation(quat newRotation)
{
	rotation = glm::normalize(newRotation);
	update_rotation();
}

void Transform::set_rotation(float angle, vec3 axis)
{
	set_rotation(glm::angleAxis(angle, axis));
}

void Transform::add_rotation(quat additionalRotation)
{
	set_rotation(get_rotation() * additionalRotation);
	update_rotation();
}

void Transform::add_rotation(float angle, vec3 axis)
{
	add_rotation(glm::angleAxis(angle, axis));
}

void Transform::update_rotation()
{
	localToParentRotation = glm::toMat4(rotation);
	parentToLocalRotation = glm::inverse(localToParentRotation);
	update_matrix();
}

vec3 Transform::get_position()
{
	return position;
}

vec3 Transform::get_right()
{
	return right;
}

vec3 Transform::get_up()
{
	return up;
}

vec3 Transform::get_forward()
{
	return forward;
}

void Transform::set_position(vec3 newPosition)
{
	position = newPosition;
	update_position();
}

void Transform::add_position(vec3 additionalPosition)
{
	set_position(get_position() + additionalPosition);
	update_position();
}

void Transform::set_position(float x, float y, float z)
{
	set_position(glm::vec3(x, y, z));
}

void Transform::add_position(float dx, float dy, float dz)
{
	add_position(glm::vec3(dx, dy, dz));
}

void Transform::update_position()
{
	localToParentTranslation = glm::translate(glm::mat4(1.0), position);
	parentToLocalTranslation = glm::translate(glm::mat4(1.0), -position);
	update_matrix();
}

vec3 Transform::get_scale()
{
	return scale;
}

void Transform::set_scale(vec3 newScale)
{
	scale = newScale;
	update_scale();
}

void Transform::set_scale(float newScale)
{
	scale = vec3(newScale, newScale, newScale);
	update_scale();
}

void Transform::add_scale(vec3 additionalScale)
{
	set_scale(get_scale() + additionalScale);
	update_scale();
}

void Transform::set_scale(float x, float y, float z)
{
	set_scale(glm::vec3(x, y, z));
}

void Transform::add_scale(float dx, float dy, float dz)
{
	add_scale(glm::vec3(dx, dy, dz));
}

void Transform::add_scale(float ds)
{
	add_scale(glm::vec3(ds, ds, ds));
}

void Transform::update_scale()
{
	localToParentScale = glm::scale(glm::mat4(1.0), scale);
	parentToLocalScale = glm::scale(glm::mat4(1.0), glm::vec3(1.0 / scale.x, 1.0 / scale.y, 1.0 / scale.z));
	update_matrix();
}

void Transform::update_matrix()
{
	localToParentMatrix = (localToParentTransform * localToParentTranslation * localToParentRotation * localToParentScale);
	parentToLocalMatrix = (parentToLocalScale * parentToLocalRotation * parentToLocalTranslation * parentToLocalTransform);

	right = glm::vec3(localToParentMatrix[0]);
	forward = glm::vec3(localToParentMatrix[1]);
	up = glm::vec3(localToParentMatrix[2]);
	position = glm::vec3(localToParentMatrix[3]);

	update_children();
}

glm::mat4 Transform::compute_world_to_local_matrix()
{
	glm::mat4 parentMatrix = glm::mat4(1.0);
	if (parent != -1) {
		parentMatrix = transforms[parent].compute_world_to_local_matrix();
		return get_parent_to_local_matrix() * parentMatrix;
	}
	else return get_parent_to_local_matrix();
}

void Transform::update_world_matrix()
{
	if (parent == -1) {
		worldToLocalMatrix = parentToLocalMatrix;
		localToWorldMatrix = localToParentMatrix;

		worldScale = scale;
		worldTranslation = position;
		worldRotation = rotation;
		worldSkew = glm::vec3(0.f, 0.f, 0.f);
		worldPerspective = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // not sure what this should default to...
	} else {
		worldToLocalMatrix = compute_world_to_local_matrix();
		localToWorldMatrix = glm::inverse(worldToLocalMatrix); 
		glm::decompose(localToWorldMatrix, worldScale, worldRotation, worldTranslation, worldSkew, worldPerspective);
	}
}

glm::mat4 Transform::get_parent_to_local_matrix()
{
	return /*(interpolation >= 1.0 ) ?*/ parentToLocalMatrix /*: glm::interpolate(glm::mat4(1.0), parentToLocalMatrix, interpolation)*/;
}

glm::mat4 Transform::get_local_to_parent_matrix()
{
	return /*(interpolation >= 1.0 ) ?*/ localToParentMatrix /*: glm::interpolate(glm::mat4(1.0), localToParentMatrix, interpolation)*/;
}

glm::mat4 Transform::get_local_to_parent_translation_matrix()
{
	return localToParentTranslation;
}

glm::mat4 Transform::get_local_to_parent_scale_matrix()
{
	return localToParentScale;
}

glm::mat4 Transform::get_local_to_parent_rotation_matrix()
{
	return localToParentRotation;
}

glm::mat4 Transform::get_parent_to_local_translation_matrix()
{
	return parentToLocalTranslation;
}

glm::mat4 Transform::get_parent_to_local_scale_matrix()
{
	return parentToLocalScale;
}

glm::mat4 Transform::get_parent_to_local_rotation_matrix()
{
	return parentToLocalRotation;
}

void Transform::set_parent(uint32_t parent) {
	if ((parent < 0) || (parent >= MAX_TRANSFORMS))
		throw std::runtime_error(std::string("Error: parent must be between 0 and ") + std::to_string(MAX_TRANSFORMS));
	
	if (parent == this->get_id())
		throw std::runtime_error(std::string("Error: a component cannot be the parent of itself"));

	this->parent = parent;
	transforms[parent].children.insert(this->id);
	update_children();
}

void Transform::clear_parent()
{
	if ((parent < 0) || (parent >= MAX_TRANSFORMS)){
		parent = -1;
		return;
	}
	
	transforms[parent].children.erase(this->id);
	this->parent = -1;
	update_children();
}

void Transform::add_child(uint32_t object) {
	if ((object < 0) || (object >= MAX_TRANSFORMS))
		throw std::runtime_error(std::string("Error: child must be between 0 and ") + std::to_string(MAX_TRANSFORMS));
	
	if (object == this->get_id())
		throw std::runtime_error(std::string("Error: a component cannot be it's own child"));

	children.insert(object);
	transforms[object].parent = this->id;
	transforms[object].update_world_matrix();
}

void Transform::remove_child(uint32_t object) {
	if ((object < 0) || (object >= MAX_TRANSFORMS))
		throw std::runtime_error(std::string("Error: child must be between 0 and ") + std::to_string(MAX_TRANSFORMS));
	
	if (object == this->get_id())
		throw std::runtime_error(std::string("Error: a component cannot be it's own child"));

	if (children.find(object) == children.end()) 
		throw std::runtime_error(std::string("Error: child does not exist"));

	children.erase(object);
	transforms[object].parent = -1;
	transforms[object].update_world_matrix();
}

glm::mat4 Transform::get_world_to_local_matrix() {
	return worldToLocalMatrix;
}

glm::mat4 Transform::get_local_to_world_matrix() {
	return localToWorldMatrix;
}

glm::quat Transform::get_world_rotation() {
	return worldRotation;
}

glm::vec3 Transform::get_world_translation() {
	return worldTranslation;
}

glm::mat4 Transform::get_world_to_local_rotation_matrix()
{
	return glm::toMat4(glm::inverse(worldRotation));
}

glm::mat4 Transform::get_local_to_world_rotation_matrix()
{
	return glm::toMat4(worldRotation);
}

glm::mat4 Transform::get_world_to_local_translation_matrix()
{
	glm::mat4 m(1.0);
	m = glm::translate(m, -1.0f * worldTranslation);
	return m;
}

glm::mat4 Transform::get_local_to_world_translation_matrix()
{
	glm::mat4 m(1.0);
	m = glm::translate(m, worldTranslation);
	return m;
}

glm::mat4 Transform::get_world_to_local_scale_matrix()
{
	glm::mat4 m(1.0);
	m = glm::scale(m, 1.0f / worldScale);
	return m;
}

glm::mat4 Transform::get_local_to_world_scale_matrix()
{
	glm::mat4 m(1.0);
	m = glm::scale(m, worldScale);
	return m;
}

void Transform::update_children()
{
	for (auto &c : children) {
		auto &t = transforms[c];
		t.update_children();
	}

	update_world_matrix();
}