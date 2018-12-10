#include "PythonSystem.hxx"
#include "Pluto/Systems/EventSystem/EventSystem.hxx"
#include "Pluto/Tools/Options.hxx"
#include <iostream>
#include <cstdlib>

#pragma optimize("", off)

#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <stdio.h>

#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace Systems {
    PythonSystem::PythonSystem() {
        wchar_t *name = L"Pluto";
        Py_SetProgramName(name);
        Py_Initialize();
    }
    PythonSystem::~PythonSystem() {
        Py_Finalize();
    }

    PythonSystem* PythonSystem::Get() {
        static PythonSystem instance;
        return &instance;
    }

    bool PythonSystem::initialize() {
        initialized = true;
        return true;
    }

    wchar_t *GetWC(const char *c)
    {
        const size_t cSize = strlen(c)+1;
        wchar_t* wc = new wchar_t[cSize];
        mbstowcs (wc, c, cSize);

        return wc;
    }

    bool PythonSystem::start() {
        if (!initialized) return false;
        if (running) return false;

        auto loop = [this]() {
            std::cout<<"Starting PythonSystem, thread id: " << std::this_thread::get_id()<<std::endl;

            Py_InspectFlag = 1;
            Py_InteractiveFlag = 1;

            /* Construct wchar_t array */
            using namespace std;

        //     std::string sArgs = std::string("Pluto.exe ") + Options::GetPythonArgs();
        //    // istringstream iss(sArgs);

        //     std::istringstream iss(sArgs);
        //     std::vector<std::string> tokens;
        //     std::string s;

        //     while (iss >> std::quoted(s)) {
        //         tokens.push_back(s);
        //     }

        //     for(auto& str: tokens)
        //         std::cout << str << std::endl;

            // vector<string> tokens{istream_iterator<string>{iss}, istream_iterator<string>{}};
            // int argc = (int)tokens.size();
            // std::vector<wchar_t*> new_argv(argc);
            // for (int i = 0; i < argc; ++i) {
            //     new_argv[i] = GetWC(tokens[i].c_str());
            // }
            // auto wargv = new_argv.data();
            
            // PySys_SetArgv(argc, wargv);

            if (Options::IsIPyKernel())
            {
                std::vector<wchar_t*> argv(2);
                argv[0] = L"Pluto";
                argv[1] = L"-f";
                std::string connectionFile = Options::GetConnectionFile();
                argv[2] = GetWC(connectionFile.c_str());
                PySys_SetArgv(3, argv.data());
                PyRun_SimpleString("import sys");
                PyRun_SimpleString("if sys.path[0] == '': del sys.path[0]\n\n");
                PyRun_SimpleString("from ipykernel import kernelapp as app");
                PyRun_SimpleString("app.launch_new_instance()");
            }
            else {
              PyRun_InteractiveLoop(stdin, "<stdin>");
            }

            wchar_t * name = (wchar_t*)"PythonSystem";

            Systems::EventSystem::Get()->stop();
        };

        running = true;
        eventThread = thread(loop);

        return true;
    }

    bool PythonSystem::stop() {
        if (!initialized) return false;
        if (!running) return false;
        running = false;
        eventThread.join();
        return true;
    } 
    
    bool PythonSystem::should_close()
    {
        return true;
    } 
}

