#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>

#include <thread>
#include <iostream>
#include <assert.h>
#include <chrono>
#include <future>
#include <vector>
#include <unordered_map>

namespace Systems {
    using namespace std;
    class GLFW
    {
    public:
        static GLFW& Get()
        {
            static GLFW instance;
            return instance;
        }
        
        bool CreateWindow(string key) {
            unordered_map<string, GLFWwindow*>::const_iterator ittr = Windows().find(key);
            if ( ittr != Windows().end() ) return false;

            /* For Vulkan, request no OGL api */
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
     		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
     		auto window = glfwCreateWindow(1920, 1080, "test", NULL, NULL);
            if (!window) {
     			cout<<"Failed to create GLFW window"<<endl;
                return false;
 	    	}
            Windows()[key] = window;

            glfwSetKeyCallback(window, 
            [](GLFWwindow*,int,int,int,int){

            });
            
            return true;
        }

        bool DestroyWindow(string key) {
            unordered_map<string, GLFWwindow*>::const_iterator window = Windows().find(key);
            if ( window == Windows().end() ) return false;
            glfwDestroyWindow(window->second);
            Windows().erase(window);
            return true;
        }

    private:
        GLFW() {  
	        future<void> futureObj = exitSignal.get_future();
            eventThread = thread(&EventSystem, move(futureObj));
        }
        ~GLFW() {
            exitSignal.set_value();
            eventThread.join();
        }
    
        GLFW(const GLFW&) = delete;
        GLFW& operator=(const GLFW&) = delete;
        GLFW(GLFW&&) = delete;
        GLFW& operator=(GLFW&&) = delete;
        
        static unordered_map<string, GLFWwindow*>& Windows()
        {
            static unordered_map<string, GLFWwindow*> windows;
            return windows;
        }

        promise<void> exitSignal;
        thread eventThread;

        static void EventSystem(future<void> futureObj) {
            if (!glfwInit()) {
     			throw runtime_error("GLFW failed to initialize!");
 	    	}

            while (futureObj.wait_for(chrono::milliseconds(1)) == future_status::timeout)
            {
                glfwPollEvents();

                for (auto const& i : Windows()) {
                    if (glfwWindowShouldClose(i.second)) {
                        glfwDestroyWindow(i.second);
                        Windows().erase(i.first);
                        break;
                    }
                }
            }
        }
        

        
    };
}
