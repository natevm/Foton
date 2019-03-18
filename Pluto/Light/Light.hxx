#pragma once

#include <mutex>

#include "Pluto/Tools/StaticFactory.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Light/LightStruct.hxx"

class Camera;

class Light : public StaticFactory
{
    private:
        /* Factory fields */
        static Light lights[MAX_LIGHTS];
        static LightStruct* pinnedMemory;
        static std::map<std::string, uint32_t> lookupTable;
        static vk::Buffer SSBO;
        static vk::DeviceMemory SSBOMemory;
        static vk::Buffer stagingSSBO;
        static vk::DeviceMemory stagingSSBOMemory;
        static std::vector<Camera*> shadowCameras;
        /* TODO */
		static std::mutex creation_mutex;
        
        /* Instance fields*/
        LightStruct light_struct;

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
        static void CreateShadowCameras();
        static void UploadSSBO(vk::CommandBuffer command_buffer);
        static vk::Buffer GetSSBO();
        static uint32_t GetSSBOSize();
        static void CleanUp();

        /* Instance functions */

        Light();

        Light(std::string name, uint32_t id);

        void set_color(float r, float g, float b);
        void set_temperature(float kelvin);        
        void set_intensity(float intensity);
        void set_double_sided(bool double_sided);
        void show_end_caps(bool show_end_caps);
        void cast_shadows(bool enable_cast_shadows);
        bool should_cast_shadows();
        void set_cone_angle(float angle);
        void set_cone_softness(float softness);

        void use_point();
        void use_plane();
        void use_disk();
        void use_rod();
        void use_sphere();

        std::string to_string();
};
