#pragma once
#include <openvr.h>
#include <thread>
#include <future>
#include <vector>
#include <set>
#include <condition_variable>

#include "Pluto/Tools/Singleton.hxx"
// #include "threadsafe_queue.hpp"

namespace Libraries {
    using namespace std;
    class OpenVR : public Singleton
    {
        public:
            static OpenVR* Get();

            bool get_required_vulkan_instance_extensions(std::vector<std::string> &outInstanceExtensionList);
        
        private:
            OpenVR();
            ~OpenVR();
    };
}