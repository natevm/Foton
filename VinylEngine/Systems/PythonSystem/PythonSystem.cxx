#include "PythonSystem.hxx"
#include <iostream>
#include <cstdlib>

namespace Systems {
    PythonSystem::PythonSystem() {
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

    wchar_t *GetWC(char *c)
    {
        const size_t cSize = strlen(c)+1;
        wchar_t* wc = new wchar_t[cSize];
        mbstowcs (wc, c, cSize);

        return wc;
    }

    bool PythonSystem::start(int argc, char** argv) {
        std::cout<<"Size of wchar_t"<< sizeof(wchar_t)<<std::endl;
        if (!initialized) return false;
        if (running) return false;

        std::vector<wchar_t*> new_argv(argc);
        for (int i = 0; i < argc; ++i) {
            new_argv[i] = GetWC(argv[i]);
        }
        auto wargv = new_argv.data();
            // PyRun_SimpleString("from ipykernel import kernelapp as app");
            // PyRun_SimpleString("app.launch_new_instance()");
        auto loop = [argc, wargv](future<void> futureObj) {
            /* Construct wchar_t array */

            std::cout<<"START"<<std::endl;
            Py_Main(argc, wargv);
            // wchar_t * flag = (wchar_t*)"-m";
            // wchar_t * mod = (wchar_t*)"ipykernel_launcher";
            // auto argv_temp = (wchar_t**) malloc(sizeof(wchar_t*) * 2);
            // argv_temp[0] = flag;
            // argv_temp[1] = mod;
            wchar_t * name = (wchar_t*)"PythonSystem";
            // PyRun_SimpleString("from ipykernel.ipkernel import IPythonKernel");
            // PyRun_SimpleString("VinylKernel = IPythonKernel(['-f', '" + connection_file + "'])");
            // PySys_SetArgv(argc, new_argv.data());
            // PyRun_SimpleString("import sys, Vinyl");
            // PyRun_SimpleString("glfw = Vinyl.Libraries.GLFW.Get()");
            // PyRun_SimpleString("vulkan = Vinyl.Libraries.Vulkan.Get()");
            
            // Py_Main(argc, argv);
            // PyRun_SimpleString("import simple_kernel");
            
            // FILE *fd = fopen("simple_kernel.py", "r");
            // PyRun_SimpleFileEx(fd, "simple_kernel.py", 1);



            std::cout<<"STOP"<<std::endl;
            // free(argv_temp);
        };

        exitSignal = promise<void>();
        future<void> futureObj = exitSignal.get_future();
        eventThread = thread(loop, move(futureObj));

        running = true;
        return true;
    }

    bool PythonSystem::stop() {
        if (!initialized) return false;
        if (!running) return false;

        PyRun_SimpleString("sys.exit()");

        running = false;
        return true;
    }   
}
