#include "PythonSystem.hxx"
#include "Pluto/Systems/EventSystem/EventSystem.hxx"
#include "Pluto/Tools/Options.hxx"
#include <iostream>
#include <cstdlib>

#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <stdio.h>

#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>

#include "Pluto/Tools/whereami.hxx"

namespace Systems
{
PythonSystem::PythonSystem()
{
}
PythonSystem::~PythonSystem()
{
}

PythonSystem *PythonSystem::Get()
{
    static PythonSystem instance;
    return &instance;
}

bool PythonSystem::initialize()
{
    initialized = true;
    return true;
}

wchar_t *GetWC(const char *c)
{
    const size_t cSize = strlen(c) + 1;
    wchar_t *wc = new wchar_t[cSize];
    mbstowcs(wc, c, cSize);

    return wc;
}

bool PythonSystem::start()
{
    if (!initialized)
        return false;
    if (running)
        return false;

    auto loop = [this]() {
        // std::cout<<"Starting PythonSystem, thread id: " << std::this_thread::get_id()<<std::endl;

        const wchar_t *name = L"Pluto";
        Py_SetProgramName(const_cast<wchar_t *>(name));
        Py_Initialize();

        Py_InspectFlag = 1;
        Py_InteractiveFlag = 1;

        int dirname_length;
        int length = wai_getExecutablePath(NULL, 0, NULL);
        std::string path(length, '\0');
        wai_getExecutablePath(path.data(), length, &dirname_length);
        path = path.substr(0, dirname_length);

        /* Construct wchar_t array */
        using namespace std;

        if (Options::IsIPyKernel())
        {
#ifdef NDEBUG
            // nondebug
            std::vector<wchar_t *> argv(3);
            argv[0] = L"Pluto";
            argv[1] = L"-f";
            std::string connectionFile = Options::GetConnectionFile();
            argv[2] = GetWC(connectionFile.c_str());
            PySys_SetArgv(3, argv.data());
            PyRun_SimpleString("import sys");
            PyRun_SimpleString(std::string(std::string("sys.path.insert(0, \"") + path + std::string("\")")).c_str());
            PyRun_SimpleString("from ipykernel import kernelapp as app");
            PyRun_SimpleString("app.launch_new_instance()");
#else
            // debug code
            std::cout << "Pluto Error: ipykernel mode does not work when Pluto is built using debug mode. This is due to a bug with pyzmq. See the following issue for details: https://github.com/zeromq/pyzmq/issues/1245 " << std::endl;
#endif
        }
        else
        {
            PyRun_SimpleString("import sys");
            PyRun_SimpleString(std::string(std::string("sys.path.insert(0, \"") + path + std::string("\")")).c_str());

            if (Options::IsMainModuleSet())
            {
                auto fp = fopen(Options::GetMainModule().c_str(), "r");
                PyRun_AnyFile(fp, Options::GetMainModule().c_str() );
            }
            else
                PyRun_InteractiveLoop(stdin, "<stdin>");
        }

        Py_Finalize();

        Systems::EventSystem::Get()->stop();
    };

    running = true;
    eventThread = thread(loop);

    return true;
}

bool PythonSystem::stop()
{
    if (!initialized)
        return false;
    if (!running)
        return false;
    running = false;
    eventThread.join();
    return true;
}

bool PythonSystem::should_close()
{
    return true;
}
} // namespace Systems
