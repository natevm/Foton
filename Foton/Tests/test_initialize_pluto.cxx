#include <iostream>
#include <thread>
#include <future>

#include "gtest/gtest.h"

#include "Foton/Foton.hxx"

TEST(start_stop_systems, initialization)
{
    auto func = []() {
        try {
            /* An asynchronous thread requests systems to stop */
            Foton::StopSystems();
        }
        catch (std::runtime_error &e) {
            FAIL() << "Runtime Error " << e.what();
        }
    };

    // Must be on the main thread, runs windowing events
    Foton::StartSystems(false, func);
    Foton::WaitForStartupCallback();
}

TEST(default_initialize, initialization)
{
    auto func = []() {
        try {
            /* An asynchronous thread requests systems to stop */
            Foton::Initialize(true, false, {"VK_LAYER_LUNARG_standard_validation"});
            Foton::StopSystems();
        }
        catch (std::runtime_error &e) {
            Foton::StopSystems();
            FAIL() << "Runtime Error " << e.what();
        }
    };

    // Must be on the main thread, runs windowing events
    Foton::StartSystems(false, func);
    Foton::WaitForStartupCallback();
}