#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED


#include <glm/glm.hpp>
#include <glm/common.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <memory>
#include <typeindex>

class StaticFactory {
    public:

    virtual std::string to_string() = 0;
    virtual std::string get_name() { return name; };
    virtual uint32_t get_id() { return id; };
    
    bool is_initialized() {
        return initialized;
    }
    
    /* Returns whether or not a key exists in the lookup table. */
    static bool DoesItemExist(std::map<std::string, uint32_t> &lookupTable, std::string name)
    {
        auto it = lookupTable.find(name);
        return (it != lookupTable.end());
    }

    /* Returns the first index where an item of type T is uninitialized. */
    template<class T>
    static int32_t FindAvailableID(T *items, uint32_t max_items) 
    {
        for (uint32_t i = 0; i < max_items; ++i)
            if (items[i].initialized == false)
                return (int32_t)i;
        return -1;
    }
    
    /* Reserves a location in items and adds an entry in the lookup table */
    template<class T>
    static T* Create(std::string name, std::string type, std::map<std::string, uint32_t> &lookupTable, T* items, uint32_t maxItems) 
    {
        if (DoesItemExist(lookupTable, name)) {
            std::cout << "Error, " << type<< " \"" << name << "\" already exists." << std::endl;
            auto id = lookupTable[name];
            return &items[id];
        }

        int32_t id = FindAvailableID(items, maxItems);

        if (id < 0) {
            std::cout << "Error, max " << type << " limit reached." << std::endl;
            return nullptr;
        }

        std::cout << "Adding " << type << " \"" << name << "\"" << std::endl;
        items[id] = T(name, id);
        lookupTable[name] = id;
        return &items[id];
    }

    /* Retrieves an element with a lookup table indirection */
    template<class T>
    static T* Get(std::string name, std::string type, std::map<std::string, uint32_t> &lookupTable, T* items, uint32_t maxItems) 
    {
        if (DoesItemExist(lookupTable, name)) {
            uint32_t id = lookupTable[name];
            return &items[id];
        }
        std::cout << "Error, " << type << " \"" << name << "\" does not exist." << std::endl;
        return nullptr;
    }

    /* Retrieves an element by ID directly */
    template<class T>
    static T* Get(uint32_t id, std::string type, std::map<std::string, uint32_t> &lookupTable, T* items, uint32_t maxItems) 
    {
        if (id >= maxItems) {
            std::cout<<"Error, id greater than max " << type << std::endl;
            return nullptr;
        }

        else if (!items[id].initialized) {
            std::cout<<"Error, entity with id " << id << " does not exist"<<std::endl; 
            return nullptr;
        }
    
        return &items[id];
    }

    /* Removes an element with a lookup table indirection, removing from both items and the lookup table */
    template<class T>
    static bool Delete(std::string name, std::string type, std::map<std::string, uint32_t> &lookupTable, T* items, uint32_t maxItems)
    {
        if (!DoesItemExist(lookupTable, name)) {
            std::cout << "Error, " << type << " \"" << name << "\" does not exist." << std::endl;
            return false;
        }

        items[lookupTable[name]] = T();
        lookupTable.erase(name);
        return true;
    }

    /* Removes an element by ID directly, removing from both items and the lookup table */
    template<class T>
    static bool Delete(uint32_t id, std::string type, std::map<std::string, uint32_t> &lookupTable, T* items, uint32_t maxItems)
    {
        if (id >= maxItems) {
            std::cout<<"Error, id greater than max " << type <<std::endl;
            return false;
        }

        if (!items[id].initialized) {
            std::cout<<"Error, " << type << " with id " << id << " does not exist" << std::endl;
            return false;
        }

        lookupTable.erase(items[id].name);
        items[id] = T();
        return true;
    }

    // static void CreateSSBO();
    // static void UploadSSBO();
    // static void FreeSSBO();
    // static vk::Buffer GetSSBO();
    
    protected:

    bool initialized = false;
    std::string name = "";
    uint32_t id = -1;
    std::set<uint32_t> entities;
};