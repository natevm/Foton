#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif 

#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/common.hpp>
#include <atomic>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "VinylEngine/Libraries/Vulkan/Vulkan.hxx"
#include "VinylEngine/Components/Component/Component.hxx"
#include "VinylEngine/Components/ComponentManager/ComponentManager.hxx"

namespace Components {
	using namespace glm;
	enum Space {
		Local,
		Parent
	};

	struct TransformBufferObject {
		glm::mat4 worldToLocal;
		glm::mat4 localToWorld;
	};

	using namespace std;
	class Transform : public Component {
	public:
		// Properties
		bool hasChanged = false;

		atomic<vec3> scale = vec3(1.0);
		atomic<vec3> position = vec3(0.0);
		atomic<quat> rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		//vec3 eulerAngles = vec3();

		/* TODO: Make these constants */
		vec3 worldRight = vec3(1.0, 0.0, 0.0);
		vec3 worldUp = vec3(0.0, 1.0, 0.0);
		vec3 worldForward = vec3(0.0, 0.0, 1.0);

		atomic<vec3> right = vec3(1.0, 0.0, 0.0);
		atomic<vec3> up = vec3(0.0, 1.0, 0.0);
		atomic<vec3> forward = vec3(0.0, 0.0, 1.0);

		atomic<mat4> localToParentTransform = mat4(1);
		atomic<mat4> localToParentRotation = mat4(1);
		atomic<mat4> localToParentPosition = mat4(1);
		atomic<mat4> localToParentScale = mat4(1);

		atomic<mat4> parentToLocalTransform = mat4(1);
		atomic<mat4> parentToLocalRotation = mat4(1);
		atomic<mat4> parentToLocalPosition = mat4(1);
		atomic<mat4> parentToLocalScale = mat4(1);

		atomic<mat4> localToParentMatrix = mat4(1);
		atomic<mat4> parentToLocalMatrix = mat4(1);

		static std::shared_ptr<Transform> Create(std::string name) {
			std::cout << "ComponentManager: Adding Transform \"" << name << "\"" << std::endl;
			auto transform = std::make_shared<Transform>();
			ComponentManager::Get()->transforms[name] = transform;
			return transform;
		}

		/*
		Transform constructor
		*/
		Transform() {
			createUniformBuffer();
		}

		Transform(const Transform &other) {
			*this = other;
		}

		Transform & operator= (const Transform & other)
		{
			if (this != &other) // protect against invalid self-assignment
			{
				this->localToParentRotation = other.localToParentRotation.load();
				this->localToParentPosition = other.localToParentPosition.load();
				this->localToParentScale = other.localToParentScale.load();
				this->localToParentTransform = other.localToParentTransform.load();

				this->parentToLocalRotation = other.parentToLocalRotation.load();
				this->parentToLocalPosition = other.parentToLocalPosition.load();
				this->parentToLocalScale = other.parentToLocalScale.load();
				this->parentToLocalTransform = other.parentToLocalTransform.load();

				this->localToParentMatrix = other.localToParentMatrix.load();
				this->parentToLocalMatrix = other.parentToLocalMatrix.load();

				this->hasChanged = other.hasChanged;

				this->scale.store(other.scale);
				this->position.store(other.position);
				this->rotation.store(other.rotation);
				//this->eulerAngles = other.eulerAngles;

				this->forward.store(other.forward);
				this->right.store(other.right);
				this->up.store(other.up);

				this->transformUBO = other.transformUBO;
				this->transformUBOMemory = other.transformUBOMemory;
			}
			return *this;
		}

		vk::Buffer transformUBO;
		vk::DeviceMemory transformUBOMemory;

		void createUniformBuffer() {
			auto vulkan = Libraries::Vulkan::Get();
			auto device = vulkan->get_device();
			auto physical_device = vulkan->get_physical_device();

			vk::BufferCreateInfo bufferInfo = {};
			bufferInfo.size = sizeof(TransformBufferObject);
			bufferInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer;
			bufferInfo.sharingMode = vk::SharingMode::eExclusive;
			vk::Buffer transformUBO = device.createBuffer(bufferInfo);
			
			vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(transformUBO);
			vk::MemoryAllocateInfo allocInfo = {};
			allocInfo.allocationSize = memReqs.size;

			vk::PhysicalDeviceMemoryProperties memProperties = physical_device.getMemoryProperties();
			vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
			for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
				if ((memReqs.memoryTypeBits & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
					allocInfo.memoryTypeIndex = i;
					break;
				}
			}

			vk::DeviceMemory transformUBOMemory = device.allocateMemory(allocInfo);
			device.bindBufferMemory(transformUBO, transformUBOMemory, 0);
		}

