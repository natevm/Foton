#include <iostream>
#include <string>

struct CreateWindowInfo{
    int width;
    int height; 
    std::string title;
    bool visible;
    bool resizable;
    bool decorated;
    bool fullscreen;
};

inline void InitWindow(CreateWindowInfo info) {
    std::cout<<"Initializing window"<<std::endl;
    std::cout<<"\tWindow Title: "<< info.width << " " << info.height <<std::endl;
    std::cout<<"\tWindow dimensions: "<< info.width << " " << info.height <<std::endl;
    std::cout<<"\tWindow resizable: "<< info.resizable <<std::endl;
}

inline void Begin() {
    std::cout<<"Begin Vinyl"<<std::endl;
}

inline void End() {
    std::cout<<"End Vinyl"<<std::endl;
}