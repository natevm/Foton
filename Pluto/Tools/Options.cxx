#include <cstdlib>
#include <cstring>
#include <fstream>
#include "./Options.hxx"
#include <iostream>

#include "./whereami.hxx"

namespace Options
{

std::string ResourcePath = "./Resources";
std::string ConnectionFile = "";
std::string PythonArgs = "";

bool ipykernelEnabled = false;
bool isClient = false;
bool isServer = false;
bool resourcePathSet = false;
bool isIPyKernel = false;
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
    std::string path(length, '\0');
    wai_getExecutablePath(path.data(), length, &dirname_length);

    path = path.substr(0, dirname_length);
    
    int i = 1;
    bool stop = false;
    while (i < argc && !stop)
    {
        stop = true;
        if (ProcessArg(i, argv))
        {
            stop = false;
        }
    }

    ResourcePath = path + "/Resources"; 

    
    // /* Process the first filename at the end of the arguments as a model */
    // for (; i < argc; ++i)
    // {
    //     std::string filename(argv[i]);
    //     objLocation = filename;
    // }
    return 0;
}

std::string GetResourcePath() {
    return ResourcePath;
}

bool IsServer()
{
    return isServer;
}

bool IsClient() 
{
    return isClient;
}

bool IsResourcePathSet() 
{
    return resourcePathSet;
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
