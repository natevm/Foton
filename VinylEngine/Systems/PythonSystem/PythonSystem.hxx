#pragma once
#include <Python.h>

#include "VinylEngine/BaseClasses/System.hxx"

namespace Systems {
    class PythonSystem : public System {
        public:
            static PythonSystem* Get();
            bool initialize();
            bool start(int argc, char** argv);
            bool stop();
        private:
            PythonSystem();
            ~PythonSystem();
    };
}