		vk::Buffer getUBO() {
			return transformUBO;
		}

		void uploadUBO(glm::mat4 worldToLocal, glm::mat4 localToWorld) {
			/* Update uniform buffer */
			TransformBufferObject tbo = {};
			tbo.worldToLocal = worldToLocal;
			tbo.localToWorld = localToWorld;
		
			auto device = Libraries::Vulkan::Get()->get_device();

			/* Map uniform buffer, copy data directly, then unmap */
			void* data = device.mapMemory(transformUBOMemory, 0, sizeof(tbo));
			memcpy(data, &tbo, sizeof(tbo));
			device.unmapMemory(transformUBOMemory);
		}

		void cleanup() {
			auto device = Libraries::Vulkan::Get()->get_device();
			device.destroyBuffer(transformUBO);
			device.freeMemory(transformUBOMemory);
		}

		/*
		Transforms direction from local to parent.
		This operation is not affected by scale or position of the transform.
		The returned vector has the same length as the input direction.
		*/
		vec3 TransformDirection(vec3 direction) {

			return vec3(localToParentRotation.load() * vec4(direction, 0.0));
		}

		/*
		Transforms position from local to parent. Note, affected by scale.
		The oposition conversion, from parent to local, can be done with Transform.InverseTransformPoint
		*/
		vec3 TransformPoint(vec3 point) {
			return vec3(localToParentMatrix.load() * vec4(point, 1.0));
		}

		/*
		Transforms vector from local to parent.
		This is not affected by position of the transform, but is affected by scale.
		The returned vector may have a different length that the input vector.
		*/
		vec3 TransformVector(vec3 vector) {
			return vec3(localToParentMatrix.load() * vec4(vector, 0.0));
		}

		/*
		Transforms a direction from parent space to local space.
		The opposite of Transform.TransformDirection.
		This operation is unaffected by scale.
		*/
		vec3 InverseTransformDirection(vec3 direction) {
			return vec3(parentToLocalRotation.load() * vec4(direction, 0.0));
		}

		/*
		Transforms position from parent space to local space.
		Essentially the opposite of Transform.TransformPoint.
		Note, affected by scale.
		*/
		vec3 InverseTransformPoint(vec3 point) {
			return vec3(parentToLocalMatrix.load() * vec4(point, 1.0));
		}

		/*
		Transforms a vector from parent space to local space.
		The opposite of Transform.TransformVector.
		This operation is affected by scale.
		*/
		vec3 InverseTransformVector(vec3 vector) {
			return vec3(localToParentMatrix.load() * vec4(vector, 0.0));
		}

		/*
		Rotates the transform so the forward vector points at the target's current position.
		Then it rotates the transform to point its up direction vector in the direction hinted at by the parentUp vector.
		*/
		void LookAt(Transform target, vec3 parentUp);

		/*
		Applies a rotation of eulerAngles.z degrees around the z axis, eulerAngles.x degrees around the x axis, and eulerAngles.y degrees around the y axis (in that order).
		If relativeTo is not specified, rotation is relative to local space.
		*/
		void Rotate(vec3 eularAngles, Space = Space::Local);

		/*
		Rotates the transform about the provided axis, passing through the provided point in parent coordinates by the provided angle in degrees.
		This modifies both the position and rotation of the transform.
		*/
		void RotateAround(vec3 point, vec3 axis, float angle) {
			glm::vec3 direction = point - GetPosition();
			glm::vec3 newPosition = GetPosition() + direction;
			glm::quat newRotation = glm::angleAxis(radians(angle), axis) * GetRotation();
			newPosition = newPosition - direction * glm::angleAxis(radians(-angle), axis);

			rotation.store(newRotation);
			localToParentRotation = glm::toMat4(rotation.load());
			parentToLocalRotation = glm::inverse(localToParentRotation.load());

			position = newPosition;
			localToParentPosition = glm::translate(glm::mat4(1.0), position.load());
			parentToLocalPosition = glm::translate(glm::mat4(1.0), -position.load());

			UpdateMatrix();
		}

