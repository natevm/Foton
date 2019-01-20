#pragma once

#include <iostream>
#include <glm/glm.hpp>
#include <tiny_obj_loader.h>
#include <map>

#include "Pluto/Tools/Options.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Tools/StaticFactory.hxx"

#define MAX_MESHES 1024

/* A mesh contains vertex information that has been loaded to the GPU. */
class Mesh : public StaticFactory
{
  private:
    static Mesh meshes[MAX_MESHES];
    static std::map<std::string, uint32_t> lookupTable;

    glm::vec3 centroid;

    std::vector<glm::vec3> points;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec4> colors;
    std::vector<glm::vec2> texcoords;
    std::vector<uint32_t> indices;

    tinyobj::attrib_t attrib;

    vk::Buffer pointBuffer;
    vk::DeviceMemory pointBufferMemory;

    vk::Buffer colorBuffer;
    vk::DeviceMemory colorBufferMemory;

    vk::Buffer indexBuffer;
    vk::DeviceMemory indexBufferMemory;

    vk::Buffer normalBuffer;
    vk::DeviceMemory normalBufferMemory;

    vk::Buffer texCoordBuffer;
    vk::DeviceMemory texCoordBufferMemory;

  public:
    static Mesh* Get(std::string name);
	static Mesh* Get(uint32_t id);
    static Mesh* CreateCube(std::string name);
    static Mesh* CreatePlane(std::string name);
    static Mesh* CreateSphere(std::string name);
    static Mesh* CreateFromOBJ(std::string name, std::string objPath);
    static Mesh* CreateFromSTL(std::string name, std::string stlPath);
    static Mesh* CreateFromGLB(std::string name, std::string glbPath);
    static Mesh* CreateFromRaw(
        std::string name,
        std::vector<glm::vec3> points, 
        std::vector<glm::vec3> normals = {}, 
        std::vector<glm::vec4> colors = {}, 
        std::vector<glm::vec2> texcoords = {});
    //static Mesh* Create(std::string name);
	static Mesh* GetFront();
	static uint32_t GetCount();
	static void Delete(std::string name);
	static void Delete(uint32_t id);

    class Vertex
    {
      public:
        glm::vec3 point = glm::vec3(0.0);
        glm::vec4 color = glm::vec4(1, 0, 1, 1);
        glm::vec3 normal = glm::vec3(0.0);
        glm::vec2 texcoord = glm::vec2(0.0);

        bool operator==(const Vertex &other) const
        {
            bool result =
                (point == other.point && color == other.color && normal == other.normal && texcoord == other.texcoord);
            return result;
        }
    };

    Mesh();

    Mesh(std::string name, uint32_t id);

    std::string to_string();
	
    static void Initialize();

    std::vector<glm::vec3> get_points();;

    std::vector<glm::vec4> get_colors();

    std::vector<glm::vec3> get_normals();

    std::vector<glm::vec2> get_texcoords();

    std::vector<uint32_t> get_indices();

    vk::Buffer get_point_buffer();

    vk::Buffer get_color_buffer();

    vk::Buffer get_index_buffer();

    vk::Buffer get_normal_buffer();

    vk::Buffer get_texcoord_buffer();

    uint32_t get_total_indices();

    uint32_t get_index_bytes();

    void compute_centroid();

    glm::vec3 get_centroid();

    void cleanup();

    void make_cube();
   
    void make_plane();
    
    void make_sphere();
    
    void load_obj(std::string objPath);

    void load_stl(std::string stlPath);

    void load_glb(std::string glbPath);

    void load_raw(
        std::vector<glm::vec3> &points, 
        std::vector<glm::vec3> &normals, 
        std::vector<glm::vec4> &colors, 
        std::vector<glm::vec2> &texcoords
    );

  private:
    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer &buffer, vk::DeviceMemory &bufferMemory);

    void createPointBuffer();

    void createColorBuffer();

    void createIndexBuffer();

    void createNormalBuffer();

    void createTexCoordBuffer();
};
