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
#include <glm/gtx/string_cast.hpp>
#include <glm/common.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_interpolation.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <map>
#include <mutex>

#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Tools/StaticFactory.hxx"

using namespace glm;
using namespace std;

#include "./TransformStruct.hxx"

class Transform : public StaticFactory
{
  private:
    vec3 scale = vec3(1.0);
    vec3 position = vec3(0.0);
    quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    
    /* TODO: Make these constants */
    vec3 worldRight = vec3(1.0, 0.0, 0.0);
    vec3 worldUp = vec3(0.0, 1.0, 0.0);
    vec3 worldForward = vec3(0.0, 0.0, 1.0);

    vec3 right = vec3(1.0, 0.0, 0.0);
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 forward = vec3(0.0, 0.0, 1.0);

    mat4 localToParentTransform = mat4(1);
    mat4 localToParentRotation = mat4(1);
    mat4 localToParentTranslation = mat4(1);
    mat4 localToParentScale = mat4(1);

    mat4 parentToLocalTransform = mat4(1);
    mat4 parentToLocalRotation = mat4(1);
    mat4 parentToLocalTranslation = mat4(1);
    mat4 parentToLocalScale = mat4(1);

    mat4 localToParentMatrix = mat4(1);
    mat4 parentToLocalMatrix = mat4(1);

    // float interpolation = 1.0;

    /* TODO */
	static std::mutex creation_mutex;
    static bool Initialized;

    static Transform transforms[MAX_TRANSFORMS];
    static TransformStruct* pinnedMemory;
    static std::map<std::string, uint32_t> lookupTable;
    static vk::Buffer stagingSSBO;
    static vk::DeviceMemory stagingSSBOMemory;
    static vk::Buffer SSBO;
    static vk::DeviceMemory SSBOMemory;

  public:
    static Transform* Create(std::string name);
    static Transform* Get(std::string name);
    static Transform* Get(uint32_t id);
    static Transform* GetFront();
	static uint32_t GetCount();
    static void Delete(std::string name);
    static void Delete(uint32_t id);

    static void Initialize();
    static bool IsInitialized();
    static void UploadSSBO(vk::CommandBuffer cmmand_buffer);
    static vk::Buffer GetSSBO();
    static uint32_t GetSSBOSize();
    static void CleanUp();

    Transform() { 
        initialized = false;
    }
    
    Transform(std::string name, uint32_t id) {
        initialized = true; this->name = name; this->id = id;
    }

