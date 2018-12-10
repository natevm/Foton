#pragma once
#include <Python.h>

#include "Pluto/Tools/System.hxx"

namespace Systems {
    class PythonSystem : public System {
        public:
            static PythonSystem* Get();
            bool initialize();
            bool start();
            bool stop();
            bool should_close();
        private:
            PythonSystem();
            ~PythonSystem();
    };
}