		void RotateAround(vec3 point, glm::quat rot) {
			glm::vec3 direction = point - GetPosition();
			glm::vec3 newPosition = GetPosition() + direction;
			glm::quat newRotation = rot * GetRotation();
			newPosition = newPosition - direction * glm::inverse(rot);

			rotation.store(newRotation);
			localToParentRotation = glm::toMat4(rotation.load());
			parentToLocalRotation = glm::inverse(localToParentRotation.load());

			position = newPosition;
			localToParentPosition = glm::translate(glm::mat4(1.0), position.load());
			parentToLocalPosition = glm::translate(glm::mat4(1.0), -position.load());

			UpdateMatrix();
		}

		/* Used primarily for non-trivial transformations */
		void SetTransform(glm::mat4 transformation) {
			this->localToParentTransform.store(transformation);
			this->parentToLocalTransform.store(glm::inverse(transformation));
			UpdateMatrix();
		}

		quat GetRotation() {
			return rotation.load();
		}
		void SetRotation(quat newRotation) {
			rotation.store(newRotation);
			UpdateRotation();
		}
		void SetRotation(float angle, vec3 axis) {
			SetRotation(glm::angleAxis(angle, axis));
		}
		void AddRotation(quat additionalRotation) {
			SetRotation(GetRotation() * additionalRotation);
			UpdateRotation();
		}
		void AddRotation(float angle, vec3 axis) {
			AddRotation(glm::angleAxis(angle, axis));
		}
		void UpdateRotation() {
			localToParentRotation = glm::toMat4(rotation.load());
			parentToLocalRotation = glm::inverse(localToParentRotation.load());
			UpdateMatrix();
		}

		vec3 GetPosition() {
			return position.load();
		}
		void SetPosition(vec3 newPosition) {
			position = newPosition;
			UpdatePosition();
		}
		void AddPosition(vec3 additionalPosition) {
			SetPosition(GetPosition() + additionalPosition);
			UpdatePosition();
		}
		void SetPosition(float x, float y, float z) {
			SetPosition(glm::vec3(x, y, z));
		}
		void AddPosition(float dx, float dy, float dz) {
			AddPosition(glm::vec3(dx, dy, dz));
		}
		void UpdatePosition() {
			localToParentPosition = glm::translate(glm::mat4(1.0), position.load());
			parentToLocalPosition = glm::translate(glm::mat4(1.0), -position.load());
			UpdateMatrix();
		}

		vec3 GetScale() {
			return scale.load();
		}
		void SetScale(vec3 newScale) {
			scale.store(newScale);
			UpdateScale();
		}
		void SetScale(float newScale) {
			scale.store(vec3(newScale, newScale, newScale));
			UpdateScale();
		}
		void AddScale(vec3 additionalScale) {
			SetScale(GetScale() + additionalScale);
			UpdateScale();
		}
		void SetScale(float x, float y, float z) {
			SetScale(glm::vec3(x, y, z));
		}
		void AddScale(float dx, float dy, float dz) {
			AddScale(glm::vec3(dx, dy, dz));
		}
		void AddScale(float ds) {
			AddScale(glm::vec3(ds, ds, ds));
		}
		void UpdateScale() {
			localToParentScale = glm::scale(glm::mat4(1.0), scale.load());
			parentToLocalScale = glm::scale(glm::mat4(1.0), glm::vec3(1.0 / scale.load().x, 1.0 / scale.load().y, 1.0 / scale.load().z));
			UpdateMatrix();
		}

		void UpdateMatrix() {

			localToParentMatrix.store(localToParentTransform.load() * localToParentPosition.load() * localToParentRotation.load() * localToParentScale.load());
			parentToLocalMatrix.store(parentToLocalTransform.load() * parentToLocalScale.load() * parentToLocalRotation.load() * parentToLocalPosition.load());

			right.store(glm::vec3(localToParentMatrix.load()[0]));
			up.store(glm::vec3(localToParentMatrix.load()[1]));
			forward.store(-glm::vec3(localToParentMatrix.load()[2]));
			position.store(glm::vec3(localToParentMatrix.load()[3]));
		}

		glm::mat4 ParentToLocalMatrix() {
			return parentToLocalMatrix.load();
		}

		glm::mat4 LocalToParentMatrix() {
			return localToParentMatrix.load();
		}
	};
}