    std::string to_string()
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
        output += "\tlocal_to_parent_matrix: " + glm::to_string(local_to_parent_matrix()) + "\n";
        output += "\tparent_to_local_matrix: " + glm::to_string(parent_to_local_matrix()) + "\n";
        output += "}";
        return output;
    }

    /*
        Transforms direction from local to parent.
        This operation is not affected by scale or position of the transform.
        The returned vector has the same length as the input direction.
    */
    vec3 transform_direction(vec3 direction)
    {

        return vec3(localToParentRotation * vec4(direction, 0.0));
    }

    /*
        Transforms position from local to parent. Note, affected by scale.
        The oposition conversion, from parent to local, can be done with Transform.inverse_transform_point
    */
    vec3 transform_point(vec3 point)
    {
        return vec3(localToParentMatrix * vec4(point, 1.0));
    }

    /*
        Transforms vector from local to parent.
        This is not affected by position of the transform, but is affected by scale.
        The returned vector may have a different length that the input vector.
    */
    vec3 transform_vector(vec3 vector)
    {
        return vec3(localToParentMatrix * vec4(vector, 0.0));
    }

    /*
        Transforms a direction from parent space to local space.
        The opposite of Transform.transform_direction.
        This operation is unaffected by scale.
    */
    vec3 inverse_transform_direction(vec3 direction)
    {
        return vec3(parentToLocalRotation * vec4(direction, 0.0));
    }

    /*
        Transforms position from parent space to local space.
        Essentially the opposite of Transform.transform_point.
        Note, affected by scale.
    */
    vec3 inverse_transform_point(vec3 point)
    {
        return vec3(parentToLocalMatrix * vec4(point, 1.0));
    }

    /*
        Transforms a vector from parent space to local space.
        The opposite of Transform.transform_vector.
        This operation is affected by scale.
    */
    vec3 inverse_transform_vector(vec3 vector)
    {
        return vec3(localToParentMatrix * vec4(vector, 0.0));
    }

    /*
        Rotates the transform so the forward vector points at the target's current position.
        Then it rotates the transform to point its up direction vector in the direction hinted at by the parentUp vector.
        */
    // void look_at(Transform target, vec3 parentUp);

    /*
        Applies a rotation of eulerAngles.z degrees around the z axis, eulerAngles.x degrees around the x axis, and eulerAngles.y degrees around the y axis (in that order).
        If relativeTo is not specified, rotation is relative to local space.
        */
    // void rotate(vec3 eularAngles, Space = Space::Local);

    /*
        Rotates the transform about the provided axis, passing through the provided point in parent coordinates by the provided angle in degrees.
        This modifies both the position and rotation of the transform.
    */
    void rotate_around(vec3 point, float angle, vec3 axis)
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

    void rotate_around(vec3 point, glm::quat rot)
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

    /* Used primarily for non-trivial transformations */
    void set_transform(glm::mat4 transformation, bool decompose = true)
    {
        if (decompose)
        {
            glm::vec3 scale;
            glm::quat rotation;
            glm::vec3 translation;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(transformation, scale, rotation, translation, skew, perspective);
            
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

    quat get_rotation()
    {
        return rotation;
    }

    void set_rotation(quat newRotation)
    {
        rotation = glm::normalize(newRotation);
        update_rotation();
    }

    void set_rotation(float angle, vec3 axis)
    {
        set_rotation(glm::angleAxis(angle, axis));
    }

    void add_rotation(quat additionalRotation)
    {
        set_rotation(get_rotation() * additionalRotation);
        update_rotation();
    }

    void add_rotation(float angle, vec3 axis)
    {
        add_rotation(glm::angleAxis(angle, axis));
    }

    void update_rotation()
    {
        localToParentRotation = glm::toMat4(rotation);
        parentToLocalRotation = glm::inverse(localToParentRotation);
        update_matrix();
    }

    vec3 get_position()
    {
        return position;
    }

    vec3 get_right()
    {
        return right;
    }

    vec3 get_up()
    {
        return up;
    }

    vec3 get_forward()
    {
        return forward;
    }

    void set_position(vec3 newPosition)
    {
        position = newPosition;
        update_position();
    }

    void add_position(vec3 additionalPosition)
    {
        set_position(get_position() + additionalPosition);
        update_position();
    }

    void set_position(float x, float y, float z)
    {
        set_position(glm::vec3(x, y, z));
    }

    void add_position(float dx, float dy, float dz)
    {
        add_position(glm::vec3(dx, dy, dz));
    }

    void update_position()
    {
        localToParentTranslation = glm::translate(glm::mat4(1.0), position);
        parentToLocalTranslation = glm::translate(glm::mat4(1.0), -position);
        update_matrix();
    }

    vec3 get_scale()
    {
        return scale;
    }

    void set_scale(vec3 newScale)
    {
        scale = newScale;
        update_scale();
    }

    void set_scale(float newScale)
    {
        scale = vec3(newScale, newScale, newScale);
        update_scale();
    }

    void add_scale(vec3 additionalScale)
    {
        set_scale(get_scale() + additionalScale);
        update_scale();
    }

    void set_scale(float x, float y, float z)
    {
        set_scale(glm::vec3(x, y, z));
    }

    void add_scale(float dx, float dy, float dz)
    {
        add_scale(glm::vec3(dx, dy, dz));
    }

    void add_scale(float ds)
    {
        add_scale(glm::vec3(ds, ds, ds));
    }

    void update_scale()
    {
        localToParentScale = glm::scale(glm::mat4(1.0), scale);
        parentToLocalScale = glm::scale(glm::mat4(1.0), glm::vec3(1.0 / scale.x, 1.0 / scale.y, 1.0 / scale.z));
        update_matrix();
    }

    void update_matrix()
    {

        localToParentMatrix = (localToParentTransform * localToParentTranslation * localToParentRotation * localToParentScale);
        parentToLocalMatrix = (parentToLocalScale * parentToLocalRotation * parentToLocalTranslation * parentToLocalTransform);

        right = glm::vec3(localToParentMatrix[0]);
        forward = glm::vec3(localToParentMatrix[1]);
        up = glm::vec3(localToParentMatrix[2]);
        position = glm::vec3(localToParentMatrix[3]);
    }

    glm::mat4 parent_to_local_matrix()
    {
        return /*(interpolation >= 1.0 ) ?*/ parentToLocalMatrix /*: glm::interpolate(glm::mat4(1.0), parentToLocalMatrix, interpolation)*/;
    }

    glm::mat4 local_to_parent_matrix()
    {
        return /*(interpolation >= 1.0 ) ?*/ localToParentMatrix /*: glm::interpolate(glm::mat4(1.0), localToParentMatrix, interpolation)*/;
    }

    glm::mat4 local_to_parent_translation()
    {
        return localToParentTranslation;
    }

    glm::mat4 local_to_parent_scale()
    {
        return localToParentScale;
    }

    glm::mat4 local_to_parent_rotation()
    {
        return localToParentRotation;
    }

    glm::mat4 parent_to_local_translation()
    {
        return parentToLocalTranslation;
    }

    glm::mat4 parent_to_local_scale()
    {
        return parentToLocalScale;
    }

    glm::mat4 parent_to_local_rotation()
    {
        return parentToLocalRotation;
    }

    // void set_test_interpolation(float value) {
    //     interpolation = value;
    // }
};
