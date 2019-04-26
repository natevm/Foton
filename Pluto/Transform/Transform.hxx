#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#define GLM_FORCE_RIGHT_HANDED

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
    /* Scene graph information */
    int32_t parent = -1;
	std::set<int32_t> children;

    /* Local <=> Parent */
    vec3 scale = vec3(1.0);
    vec3 position = vec3(0.0);
    quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

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

    /* Local <=> World */
    mat4 localToWorldMatrix = mat4(1);
    mat4 worldToLocalMatrix = mat4(1);

    // from local to world decomposition. 
    // May only approximate the localToWorldMatrix
    glm::vec3 worldScale;
    glm::quat worldRotation;
    glm::vec3 worldTranslation;
    glm::vec3 worldSkew;
    glm::vec4 worldPerspective;
    
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

    /* Updates cached rotation values */
    void update_rotation();

    /* Updates cached position values */
    void update_position();

    /* Updates cached scale values */
    void update_scale();

    /* Updates cached final local to parent matrix values */
    void update_matrix();

    /* Updates cached final local to world matrix values */
    void update_world_matrix();

    /* updates all childrens cached final local to world matrix values */
    void update_children();
    
    /* traverses from the current transform up through its ancestors, 
    computing a final world to local matrix */
    glm::mat4 compute_world_to_local_matrix();

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

    Transform();
    Transform(std::string name, uint32_t id);
    std::string to_string();

    /*
        Transforms direction from local to parent.
        This operation is not affected by scale or position of the transform.
        The returned vector has the same length as the input direction.
    */
    vec3 transform_direction(vec3 direction);

    /*
        Transforms position from local to parent. Note, affected by scale.
        The oposition conversion, from parent to local, can be done with Transform.inverse_transform_point
    */
    vec3 transform_point(vec3 point);

    /*
        Transforms vector from local to parent.
        This is not affected by position of the transform, but is affected by scale.
        The returned vector may have a different length that the input vector.
    */
    vec3 transform_vector(vec3 vector);

    /*
        Transforms a direction from parent space to local space.
        The opposite of Transform.transform_direction.
        This operation is unaffected by scale.
    */
    vec3 inverse_transform_direction(vec3 direction);

    /*
        Transforms position from parent space to local space.
        Essentially the opposite of Transform.transform_point.
        Note, affected by scale.
    */
    vec3 inverse_transform_point(vec3 point);

    /*
        Transforms a vector from parent space to local space.
        The opposite of Transform.transform_vector.
        This operation is affected by scale.
    */
    vec3 inverse_transform_vector(vec3 vector);

    /*
        Rotates the transform so the forward vector points at the target's current position.
        Then it rotates the transform to point its up direction vector in the direction hinted at 
        by the parentUp vector.
        */
    // void look_at(Transform target, vec3 parentUp);

    /*
        Applies a rotation of eulerAngles.z degrees around the z axis, eulerAngles.x degrees around 
        the x axis, and eulerAngles.y degrees around the y axis (in that order).
        If relativeTo is not specified, rotation is relative to local space.
        */
    // void rotate(vec3 eularAngles, Space = Space::Local);

    /*
        Rotates the transform about the provided axis, passing through the provided point in parent 
        coordinates by the provided angle in degrees.
        This modifies both the position and rotation of the transform.
    */
    void rotate_around(vec3 point, float angle, vec3 axis);

    /* TODO: Explain this */
    void rotate_around(vec3 point, glm::quat rot);

    /* Used primarily for non-trivial transformations */
    void set_transform(glm::mat4 transformation, bool decompose = true);

    /* Returns a quaternion rotating the transform from local to parent */
    quat get_rotation();

    /* Sets the rotation of the transform from local to parent via a quaternion */
    void set_rotation(quat newRotation);

    /* Sets the rotation of the transform from local to parent using an axis 
       in local space to rotate about, and an angle in radians to drive the rotation */
    void set_rotation(float angle, vec3 axis);

    /* Adds a rotation to the existing transform rotation from local to parent 
       via a quaternion */
    void add_rotation(quat additionalRotation);

    /* Adds a rotation to the existing transform rotation from local to parent 
       using an axis in local space to rotate about, and an angle in radians to 
       drive the rotation */
    void add_rotation(float angle, vec3 axis);

    /* Returns a position vector describing where this transform will be translated to in its parent space. */
    vec3 get_position();

    /* Returns a vector pointing right relative to the current transform placed in its parent's space. */
    vec3 get_right();

    /* Returns a vector pointing up relative to the current transform placed in its parent's space. */
    vec3 get_up();

    /* Returns a vector pointing forward relative to the current transform placed in its parent's space. */
    vec3 get_forward();

    /* Sets the position vector describing where this transform should be translated to when placed in its 
    parent space. */
    void set_position(vec3 newPosition);

    /* Adds to the current the position vector describing where this transform should be translated to 
    when placed in its parent space. */
    void add_position(vec3 additionalPosition);

    /* Sets the position vector describing where this transform should be translated to when placed in its 
    parent space. */
    void set_position(float x, float y, float z);

    /* Adds to the current the position vector describing where this transform should be translated to 
    when placed in its parent space. */
    void add_position(float dx, float dy, float dz);

    /* Returns the scale of this transform from local to parent space along its right, up, and forward 
    directions respectively */
    vec3 get_scale();

    /* Sets the scale of this transform from local to parent space along its right, up, and forward 
    directions respectively */
    void set_scale(vec3 newScale);

    /* Sets the scale of this transform from local to parent space along its right, up, and forward 
    directions simultaneously */
    void set_scale(float newScale);

    /* Adds to the current the scale of this transform from local to parent space along its right, up, 
    and forward directions respectively */
    void add_scale(vec3 additionalScale);

    /* Sets the scale of this transform from local to parent space along its right, up, and forward 
    directions respectively */
    void set_scale(float x, float y, float z);

    /* Adds to the current the scale of this transform from local to parent space along its right, up, 
    and forward directions respectively */
    void add_scale(float dx, float dy, float dz);

    /* Adds to the scale of this transform from local to parent space along its right, up, and forward 
    directions simultaneously */
    void add_scale(float ds);

    /* Returns the final matrix transforming this object from it's parent coordinate space to it's 
    local coordinate space */
    glm::mat4 get_parent_to_local_matrix();

    /* Returns the final matrix transforming this object from it's local coordinate space to it's 
    parents coordinate space */
    glm::mat4 get_local_to_parent_matrix();

    /* Returns the final matrix translating this object from it's local coordinate space to it's 
    parent coordinate space */
    glm::mat4 get_local_to_parent_translation_matrix();

    /* Returns the final matrix translating this object from it's local coordinate space to it's 
    parent coordinate space */
    glm::mat4 get_local_to_parent_scale_matrix();

    /* Returns the final matrix rotating this object in it's local coordinate space to it's 
    parent coordinate space */
    glm::mat4 get_local_to_parent_rotation_matrix();

    /* Returns the final matrix translating this object from it's parent coordinate space to it's 
    local coordinate space */
    glm::mat4 get_parent_to_local_translation_matrix();

    /* Returns the final matrix scaling this object from it's parent coordinate space to it's 
    local coordinate space */
    glm::mat4 get_parent_to_local_scale_matrix();

    /* Returns the final matrix rotating this object from it's parent coordinate space to it's 
    local coordinate space */
    glm::mat4 get_parent_to_local_rotation_matrix();

    /* Set the parent of this transform, whose transformation will be applied after the current
    transform. */
    void set_parent(uint32_t parent);

    /* Removes the parent-child relationship affecting this node. */
    void clear_parent();

    /* Add a child to this transform, whose transformation will be applied before the current
    transform. */
	void add_child(uint32_t object);

    /* Removes a child transform previously added to the current transform. */
	void remove_child(uint32_t object);

    /* Returns a matrix transforming this component from world space to its local space, taking all 
    parent transforms into account. */
    glm::mat4 get_world_to_local_matrix();

    /* Returns a matrix transforming this component from its local space to world space, taking all 
    parent transforms into account. */
	glm::mat4 get_local_to_world_matrix();

    /* Returns a (possibly approximate) rotation rotating the current transform from 
    local space to world space, taking all parent transforms into account */
	glm::quat get_world_rotation();

    /* Returns a (possibly approximate) translation moving the current transform from 
    local space to world space, taking all parent transforms into account */
    glm::vec3 get_world_translation();

    /* Returns a (possibly approximate) rotation matrix rotating the current transform from 
    local space to world space, taking all parent transforms into account */
    glm::mat4 get_world_to_local_rotation_matrix();

    /* Returns a (possibly approximate) rotation matrix rotating the current transform from 
    world space to local space, taking all parent transforms into account */
    glm::mat4 get_local_to_world_rotation_matrix();

    /* Returns a (possibly approximate) translation matrix translating the current transform from 
    local space to world space, taking all parent transforms into account */
    glm::mat4 get_world_to_local_translation_matrix();

    /* Returns a (possibly approximate) translation matrix rotating the current transform from 
    world space to local space */
    glm::mat4 get_local_to_world_translation_matrix();

    /* Returns a (possibly approximate) scale matrix scaling the current transform from 
    local space to world space, taking all parent transforms into account */
    glm::mat4 get_world_to_local_scale_matrix();

    /* Returns a (possibly approximate) scale matrix scaling the current transform from 
    world space to local space, taking all parent transforms into account */
    glm::mat4 get_local_to_world_scale_matrix();
};
