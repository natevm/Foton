// ┌───────────────────────────────────────────────────────────────────────────┐
// │  Light Component                                                          │
// │                                                                           │
// │  A light component illuminates objects in a scene. Light components must  │
// │  be added to an entity with a transform component to have a visible       │
// │  impact on the scene.                                                     │
// │                                                                           │
// │  At the moment, light components which cast shadows will have undefined   │
// │  behavior when added to an entity with a camera component. This is due to │
// │  a conflict with the current shadow mapping scheme.                       │
// └───────────────────────────────────────────────────────────────────────────┘

#pragma once

/* System includes */
#include <mutex>

/* Project includes */
#include "Pluto/Tools/StaticFactory.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Light/LightStruct.hxx"

/* Forward declarations */
class Camera;

/* Class declaration */
class Light : public StaticFactory
{
    friend class StaticFactory;
    public:
        /* Creates a light component with default settings */
        static Light* Create(std::string name);

        /* Creates a light component which produces a plausable light color based on standard temperature measurement. */
        static Light* CreateTemperature(std::string name, float kelvin, float intensity);
        
        /* Creates a light component which produces the given color value at the specified intensity. */
        static Light* CreateRGB(std::string name, float r, float g, float b, float intensity);

        /* Retrieves a light component by name */
        static Light* Get(std::string name);

        /* Retrieves a light component by id */
        static Light* Get(uint32_t id);

        /* Returns a pointer to the list of light components */
        static Light* GetFront();

        /* Returns the total number of reserved light components */
        static uint32_t GetCount();

        /* Returns the number of lights stored in the light entities ssbo */
        static uint32_t GetNumActiveLights();

        /* Deallocates a light with the given name */
        static void Delete(std::string name);

        /* Deallocates a light with the given id */
        static void Delete(uint32_t id);

        /* Initializes static resources */
        static void Initialize();

        /* Returns true only if static resources are initialized */
        static bool IsInitialized();

        /* Allocates a set of cameras which can be used to render shadow maps */
        static void CreateShadowCameras();

        /* Releases all static resources */
        static void CleanUp();

        /* Transfers all light components to an SSBO */
        static void UploadSSBO(vk::CommandBuffer command_buffer);

        /* Returns the SSBO vulkan buffer handle pointing to the uploaded components */
        static vk::Buffer GetSSBO();

        /* Returns the size in bytes of the current light component SSBO */
        static uint32_t GetSSBOSize();

        /* Returns the SSBO vulkan buffer handle pointing to a list of entities with light components attached. */
        static vk::Buffer GetLightEntitiesSSBO();
        
        /* Returns the size in bytes of the current light entity SSBO */
        static uint32_t GetLightEntitiesSSBOSize();

        /* Returns a json string summarizing the light */
        std::string to_string();

        /* Sets the color which this light component will emit */
        void set_color(float r, float g, float b);

        /* Sets a realistic emission color via a temperature. */
        void set_temperature(float kelvin);

        /* Sets the intensity, or brightness, that this light component will emit its color */
        void set_intensity(float intensity);
        
        /* In the case of disk and quad area lights, enables or disables light from coming from one or both sides. */
        void set_double_sided(bool double_sided);
        
        /* In the case of rod area lights, enables or disables light from coming from the ends of the rod. */
        void show_end_caps(bool show_end_caps);

        /* Enables casting shadows from the current light */
        void cast_shadows(bool enable_cast_shadows);

        /* Enables casting dynamic shadows (ie shadows which change). */
        void cast_dynamic_shadows(bool enable);

        /* Returns true if a light should cast shadows */
        bool should_cast_shadows();

        /* Returns true if the shadows that a light casts are dynamic (ie change from frame to frame) */
        bool should_cast_dynamic_shadows();

        /* Sets a cone angle, which creates a spot light like effect */
        void set_cone_angle(float angle);

        /* Sets the transition rate of a spot light shadow */
        void set_cone_softness(float softness);

        /* When using shadow maps, specifies how many samples should be taken to approximate a softness */
        void set_shadow_softness_samples(uint32_t samples);

        /* When using shadow maps, specifies a radius to blur a shadow map to approximate soft shadows */
        void set_shadow_softness_radius(float radius);

        /* Make the light act as a point light */
        void use_point();

        /* Make the light act as a plane (or rectangle) area light */
        void use_plane();

        /* Make the light act as a disk area light */
        void use_disk();

        /* Make the light act as a rod area light */
        void use_rod();

        /* Make the light act as a sphere area light */
        void use_sphere();

        /* Toggles the light on or off */
        void disable(bool disabled);

        /* Enables variance shadow mapping, which blurs the shadow map before sampling, 
        creating fake soft shadows. Note: may result in "peter-panning" artifacts */
        void enable_vsm(bool enabled);

        /* Returns true if variance shadow mapping is enabled. Useful when recording compute pass on shadow maps. */
        bool should_use_vsm();

    private:
        /* Creates an uninitialized light. Useful for preallocation. */
        Light();
        
        /* Creates a light with the given name and id */
        Light(std::string name, uint32_t id);

        /* A mutex used to make component access and modification thread safe */
        static std::shared_ptr<std::mutex> creation_mutex;

        /* Flag indicating that static resources were created */
        static bool Initialized;

        /* A list of light components, allocated statically */
        static Light lights[MAX_LIGHTS];

/* Remove this? */
        static LightStruct* pinnedMemory;

        /* A lookup table of name to light id */
        static std::map<std::string, uint32_t> lookupTable;

        /* A vulkan buffer handle corresponding to the light SSBO */
        static vk::Buffer SSBO;

        /* The corresponding light SSBO memory */
        static vk::DeviceMemory SSBOMemory;

        /* A vulkan buffer handle corresponding to a staging buffer for the light SSBO */
        static vk::Buffer stagingSSBO;
        
        /* The corresponding light staging SSBO memory */
        static vk::DeviceMemory stagingSSBOMemory;
        
        /* A vulkan buffer handle corresponding to the light entities SSBO */
        static vk::Buffer LightEntitiesSSBO;

        /* The corresponding light entities SSBO memory */
        static vk::DeviceMemory LightEntitiesSSBOMemory;

        /* A vulkan buffer handle corresponding to a staging buffer for the light entities SSBO */
        static vk::Buffer stagingLightEntitiesSSBO;
        
        /* The corresponding light entities staging SSBO memory */
        static vk::DeviceMemory stagingLightEntitiesSSBOMemory;

        /* The number of entities in the light entities ssbo */
        static uint32_t numLightEntities;

/* Remove this? */
        static std::vector<Camera*> shadowCameras;
        
        /* The structure containing all shader light properties. This is what's copied into the SSBO per instance */
        LightStruct light_struct;

/* This should probably be in the light struct.. A maximum distance that the light can cast */
        float max_distance;

        /* A flag indicating whether the current light should cast shadows or not */
        bool castsDynamicShadows = true;

        /* Flag to enable variance shadow mapping effect */
        bool enableVSM = false;
};
