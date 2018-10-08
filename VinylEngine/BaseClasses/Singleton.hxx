#pragma once

#include <thread>
#include <future>
#include <vector>

class Singleton {
public:
    virtual bool initialize() {
        initialized = true;
        return true;
    }

    bool is_initialized() const {
        return initialized;
    }

protected:
    bool initialized = false;
    Singleton() {};
    ~Singleton() {};    
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;
};