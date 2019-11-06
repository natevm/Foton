#pragma once

#include <thread>
#include <future>
#include <vector>

#include "Foton/Tools/Singleton.hxx"

namespace Systems {
    using namespace std;
    class System : public Singleton 
    {
    public:
        bool running = false;
        virtual bool start() {return false;}
        virtual bool stop() {return false;}
    protected:
        System() {}
        ~System() {}
        System(const System&) = delete;
        System& operator=(const System&) = delete;
        System(System&&) = delete;
        System& operator=(System&&) = delete;
        
        std::promise<void> exitSignal;
        std::thread eventThread;
    };
}
