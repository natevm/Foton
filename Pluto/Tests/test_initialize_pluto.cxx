#include <iostream>
#include <thread>
#include <future>

#include "gtest/gtest.h"

#include "Pluto/Pluto.hxx"

TEST(start_stop_systems, initialization)
{
    auto func = []() {
        try {
            /* An asynchronous thread requests systems to stop */
            Pluto::StopSystems();
        }
        catch (std::runtime_error &e) {
            FAIL() << "Runtime Error " << e.what();
        }
    };

    // Must be on the main thread, runs windowing events
    Pluto::StartSystems(false, func);
    Pluto::WaitForStartupCallback();
}

TEST(default_initialize, initialization)
{
    auto func = []() {
        try {
            /* An asynchronous thread requests systems to stop */
            Pluto::Initialize();
            Pluto::StopSystems();
        }
        catch (std::runtime_error &e) {
            Pluto::StopSystems();
            FAIL() << "Runtime Error " << e.what();
        }
    };

    // Must be on the main thread, runs windowing events
    Pluto::StartSystems(false, func);
    Pluto::WaitForStartupCallback();
}