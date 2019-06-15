#pragma once

#include <mutex>

#include "Pluto/Tools/StaticFactory.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Light/LightStruct.hxx"

class Camera;

class Light : public StaticFactory
{
    friend class StaticFactory;
    private:

        /* TODO */
		static std::mutex creation_mutex;
        static bool Initialized;

        /* Factory fields */
        static Light lights[MAX_LIGHTS];
        static LightStruct* pinnedMemory;
        static std::map<std::string, uint32_t> lookupTable;
        static vk::Buffer SSBO;
        static vk::DeviceMemory SSBOMemory;
        static vk::Buffer stagingSSBO;
        static vk::DeviceMemory stagingSSBOMemory;
        static std::vector<Camera*> shadowCameras;
        
        /* Instance fields*/
        LightStruct light_struct;

        Light();
        Light(std::string name, uint32_t id);

        float max_distance;
        bool castsDynamicShadows = true;

    public:
        
        /* Factory functions */
        static Light* Create(std::string name);
        static Light* Get(std::string name);
        static Light* Get(uint32_t id);
        static Light* GetFront();
	    static uint32_t GetCount();
        static void Delete(std::string name);
        static void Delete(uint32_t id);
    
        static void Initialize();
        static bool IsInitialized();
        static void CreateShadowCameras();
        static void UploadSSBO(vk::CommandBuffer command_buffer);
        static vk::Buffer GetSSBO();
        static uint32_t GetSSBOSize();
        static void CleanUp();


        /* Instance functions */

        void set_color(float r, float g, float b);
        void set_temperature(float kelvin);        
        void set_intensity(float intensity);
        void set_double_sided(bool double_sided);
        void show_end_caps(bool show_end_caps);
        void cast_shadows(bool enable_cast_shadows);
        void cast_dynamic_shadows(bool enable);
        bool should_cast_shadows();
        bool should_cast_dynamic_shadows();
        void set_cone_angle(float angle);
        void set_cone_softness(float softness);
        void set_shadow_softness_samples(uint32_t samples);
        void set_shadow_softness_radius(float radius);

        void use_point();
        void use_plane();
        void use_disk();
        void use_rod();
        void use_sphere();

        std::string to_string();
};
