#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>

namespace Options
{
    bool ProcessArg(int &i, char **argv);
    int ProcessArgs(int argc, char **argv);

    std::string GetResourcePath();
    std::string GetConnectionFile();
    bool IsServer();
    bool IsClient();
    std::string GetIP();
    bool IsResourcePathSet();
    bool IsIPyKernel();


}; // namespace Options