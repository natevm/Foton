#pragma once

#include "Pluto/Tools/StaticFactory.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Light/LightStruct.hxx"

class Light : public StaticFactory
{
    private:
        /* Factory fields */
        static Light lights[MAX_LIGHTS];
        static LightStruct* pinnedMemory;
        static std::map<std::string, uint32_t> lookupTable;
        static vk::Buffer ssbo;
        static vk::DeviceMemory ssboMemory;
        
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
        static void UploadSSBO();
        static vk::Buffer GetSSBO();
        static uint32_t GetSSBOSize();
        static void CleanUp();

        /* Instance functions */

        Light();

        Light(std::string name, uint32_t id);

        void set_color(float r, float g, float b);

        std::string to_string();
};
