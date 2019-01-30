#include <cstdlib>
#include <cstring>
#include <fstream>
#include "./Options.hxx"
#include <iostream>
#include <sys/stat.h>
#include <exception>

#include "./whereami.hxx"

namespace Options
{

std::string ResourcePath = "";
std::string MainModule = "";
std::string ConnectionFile = "";
std::string PythonArgs = "";

bool ipykernelEnabled = false;
bool isClient = false;
bool isServer = false;
bool isIPyKernel = false;
bool isMainModuleSet = false;
std::string ip = "";

#define $(flag) (strcmp(argv[i], flag) == 0)
bool ProcessArg(int &i, char **argv)
{
    int orig_i = i;
    
    if $("-ipykernel")
    {
        ++i;
        ConnectionFile = std::string(argv[i]);
        std::cout<<"Activating ipykernel mode. Connection file is "<< ConnectionFile <<std::endl;
        ++i;
        isIPyKernel = true;
    }
    else if $("-server") {
        ++i;
        isServer = true;
        ip = std::string(argv[i]);
        ++i;
        std::cout<<"Activating server mode. " <<std::endl;
    }
    else if $("-client") {
        ++i;
        isClient = true;
        ip = std::string(argv[i]);
        ++i;
        std::cout<<"Activating client mode. " <<std::endl;
    }

    return i != orig_i;
}

int ProcessArgs(int argc, char **argv)
{
    using namespace Options;
    
    int dirname_length;
    int length = wai_getExecutablePath(NULL, 0, NULL);
    std::string executable_path(length, '\0');
    wai_getExecutablePath(executable_path.data(), length, &dirname_length);

    executable_path = executable_path.substr(0, dirname_length);
    ResourcePath = executable_path + "/Resources"; 
    
    int i = 1;
    bool stop = false;
    while (i < argc && !stop) {
        stop = true;
        if (ProcessArg(i, argv)) {
            stop = false;
        }
    }

    /* Process the first filename at the end of the arguments as a module to run */
    for (; i < argc; ++i) {
        std::string filename(argv[i]);
        
        /* Make sure that file exists */
        struct stat st;
        if (stat(filename.c_str(), &st) != 0)
            throw std::runtime_error( std::string(filename + " does not exist!"));

        MainModule = filename;
        isMainModuleSet = true;
    }
    return 0;
}

std::string GetResourcePath() {
    return ResourcePath;
}

std::string GetMainModule() {
    return MainModule;
}

bool IsServer()
{
    return isServer;
}

bool IsClient() 
{
    return isClient;
}

bool IsMainModuleSet() 
{
    return isMainModuleSet;
}


bool IsIPyKernel() 
{
    return isIPyKernel;
}

std::string GetConnectionFile()
{
    return ConnectionFile;
}


std::string GetIP() {
    return ip;
}

} // namespace Options
