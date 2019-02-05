#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#endif

#define TINYGLTF_IMPLEMENTATION
// #define TINYGLTF_NO_FS
// #define TINYGLTF_NO_STB_IMAGE_WRITE

#include <sys/types.h>
#include <sys/stat.h>

#include "./Mesh.hxx"

#include "Pluto/Tools/Options.hxx"
#include "Pluto/Tools/HashCombiner.hxx"
#include <tiny_stl.h>
#include <tiny_gltf.h>

#define GENERATOR_USE_GLM
#include <generator/generator.hpp>

// For some reason, windows is defining MemoryBarrier as something else, preventing me 
// from using the vulkan MemoryBarrier type...
#ifdef WIN32
#undef MemoryBarrier
#endif

Mesh Mesh::meshes[MAX_MESHES];
std::map<std::string, uint32_t> Mesh::lookupTable;
vk::AccelerationStructureNV Mesh::topAS;
vk::DeviceMemory Mesh::topASMemory;
vk::Buffer Mesh::instanceBuffer;
vk::DeviceMemory Mesh::instanceBufferMemory;

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

namespace std
{
template <>
struct hash<Vertex>
{
    size_t operator()(const Vertex &k) const
    {
        std::size_t h = 0;
        hash_combine(h, k.point.x, k.point.y, k.point.z,
                     k.color.x, k.color.y, k.color.z, k.color.a,
                     k.normal.x, k.normal.y, k.normal.z,
                     k.texcoord.x, k.texcoord.y);
        return h;
    }
};
} // namespace std

Mesh::Mesh() {
    this->initialized = false;
}

Mesh::Mesh(std::string name, uint32_t id)
{
    this->initialized = true;
    this->name = name;
    this->id = id;
}

std::string Mesh::to_string() {
    std::string output;
    output += "{\n";
    output += "\ttype: \"Mesh\",\n";
    output += "\tname: \"" + name + "\",\n";
    output += "\tnum_points: \"" + std::to_string(points.size()) + "\",\n";
    output += "\tnum_indices: \"" + std::to_string(indices.size()) + "\",\n";
    output += "}";
    return output;
}


std::vector<glm::vec3> Mesh::get_points() {
    return points;
}

std::vector<glm::vec4> Mesh::get_colors() {
    return colors;
}

std::vector<glm::vec3> Mesh::get_normals() {
    return normals;
}

std::vector<glm::vec2> Mesh::get_texcoords() {
    return texcoords;
}

std::vector<uint32_t> Mesh::get_indices() {
    return indices;
}

vk::Buffer Mesh::get_point_buffer()
{
    return pointBuffer;
}

vk::Buffer Mesh::get_color_buffer()
{
    return colorBuffer;
}

vk::Buffer Mesh::get_index_buffer()
{
    return indexBuffer;
}

vk::Buffer Mesh::get_normal_buffer()
{
    return normalBuffer;
}

vk::Buffer Mesh::get_texcoord_buffer()
{
    return texCoordBuffer;
}

uint32_t Mesh::get_total_indices()
{
    return (uint32_t)indices.size();
}

uint32_t Mesh::get_index_bytes()
{
    return sizeof(uint32_t);
}

void Mesh::compute_centroid()
{
    glm::vec3 s(0.0);
    for (int i = 0; i < points.size(); i += 1)
    {
        s += points[i];
    }
    s /= points.size();
    centroid = s;
}

glm::vec3 Mesh::get_centroid()
{
    return centroid;
}

void Mesh::cleanup()
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error( std::string("Vulkan library is not initialized"));
    auto device = vulkan->get_device();
    if (device == vk::Device())
        throw std::runtime_error( std::string("Invalid vulkan device"));

    /* Destroy index buffer */
    device.destroyBuffer(indexBuffer);
    device.freeMemory(indexBufferMemory);

    /* Destroy vertex buffer */
    device.destroyBuffer(pointBuffer);
    device.freeMemory(pointBufferMemory);

    /* Destroy vertex color buffer */
    device.destroyBuffer(colorBuffer);
    device.freeMemory(colorBufferMemory);

    /* Destroy normal buffer */
    device.destroyBuffer(normalBuffer);
    device.freeMemory(normalBufferMemory);

    /* Destroy uv buffer */
    device.destroyBuffer(texCoordBuffer);
    device.freeMemory(texCoordBufferMemory);
}

void Mesh::Initialize() {    

}

void Mesh::load_obj(std::string objPath, bool allow_edits, bool submit_immediately)
{
    allowEdits = allow_edits;

    struct stat st;
    if (stat(objPath.c_str(), &st) != 0)
        throw std::runtime_error( std::string(objPath + " does not exist!"));

    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, objPath.c_str()))
        throw std::runtime_error( std::string("Error: Unable to load " + objPath));

    std::vector<Vertex> vertices;

    /* If the mesh has a set of shapes, merge them all into one */
    if (shapes.size() > 0)
    {
        for (const auto &shape : shapes)
        {
            for (const auto &index : shape.mesh.indices)
            {
                Vertex vertex = Vertex();
                vertex.point = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]};
                if (attrib.colors.size() != 0)
                {
                    vertex.color = {
                        attrib.colors[3 * index.vertex_index + 0],
                        attrib.colors[3 * index.vertex_index + 1],
                        attrib.colors[3 * index.vertex_index + 2],
                        1.f};
                }
                if (attrib.normals.size() != 0)
                {
                    vertex.normal = {
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]};
                }
                if (attrib.texcoords.size() != 0)
                {
                    vertex.texcoord = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        attrib.texcoords[2 * index.texcoord_index + 1]};
                }
                vertices.push_back(vertex);
            }
        }
    }

    /* If the obj has no shapes, eg polylines, then try looking for per vertex data */
    else if (shapes.size() == 0)
    {
        for (int idx = 0; idx < attrib.vertices.size() / 3; ++idx)
        {
            Vertex v = Vertex();
            v.point = glm::vec3(attrib.vertices[(idx * 3)], attrib.vertices[(idx * 3) + 1], attrib.vertices[(idx * 3) + 2]);
            if (attrib.normals.size() != 0)
            {
                v.normal = glm::vec3(attrib.normals[(idx * 3)], attrib.normals[(idx * 3) + 1], attrib.normals[(idx * 3) + 2]);
            }
            if (attrib.colors.size() != 0)
            {
                v.normal = glm::vec3(attrib.colors[(idx * 3)], attrib.colors[(idx * 3) + 1], attrib.colors[(idx * 3) + 2]);
            }
            if (attrib.texcoords.size() != 0)
            {
                v.texcoord = glm::vec2(attrib.texcoords[(idx * 2)], attrib.texcoords[(idx * 2) + 1]);
            }
            vertices.push_back(v);
        }
    }

    /* Eliminate duplicate points */
    std::unordered_map<Vertex, uint32_t> uniqueVertexMap = {};
    std::vector<Vertex> uniqueVertices;
    for (int i = 0; i < vertices.size(); ++i)
    {
        Vertex vertex = vertices[i];
        if (uniqueVertexMap.count(vertex) == 0)
        {
            uniqueVertexMap[vertex] = static_cast<uint32_t>(uniqueVertices.size());
            uniqueVertices.push_back(vertex);
        }
        indices.push_back(uniqueVertexMap[vertex]);
    }

    /* Map vertices to buffers */
    for (int i = 0; i < uniqueVertices.size(); ++i)
    {
        Vertex v = uniqueVertices[i];
        points.push_back(v.point);
        colors.push_back(v.color);
        normals.push_back(v.normal);
        texcoords.push_back(v.texcoord);
    }

    cleanup();
    compute_centroid();
    createPointBuffer(allow_edits, submit_immediately);
    createColorBuffer(allow_edits, submit_immediately);
    createIndexBuffer(allow_edits, submit_immediately);
    createNormalBuffer(allow_edits, submit_immediately);
    createTexCoordBuffer(allow_edits, submit_immediately);
}


void Mesh::load_stl(std::string stlPath, bool allow_edits, bool submit_immediately) {
    allowEdits = allow_edits;

    struct stat st;
    if (stat(stlPath.c_str(), &st) != 0)
        throw std::runtime_error( std::string(stlPath + " does not exist!"));

    std::vector<float> p;
    std::vector<float> n;

    if (!read_stl(stlPath, p, n) )
        throw std::runtime_error( std::string("Error: Unable to load " + stlPath));

    std::vector<Vertex> vertices;

    /* STLs only have points and face normals, so generate colors and UVs */
    for (uint32_t i = 0; i < p.size() / 3; ++i) {
        Vertex vertex = Vertex();
        vertex.point = {
            p[i * 3 + 0],
            p[i * 3 + 1],
            p[i * 3 + 2],
        };
        vertex.normal = {
            n[i * 3 + 0],
            n[i * 3 + 1],
            n[i * 3 + 2],
        };
        vertices.push_back(vertex);
    }

    /* Eliminate duplicate points */
    std::unordered_map<Vertex, uint32_t> uniqueVertexMap = {};
    std::vector<Vertex> uniqueVertices;
    for (int i = 0; i < vertices.size(); ++i)
    {
        Vertex vertex = vertices[i];
        if (uniqueVertexMap.count(vertex) == 0)
        {
            uniqueVertexMap[vertex] = static_cast<uint32_t>(uniqueVertices.size());
            uniqueVertices.push_back(vertex);
        }
        indices.push_back(uniqueVertexMap[vertex]);
    }

    /* Map vertices to buffers */
    for (int i = 0; i < uniqueVertices.size(); ++i)
    {
        Vertex v = uniqueVertices[i];
        points.push_back(v.point);
        colors.push_back(v.color);
        normals.push_back(v.normal);
        texcoords.push_back(v.texcoord);
    }

    cleanup();
    compute_centroid();
    createPointBuffer(allow_edits, submit_immediately);
    createColorBuffer(allow_edits, submit_immediately);
    createIndexBuffer(allow_edits, submit_immediately);
    createNormalBuffer(allow_edits, submit_immediately);
    createTexCoordBuffer(allow_edits, submit_immediately);
}

void Mesh::load_glb(std::string glbPath, bool allow_edits, bool submit_immediately)
{
    allowEdits = allow_edits;
    struct stat st;
    if (stat(glbPath.c_str(), &st) != 0)
    {
        throw std::runtime_error(std::string("Error: " + glbPath + " does not exist"));
    }

    // read file
    unsigned char *file_buffer = NULL;
	uint32_t file_size = 0;
	{
        FILE *fp = fopen(glbPath.c_str(), "rb");
        if (!fp) {
            throw std::runtime_error( std::string(glbPath + " does not exist!"));
        }
		assert(fp);
		fseek(fp, 0, SEEK_END);
		file_size = (uint32_t)ftell(fp);
		rewind(fp);
		file_buffer = (unsigned char *)malloc(file_size);
		assert(file_buffer);
		fread(file_buffer, 1, file_size, fp);
		fclose(fp);
	}

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;

    std::string err, warn;
    if (!loader.LoadBinaryFromMemory(&model, &err, &warn, file_buffer, file_size, "", tinygltf::REQUIRE_ALL))
        throw std::runtime_error( std::string("Error: Unable to load " + glbPath + " " + err));

    std::vector<Vertex> vertices;

    for (const auto &mesh : model.meshes) {
        for (const auto &primitive : mesh.primitives)
        {
            const auto &idx_accessor = model.accessors[primitive.indices];
            const auto &pos_accessor = model.accessors[primitive.attributes.find("POSITION")->second];
            const auto &nrm_accessor = model.accessors[primitive.attributes.find("NORMAL")->second];
            const auto &tex_accessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];

            const auto &idx_bufferView = model.bufferViews[idx_accessor.bufferView];
            const auto &pos_bufferView = model.bufferViews[pos_accessor.bufferView];
            const auto &nrm_bufferView = model.bufferViews[nrm_accessor.bufferView];
            const auto &tex_bufferView = model.bufferViews[tex_accessor.bufferView];

            const auto &idx_buffer = model.buffers[idx_bufferView.buffer]; 
            const auto &pos_buffer = model.buffers[pos_bufferView.buffer]; 
            const auto &nrm_buffer = model.buffers[nrm_bufferView.buffer]; 
            const auto &tex_buffer = model.buffers[tex_bufferView.buffer]; 

            const float *pos = (const float *) pos_buffer.data.data();
            const float *nrm = (const float *) nrm_buffer.data.data();
            const float *tex = (const float *) tex_buffer.data.data();
            const char* idx  = (const char *) &idx_buffer.data[idx_bufferView.byteOffset];

            /* For each vertex */
            for (int i = 0; i < idx_accessor.count; ++ i) {
                unsigned int index = -1;
                if (idx_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                    index = (unsigned int) ((unsigned int*)idx)[i];
                else 
                    index = (unsigned int) ((unsigned short*)idx)[i];
                
                Vertex vertex = Vertex();
                vertex.point = {
                    pos[3 * index + 0],
                    pos[3 * index + 1],
                    pos[3 * index + 2]};

                vertex.normal = {
                    nrm[3 * index + 0],
                    nrm[3 * index + 1],
                    nrm[3 * index + 2]};

                vertex.texcoord = {
                    tex[2 * index + 0],
                    tex[2 * index + 1]};
                
                vertices.push_back(vertex);
            }
        }
    }

    /* Eliminate duplicate points */
    std::unordered_map<Vertex, uint32_t> uniqueVertexMap = {};
    std::vector<Vertex> uniqueVertices;
    for (int i = 0; i < vertices.size(); ++i)
    {
        Vertex vertex = vertices[i];
        if (uniqueVertexMap.count(vertex) == 0)
        {
            uniqueVertexMap[vertex] = static_cast<uint32_t>(uniqueVertices.size());
            uniqueVertices.push_back(vertex);
        }
        indices.push_back(uniqueVertexMap[vertex]);
    }

    /* Map vertices to buffers */
    for (int i = 0; i < uniqueVertices.size(); ++i)
    {
        Vertex v = uniqueVertices[i];
        points.push_back(v.point);
        colors.push_back(v.color);
        normals.push_back(v.normal);
        texcoords.push_back(v.texcoord);
    }

    cleanup();
    compute_centroid();
    createPointBuffer(allow_edits, submit_immediately);
    createColorBuffer(allow_edits, submit_immediately);
    createIndexBuffer(allow_edits, submit_immediately);
    createNormalBuffer(allow_edits, submit_immediately);
    createTexCoordBuffer(allow_edits, submit_immediately);
}

void Mesh::load_raw(
    std::vector<glm::vec3> &points_, 
    std::vector<glm::vec3> &normals_, 
    std::vector<glm::vec4> &colors_, 
    std::vector<glm::vec2> &texcoords_, 
    std::vector<uint32_t> indices_,
    bool allow_edits, bool submit_immediately
)
{
    allowEdits = allow_edits;
    bool reading_normals = normals_.size() > 0;
    bool reading_colors = colors_.size() > 0;
    bool reading_texcoords = texcoords_.size() > 0;
    bool reading_indices = indices_.size() > 0;

    if (points_.size() == 0)
        throw std::runtime_error( std::string("Error, no points supplied. "));

    if ((!reading_indices) && ((points_.size() % 3) != 0))
        throw std::runtime_error( std::string("Error: No indices provided, and length of points is not a multiple of 3."));

    if ((reading_indices) && ((indices_.size() % 3) != 0))
        throw std::runtime_error( std::string("Error: Length of indices is not a multiple of 3."));
    
    if (reading_normals && (normals_.size() != points_.size()))
        throw std::runtime_error( std::string("Error, length mismatch. Total normals: " + std::to_string(normals_.size()) + " does not equal total points: " + std::to_string(points_.size())));

    if (reading_colors && (colors_.size() != points_.size()))
        throw std::runtime_error( std::string("Error, length mismatch. Total colors: " + std::to_string(colors_.size()) + " does not equal total points: " + std::to_string(points_.size())));
        
    if (reading_texcoords && (texcoords_.size() != points_.size()))
        throw std::runtime_error( std::string("Error, length mismatch. Total texcoords: " + std::to_string(texcoords_.size()) + " does not equal total points: " + std::to_string(points_.size())));
    
    if (reading_indices) {
        for (uint32_t i = 0; i < indices_.size(); ++i) {
            if (indices_[i] >= points_.size())
                throw std::runtime_error( std::string("Error, index out of bounds. Index " + std::to_string(i) + " is greater than total points: " + std::to_string(points_.size())));
        }
    }
        
    std::vector<Vertex> vertices;

    /* For each vertex */
    for (int i = 0; i < points_.size(); ++ i) {
        Vertex vertex = Vertex();
        vertex.point = points_[i];
        if (reading_normals) vertex.normal = normals_[i];
        if (reading_colors) vertex.color = colors_[i];
        if (reading_texcoords) vertex.texcoord = texcoords_[i];        
        vertices.push_back(vertex);
    }

    /* Eliminate duplicate points */
    std::unordered_map<Vertex, uint32_t> uniqueVertexMap = {};
    std::vector<Vertex> uniqueVertices;

    /* Don't bin points as unique when editing, since it's unexpected for a user to lose points */
    if (allow_edits && !reading_indices) {
        uniqueVertices = vertices;
        for (int i = 0; i < vertices.size(); ++i) {
            indices.push_back(i);
        }
    }
    else if (reading_indices) {
        indices = indices_;
        uniqueVertices = vertices;
    }
    /* If indices werent supplied and editing isn't allowed, optimize by binning unique verts */
    else {    
        for (int i = 0; i < vertices.size(); ++i)
        {
            Vertex vertex = vertices[i];
            if (uniqueVertexMap.count(vertex) == 0)
            {
                uniqueVertexMap[vertex] = static_cast<uint32_t>(uniqueVertices.size());
                uniqueVertices.push_back(vertex);
            }
            indices.push_back(uniqueVertexMap[vertex]);
        }
    }

    /* Map vertices to buffers */
    for (int i = 0; i < uniqueVertices.size(); ++i)
    {
        Vertex v = uniqueVertices[i];
        points.push_back(v.point);
        colors.push_back(v.color);
        normals.push_back(v.normal);
        texcoords.push_back(v.texcoord);
    }

    cleanup();
    compute_centroid();
    createPointBuffer(allow_edits, submit_immediately);
    createColorBuffer(allow_edits, submit_immediately);
    createIndexBuffer(allow_edits, submit_immediately);
    createNormalBuffer(allow_edits, submit_immediately);
    createTexCoordBuffer(allow_edits, submit_immediately);
}

void Mesh::edit_position(uint32_t index, glm::vec3 new_position)
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error("Error: Vulkan is not initialized");
    auto device = vulkan->get_device();

    if (!allowEdits)
        throw std::runtime_error("Error: editing this component is not allowed. \
            Edits can be enabled during creation.");
    
    if (index >= this->points.size())
        throw std::runtime_error("Error: index out of bounds. Max index is " + std::to_string(this->points.size() - 1));
    
    void *data = device.mapMemory(pointBufferMemory, (index * sizeof(glm::vec3)), sizeof(glm::vec3), vk::MemoryMapFlags());
    memcpy(data, &new_position, sizeof(glm::vec3));
    device.unmapMemory(pointBufferMemory);
}

void Mesh::edit_positions(uint32_t index, std::vector<glm::vec3> new_positions)
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error("Error: Vulkan is not initialized");
    auto device = vulkan->get_device();

    if (!allowEdits)
        throw std::runtime_error("Error: editing this component is not allowed. \
            Edits can be enabled during creation.");
    
    if (index >= this->points.size())
        throw std::runtime_error("Error: index out of bounds. Max index is " + std::to_string(this->points.size() - 1));
    
    if ((index + new_positions.size()) > this->points.size())
        throw std::runtime_error("Error: too many positions for given index, out of bounds. Max index is " + std::to_string(this->points.size() - 1));
    
    void *data = device.mapMemory(pointBufferMemory, (index * sizeof(glm::vec3)), sizeof(glm::vec3) * new_positions.size(), vk::MemoryMapFlags());
    memcpy(data, new_positions.data(), sizeof(glm::vec3) * new_positions.size());
    device.unmapMemory(pointBufferMemory);
}

void Mesh::edit_normal(uint32_t index, glm::vec3 new_normal)
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error("Error: Vulkan is not initialized");
    auto device = vulkan->get_device();

    if (!allowEdits)
        throw std::runtime_error("Error: editing this component is not allowed. \
            Edits can be enabled during creation.");
    
    if (index >= this->normals.size())
        throw std::runtime_error("Error: index out of bounds. Max index is " + std::to_string(this->normals.size() - 1));
    
    void *data = device.mapMemory(normalBufferMemory, (index * sizeof(glm::vec3)), sizeof(glm::vec3), vk::MemoryMapFlags());
    memcpy(data, &new_normal, sizeof(glm::vec3));
    device.unmapMemory(normalBufferMemory);
}

void Mesh::edit_normals(uint32_t index, std::vector<glm::vec3> new_normals)
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error("Error: Vulkan is not initialized");
    auto device = vulkan->get_device();

    if (!allowEdits)
        throw std::runtime_error("Error: editing this component is not allowed. \
            Edits can be enabled during creation.");
    
    if (index >= this->normals.size())
        throw std::runtime_error("Error: index out of bounds. Max index is " + std::to_string(this->normals.size() - 1));
    
    if ((index + new_normals.size()) > this->normals.size())
        throw std::runtime_error("Error: too many normals for given index, out of bounds. Max index is " + std::to_string(this->normals.size() - 1));
    
    void *data = device.mapMemory(normalBufferMemory, (index * sizeof(glm::vec3)), sizeof(glm::vec3) * new_normals.size(), vk::MemoryMapFlags());
    memcpy(data, new_normals.data(), sizeof(glm::vec3) * new_normals.size());
    device.unmapMemory(normalBufferMemory);
}

void Mesh::edit_vertex_color(uint32_t index, glm::vec4 new_color)
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error("Error: Vulkan is not initialized");
    auto device = vulkan->get_device();

    if (!allowEdits)
        throw std::runtime_error("Error: editing this component is not allowed. \
            Edits can be enabled during creation.");
    
    if (index >= this->colors.size())
        throw std::runtime_error("Error: index out of bounds. Max index is " + std::to_string(this->colors.size() - 1));
    
    void *data = device.mapMemory(colorBufferMemory, (index * sizeof(glm::vec4)), sizeof(glm::vec4), vk::MemoryMapFlags());
    memcpy(data, &new_color, sizeof(glm::vec4));
    device.unmapMemory(colorBufferMemory);
}

void Mesh::edit_vertex_colors(uint32_t index, std::vector<glm::vec4> new_colors)
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error("Error: Vulkan is not initialized");
    auto device = vulkan->get_device();

    if (!allowEdits)
        throw std::runtime_error("Error: editing this component is not allowed. \
            Edits can be enabled during creation.");
    
    if (index >= this->colors.size())
        throw std::runtime_error("Error: index out of bounds. Max index is " + std::to_string(this->colors.size() - 1));
    
    if ((index + new_colors.size()) > this->colors.size())
        throw std::runtime_error("Error: too many colors for given index, out of bounds. Max index is " + std::to_string(this->colors.size() - 1));
    
    void *data = device.mapMemory(colorBufferMemory, (index * sizeof(glm::vec4)), sizeof(glm::vec4) * new_colors.size(), vk::MemoryMapFlags());
    memcpy(data, new_colors.data(), sizeof(glm::vec4) * new_colors.size());
    device.unmapMemory(colorBufferMemory);
}

void Mesh::edit_texture_coordinate(uint32_t index, glm::vec2 new_texcoord)
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error("Error: Vulkan is not initialized");
    auto device = vulkan->get_device();

    if (!allowEdits)
        throw std::runtime_error("Error: editing this component is not allowed. \
            Edits can be enabled during creation.");
    
    if (index >= this->texcoords.size())
        throw std::runtime_error("Error: index out of bounds. Max index is " + std::to_string(this->texcoords.size() - 1));
    
    void *data = device.mapMemory(texCoordBufferMemory, (index * sizeof(glm::vec2)), sizeof(glm::vec2), vk::MemoryMapFlags());
    memcpy(data, &new_texcoord, sizeof(glm::vec2));
    device.unmapMemory(texCoordBufferMemory);
}

void Mesh::edit_texture_coordinates(uint32_t index, std::vector<glm::vec2> new_texcoords)
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized())
        throw std::runtime_error("Error: Vulkan is not initialized");
    auto device = vulkan->get_device();

    if (!allowEdits)
        throw std::runtime_error("Error: editing this component is not allowed. \
            Edits can be enabled during creation.");
    
    if (index >= this->texcoords.size())
        throw std::runtime_error("Error: index out of bounds. Max index is " + std::to_string(this->texcoords.size() - 1));
    
    if ((index + new_texcoords.size()) > this->texcoords.size())
        throw std::runtime_error("Error: too many texture coordinates for given index, out of bounds. Max index is " + std::to_string(this->texcoords.size() - 1));
    
    void *data = device.mapMemory(texCoordBufferMemory, (index * sizeof(glm::vec2)), sizeof(glm::vec2) * new_texcoords.size(), vk::MemoryMapFlags());
    memcpy(data, new_texcoords.data(), sizeof(glm::vec2) * new_texcoords.size());
    device.unmapMemory(texCoordBufferMemory);
}

void Mesh::build_top_level_bvh(bool submit_immediately)
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized()) throw std::runtime_error("Error: vulkan is not initialized");

    if (!vulkan->is_ray_tracing_enabled()) 
        throw std::runtime_error("Error: Vulkan device extension VK_NVX_raytracing is not currently enabled.");
    
    auto dldi = vulkan->get_dldi();
    auto device = vulkan->get_device();
    if (!device) 
        throw std::runtime_error("Error: vulkan device not initialized");

    auto CreateAccelerationStructure = [&](vk::AccelerationStructureTypeNV type, uint32_t geometryCount,
        vk::GeometryNV* geometries, uint32_t instanceCount, vk::AccelerationStructureNV& AS, vk::DeviceMemory& memory)
    {
        vk::AccelerationStructureCreateInfoNV accelerationStructureInfo;
        accelerationStructureInfo.compactedSize = 0;
        accelerationStructureInfo.info.type = type;
        accelerationStructureInfo.info.instanceCount = instanceCount;
        accelerationStructureInfo.info.geometryCount = geometryCount;
        accelerationStructureInfo.info.pGeometries = geometries;

        AS = device.createAccelerationStructureNV(accelerationStructureInfo, nullptr, dldi);

        vk::AccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo;
        memoryRequirementsInfo.accelerationStructure = AS;
        memoryRequirementsInfo.type = vk::AccelerationStructureMemoryRequirementsTypeNV::eObject;

        vk::MemoryRequirements2 memoryRequirements;
        memoryRequirements = device.getAccelerationStructureMemoryRequirementsNV(memoryRequirementsInfo, dldi);

        vk::MemoryAllocateInfo memoryAllocateInfo;
        memoryAllocateInfo.allocationSize = memoryRequirements.memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = vulkan->find_memory_type(memoryRequirements.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

        memory = device.allocateMemory(memoryAllocateInfo);
        
        vk::BindAccelerationStructureMemoryInfoNV bindInfo;
        bindInfo.accelerationStructure = AS;
        bindInfo.memory = memory;
        bindInfo.memoryOffset = 0;
        bindInfo.deviceIndexCount = 0;
        bindInfo.pDeviceIndices = nullptr;

        device.bindAccelerationStructureMemoryNV({bindInfo}, dldi);
    };

    /* Create top level acceleration structure */
    CreateAccelerationStructure(vk::AccelerationStructureTypeNV::eTopLevel,
        0, nullptr, 1, topAS, topASMemory);


    /* Gather Instances */
    std::vector<VkGeometryInstance> instances;
    for (uint32_t i = 0; i < MAX_MESHES; ++i)
    {
        if (!meshes[i].is_initialized()) continue;
        if (!meshes[i].lowBVHBuilt) continue;
        instances.push_back(meshes[i].instance);
    }

    /* ----- Create Instance Buffer ----- */
    {
        uint32_t instanceBufferSize = (uint32_t)(sizeof(VkGeometryInstance) * instances.size());

        vk::BufferCreateInfo instanceBufferInfo;
        instanceBufferInfo.size = instanceBufferSize;
        instanceBufferInfo.usage = vk::BufferUsageFlagBits::eRayTracingNV;
        instanceBufferInfo.sharingMode = vk::SharingMode::eExclusive;

        instanceBuffer = device.createBuffer(instanceBufferInfo);

        vk::MemoryRequirements instanceBufferRequirements;
        instanceBufferRequirements = device.getBufferMemoryRequirements(instanceBuffer);

        vk::MemoryAllocateInfo instanceMemoryAllocateInfo;
        instanceMemoryAllocateInfo.allocationSize = instanceBufferRequirements.size;
        instanceMemoryAllocateInfo.memoryTypeIndex = vulkan->find_memory_type(instanceBufferRequirements.memoryTypeBits, 
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent );

        instanceBufferMemory = device.allocateMemory(instanceMemoryAllocateInfo);
        
        device.bindBufferMemory(instanceBuffer, instanceBufferMemory, 0);

        void* ptr = device.mapMemory(instanceBufferMemory, 0, instanceBufferSize);
        memcpy(ptr, instances.data(), instanceBufferSize);
        device.unmapMemory(instanceBufferMemory);
    }

    /* Build top level BVH */
    auto GetScratchBufferSize = [&](vk::AccelerationStructureNV handle)
    {
        vk::AccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo;
        memoryRequirementsInfo.accelerationStructure = handle;
        memoryRequirementsInfo.type = vk::AccelerationStructureMemoryRequirementsTypeNV::eBuildScratch;

        vk::MemoryRequirements2 memoryRequirements;
        memoryRequirements = device.getAccelerationStructureMemoryRequirementsNV( memoryRequirementsInfo, dldi);

        vk::DeviceSize result = memoryRequirements.memoryRequirements.size;
        return result;
    };

    {
        vk::DeviceSize scratchBufferSize = GetScratchBufferSize(topAS);

        vk::BufferCreateInfo bufferInfo;
        bufferInfo.size = scratchBufferSize;
        bufferInfo.usage = vk::BufferUsageFlagBits::eRayTracingNV;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;
        vk::Buffer accelerationStructureScratchBuffer = device.createBuffer(bufferInfo);
        
        vk::MemoryRequirements scratchBufferRequirements;
        scratchBufferRequirements = device.getBufferMemoryRequirements(accelerationStructureScratchBuffer);
        
        vk::MemoryAllocateInfo scratchMemoryAllocateInfo;
        scratchMemoryAllocateInfo.allocationSize = scratchBufferRequirements.size;
        scratchMemoryAllocateInfo.memoryTypeIndex = vulkan->find_memory_type(scratchBufferRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::DeviceMemory accelerationStructureScratchMemory = device.allocateMemory(scratchMemoryAllocateInfo);
        device.bindBufferMemory(accelerationStructureScratchBuffer, accelerationStructureScratchMemory, 0);

        /* Now we can build our acceleration structure */
        vk::MemoryBarrier memoryBarrier;
        memoryBarrier.srcAccessMask  = vk::AccessFlagBits::eAccelerationStructureWriteNV;
        memoryBarrier.srcAccessMask |= vk::AccessFlagBits::eAccelerationStructureReadNV;
        memoryBarrier.dstAccessMask  = vk::AccessFlagBits::eAccelerationStructureWriteNV;
        memoryBarrier.dstAccessMask |= vk::AccessFlagBits::eAccelerationStructureReadNV;

        auto cmd = vulkan->begin_one_time_graphics_command();

        /* TODO: MOVE INSTANCE STUFF INTO HERE */
        {
            vk::AccelerationStructureInfoNV asInfo;
            asInfo.type = vk::AccelerationStructureTypeNV::eTopLevel;
            asInfo.instanceCount = (uint32_t) instances.size();
            asInfo.geometryCount = 0;// (uint32_t)geometries.size();
            asInfo.pGeometries = nullptr;//&geometries[0];

            cmd.buildAccelerationStructureNV(&asInfo, 
                instanceBuffer, 0, VK_FALSE, 
                topAS, vk::AccelerationStructureNV(),
                accelerationStructureScratchBuffer, 0, dldi);
        }
        
        // cmd.pipelineBarrier(
        //     vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, 
        //     vk::PipelineStageFlagBits::eRayTracingShaderNV, 
        //     vk::DependencyFlags(), {memoryBarrier}, {}, {});

        vulkan->end_one_time_graphics_command(cmd, "build acceleration structure", true, submit_immediately);
    }

}

void Mesh::build_low_level_bvh(bool submit_immediately)
{
    auto vulkan = Libraries::Vulkan::Get();
    if (!vulkan->is_initialized()) throw std::runtime_error("Error: vulkan is not initialized");

    if (!vulkan->is_ray_tracing_enabled()) 
        throw std::runtime_error("Error: Vulkan device extension VK_NVX_raytracing is not currently enabled.");
    
    auto dldi = vulkan->get_dldi();
    auto device = vulkan->get_device();
    if (!device) 
        throw std::runtime_error("Error: vulkan device not initialized");



    /* ----- Make geometry handle ----- */
    vk::GeometryDataNV geoData;

    {
        vk::GeometryTrianglesNV tris;
        tris.vertexData = this->pointBuffer;
        tris.vertexOffset = 0;
        tris.vertexCount = (uint32_t) this->points.size();
        tris.vertexStride = sizeof(glm::vec3);
        tris.vertexFormat = vk::Format::eR32G32B32A32Sfloat;
        tris.indexData = this->indexBuffer;
        tris.indexOffset = 0;
        tris.indexType = vk::IndexType::eUint32;

        geoData.triangles = tris;
        geometry.geometryType = vk::GeometryTypeNV::eTriangles;
        geometry.geometry = geoData;
    }
    


    /* ----- Create the bottom level acceleration structure ----- */
    // Bottom level acceleration structures correspond to the geometry

    auto CreateAccelerationStructure = [&](vk::AccelerationStructureTypeNV type, uint32_t geometryCount,
        vk::GeometryNV* geometries, uint32_t instanceCount, vk::AccelerationStructureNV& AS, vk::DeviceMemory& memory)
    {
        vk::AccelerationStructureCreateInfoNV accelerationStructureInfo;
        accelerationStructureInfo.compactedSize = 0;
        accelerationStructureInfo.info.type = type;
        accelerationStructureInfo.info.instanceCount = instanceCount;
        accelerationStructureInfo.info.geometryCount = geometryCount;
        accelerationStructureInfo.info.pGeometries = geometries;

        AS = device.createAccelerationStructureNV(accelerationStructureInfo, nullptr, dldi);

        vk::AccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo;
        memoryRequirementsInfo.accelerationStructure = AS;
        memoryRequirementsInfo.type = vk::AccelerationStructureMemoryRequirementsTypeNV::eObject;

        vk::MemoryRequirements2 memoryRequirements;
        memoryRequirements = device.getAccelerationStructureMemoryRequirementsNV(memoryRequirementsInfo, dldi);

        vk::MemoryAllocateInfo memoryAllocateInfo;
        memoryAllocateInfo.allocationSize = memoryRequirements.memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = vulkan->find_memory_type(memoryRequirements.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

        memory = device.allocateMemory(memoryAllocateInfo);
        
        vk::BindAccelerationStructureMemoryInfoNV bindInfo;
        bindInfo.accelerationStructure = AS;
        bindInfo.memory = memory;
        bindInfo.memoryOffset = 0;
        bindInfo.deviceIndexCount = 0;
        bindInfo.pDeviceIndices = nullptr;

        device.bindAccelerationStructureMemoryNV({bindInfo}, dldi);
    };

    CreateAccelerationStructure(vk::AccelerationStructureTypeNV::eBottomLevel,
        1, &geometry, 0, lowAS, lowASMemory);

    /* Create Instance */
    {
        uint64_t accelerationStructureHandle;
        device.getAccelerationStructureHandleNV(lowAS, sizeof(uint64_t), &accelerationStructureHandle, dldi);
        float transform[12] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
        };

        memcpy(instance.transform, transform, sizeof(instance.transform));
        instance.instanceId = 0;
        instance.mask = 0xff;
        instance.instanceOffset = 0;
        instance.flags = (uint32_t) vk::GeometryInstanceFlagBitsNV::eTriangleCullDisable;
        instance.accelerationStructureHandle = accelerationStructureHandle;
    }


    /* Build low level BVH */
    auto GetScratchBufferSize = [&](vk::AccelerationStructureNV handle)
    {
        vk::AccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo;
        memoryRequirementsInfo.accelerationStructure = handle;
        memoryRequirementsInfo.type = vk::AccelerationStructureMemoryRequirementsTypeNV::eBuildScratch;

        vk::MemoryRequirements2 memoryRequirements;
        memoryRequirements = device.getAccelerationStructureMemoryRequirementsNV( memoryRequirementsInfo, dldi);

        vk::DeviceSize result = memoryRequirements.memoryRequirements.size;
        return result;
    };

    {
        vk::DeviceSize scratchBufferSize = GetScratchBufferSize(lowAS);

        vk::BufferCreateInfo bufferInfo;
        bufferInfo.size = scratchBufferSize;
        bufferInfo.usage = vk::BufferUsageFlagBits::eRayTracingNV;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;
        vk::Buffer accelerationStructureScratchBuffer = device.createBuffer(bufferInfo);
        
        vk::MemoryRequirements scratchBufferRequirements;
        scratchBufferRequirements = device.getBufferMemoryRequirements(accelerationStructureScratchBuffer);
        
        vk::MemoryAllocateInfo scratchMemoryAllocateInfo;
        scratchMemoryAllocateInfo.allocationSize = scratchBufferRequirements.size;
        scratchMemoryAllocateInfo.memoryTypeIndex = vulkan->find_memory_type(scratchBufferRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::DeviceMemory accelerationStructureScratchMemory = device.allocateMemory(scratchMemoryAllocateInfo);
        device.bindBufferMemory(accelerationStructureScratchBuffer, accelerationStructureScratchMemory, 0);

        /* Now we can build our acceleration structure */
        vk::MemoryBarrier memoryBarrier;
        memoryBarrier.srcAccessMask  = vk::AccessFlagBits::eAccelerationStructureWriteNV;
        memoryBarrier.srcAccessMask |= vk::AccessFlagBits::eAccelerationStructureReadNV;
        memoryBarrier.dstAccessMask  = vk::AccessFlagBits::eAccelerationStructureWriteNV;
        memoryBarrier.dstAccessMask |= vk::AccessFlagBits::eAccelerationStructureReadNV;

        auto cmd = vulkan->begin_one_time_graphics_command();

        {
            vk::AccelerationStructureInfoNV asInfo;
            asInfo.type = vk::AccelerationStructureTypeNV::eBottomLevel;
            asInfo.instanceCount = 0;
            asInfo.geometryCount = 1;// (uint32_t)geometries.size();
            asInfo.pGeometries = &geometry;//&geometries[0];

            cmd.buildAccelerationStructureNV(&asInfo, 
                vk::Buffer(), 0, VK_FALSE, 
                lowAS, vk::AccelerationStructureNV(),
                accelerationStructureScratchBuffer, 0, dldi);
        }
        
        // cmd.pipelineBarrier(
        //     vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, 
        //     vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, 
        //     vk::DependencyFlags(), {memoryBarrier}, {}, {});

        vulkan->end_one_time_graphics_command(cmd, "build acceleration structure", true, submit_immediately);
    }

    /* Might need a fence here */
    lowBVHBuilt = true;
}

/* Static Factory Implementations */
Mesh* Mesh::Get(std::string name) {
    return StaticFactory::Get(name, "Mesh", lookupTable, meshes, MAX_MESHES);
}

Mesh* Mesh::Get(uint32_t id) {
    return StaticFactory::Get(id, "Mesh", lookupTable, meshes, MAX_MESHES);
}

Mesh* Mesh::CreateBox(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::BoxMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateCappedCone(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::CappedConeMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateCappedCylinder(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::CappedCylinderMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateCappedTube(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::CappedTubeMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateCapsule(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::CapsuleMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateCone(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::ConeMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateConvexPolygon(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::ConvexPolygonMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateCylinder(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::CylinderMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateDisk(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::DiskMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateDodecahedron(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::DodecahedronMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreatePlane(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::PlaneMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateIcosahedron(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::IcosahedronMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateIcosphere(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::IcoSphereMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

/* Might add this later. Requires a callback which defines a function mapping R2->R */
// Mesh* Mesh::CreateParametricMesh(std::string name, uint32_t x_segments = 16, uint32_t y_segments = 16, bool allow_edits, bool submit_immediately)
// {
//     auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
//     if (!mesh) return nullptr;
//     auto gen_mesh = generator::ParametricMesh( , glm::ivec2(x_segments, y_segments));
//     mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
//     return mesh;
// }

Mesh* Mesh::CreateRoundedBox(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::RoundedBoxMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateSphere(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::SphereMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateSphericalCone(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::SphericalConeMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateSphericalTriangle(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::SphericalTriangleMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateSpring(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::SpringMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateTeapot(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::TeapotMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateTorus(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::TorusMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateTorusKnot(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::TorusKnotMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateTriangle(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::TriangleMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateTube(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    generator::TubeMesh gen_mesh{};
    mesh->make_primitive(gen_mesh, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateFromOBJ(std::string name, std::string objPath, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    mesh->load_obj(objPath, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateFromSTL(std::string name, std::string stlPath, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    mesh->load_stl(stlPath, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateFromGLB(std::string name, std::string glbPath, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    mesh->load_glb(glbPath, allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateFromRaw (
    std::string name,
    std::vector<glm::vec3> points, 
    std::vector<glm::vec3> normals, 
    std::vector<glm::vec4> colors, 
    std::vector<glm::vec2> texcoords, 
    std::vector<uint32_t> indices, 
    bool allow_edits, 
    bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    mesh->load_raw(points, normals, colors, texcoords, indices, allow_edits, submit_immediately);
    return mesh;
}

void Mesh::Delete(std::string name) {
    StaticFactory::Delete(name, "Mesh", lookupTable, meshes, MAX_MESHES);
}

void Mesh::Delete(uint32_t id) {
    StaticFactory::Delete(id, "Mesh", lookupTable, meshes, MAX_MESHES);
}

Mesh* Mesh::GetFront() {
    return meshes;
}

uint32_t Mesh::GetCount() {
    return MAX_MESHES;
}

void Mesh::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer &buffer, vk::DeviceMemory &bufferMemory)
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    /* To create a VBO, we need to use this struct: */
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    /* Now create the buffer */
    buffer = device.createBuffer(bufferInfo);

    /* Identify the memory requirements for the vertex buffer */
    vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(buffer);

    /* Look for a suitable type that meets our property requirements */
    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vulkan->find_memory_type(memRequirements.memoryTypeBits, properties);

    /* Now, allocate the memory for that buffer */
    bufferMemory = device.allocateMemory(allocInfo);

    /* Associate the allocated memory with the VBO handle */
    device.bindBufferMemory(buffer, bufferMemory, 0);
}

void Mesh::createPointBuffer(bool allow_edits, bool submit_immediately)
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    vk::DeviceSize bufferSize = points.size() * sizeof(glm::vec3);
    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    /* Map the memory to a pointer on the host */
    void *data = device.mapMemory(stagingBufferMemory, 0, bufferSize,  vk::MemoryMapFlags());

    /* Copy over our vertex data, then unmap */
    memcpy(data, points.data(), (size_t)bufferSize);
    device.unmapMemory(stagingBufferMemory);

    vk::MemoryPropertyFlags memoryProperties;
    if (!allowEdits) memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    else {
        memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible;
        memoryProperties |= vk::MemoryPropertyFlagBits::eHostCoherent;
    }
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, memoryProperties, pointBuffer, pointBufferMemory);
    
    auto cmd = vulkan->begin_one_time_graphics_command();
    vk::BufferCopy copyRegion;
    copyRegion.size = bufferSize;
    cmd.copyBuffer(stagingBuffer, pointBuffer, copyRegion);
    vulkan->end_one_time_graphics_command(cmd, "copy point buffer", true, submit_immediately);

    /* Clean up the staging buffer */
    device.destroyBuffer(stagingBuffer);
    device.freeMemory(stagingBufferMemory);
}

void Mesh::createColorBuffer(bool allow_edits, bool submit_immediately)
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    vk::DeviceSize bufferSize = colors.size() * sizeof(glm::vec4);
    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    /* Map the memory to a pointer on the host */
    void *data = device.mapMemory(stagingBufferMemory, 0, bufferSize,  vk::MemoryMapFlags());

    /* Copy over our vertex data, then unmap */
    memcpy(data, colors.data(), (size_t)bufferSize);
    device.unmapMemory(stagingBufferMemory);

    vk::MemoryPropertyFlags memoryProperties;
    if (!allowEdits) memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    else {
        memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible;
        memoryProperties |= vk::MemoryPropertyFlagBits::eHostCoherent;
    }
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, memoryProperties, colorBuffer, colorBufferMemory);
    
    auto cmd = vulkan->begin_one_time_graphics_command();
    vk::BufferCopy copyRegion;
    copyRegion.size = bufferSize;
    cmd.copyBuffer(stagingBuffer, colorBuffer, copyRegion);
    vulkan->end_one_time_graphics_command(cmd, "copy point color buffer", true, submit_immediately);

    /* Clean up the staging buffer */
    device.destroyBuffer(stagingBuffer);
    device.freeMemory(stagingBufferMemory);
}

void Mesh::createIndexBuffer(bool allow_edits, bool submit_immediately)
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    vk::DeviceSize bufferSize = indices.size() * sizeof(uint32_t);
    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    void *data = device.mapMemory(stagingBufferMemory, 0, bufferSize, vk::MemoryMapFlags());
    memcpy(data, indices.data(), (size_t)bufferSize);
    device.unmapMemory(stagingBufferMemory);

    vk::MemoryPropertyFlags memoryProperties;
    if (!allowEdits) memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    else {
        memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible;
        memoryProperties |= vk::MemoryPropertyFlagBits::eHostCoherent;
    }
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, memoryProperties, indexBuffer, indexBufferMemory);
    
    auto cmd = vulkan->begin_one_time_graphics_command();
    vk::BufferCopy copyRegion;
    copyRegion.size = bufferSize;
    cmd.copyBuffer(stagingBuffer, indexBuffer, copyRegion);
    vulkan->end_one_time_graphics_command(cmd, "copy point index buffer", true, submit_immediately);

    device.destroyBuffer(stagingBuffer);
    device.freeMemory(stagingBufferMemory);
}

void Mesh::createNormalBuffer(bool allow_edits, bool submit_immediately)
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    vk::DeviceSize bufferSize = normals.size() * sizeof(glm::vec3);
    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    /* Map the memory to a pointer on the host */
    void *data = device.mapMemory(stagingBufferMemory, 0, bufferSize, vk::MemoryMapFlags());

    /* Copy over our normal data, then unmap */
    memcpy(data, normals.data(), (size_t)bufferSize);
    device.unmapMemory(stagingBufferMemory);

    vk::MemoryPropertyFlags memoryProperties;
    if (!allowEdits) memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    else {
        memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible;
        memoryProperties |= vk::MemoryPropertyFlagBits::eHostCoherent;
    }
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, memoryProperties, normalBuffer, normalBufferMemory);
    
    auto cmd = vulkan->begin_one_time_graphics_command();
    vk::BufferCopy copyRegion;
    copyRegion.size = bufferSize;
    cmd.copyBuffer(stagingBuffer, normalBuffer, copyRegion);
    vulkan->end_one_time_graphics_command(cmd, "copy point normal buffer", true, submit_immediately);

    /* Clean up the staging buffer */
    device.destroyBuffer(stagingBuffer);
    device.freeMemory(stagingBufferMemory);
}

void Mesh::createTexCoordBuffer(bool allow_edits, bool submit_immediately)
{
    auto vulkan = Libraries::Vulkan::Get();
    auto device = vulkan->get_device();

    vk::DeviceSize bufferSize = texcoords.size() * sizeof(glm::vec2);
    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    /* Map the memory to a pointer on the host */
    void *data = device.mapMemory(stagingBufferMemory, 0, bufferSize, vk::MemoryMapFlags());

    /* Copy over our normal data, then unmap */
    memcpy(data, texcoords.data(), (size_t)bufferSize);
    device.unmapMemory(stagingBufferMemory);

    vk::MemoryPropertyFlags memoryProperties;
    if (!allowEdits) memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    else {
        memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible;
        memoryProperties |= vk::MemoryPropertyFlagBits::eHostCoherent;
    }
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, memoryProperties, texCoordBuffer, texCoordBufferMemory);
    
    auto cmd = vulkan->begin_one_time_graphics_command();
    vk::BufferCopy copyRegion;
    copyRegion.size = bufferSize;
    cmd.copyBuffer(stagingBuffer, texCoordBuffer, copyRegion);
    vulkan->end_one_time_graphics_command(cmd, "copy point texcoord buffer", true, submit_immediately);

    /* Clean up the staging buffer */
    device.destroyBuffer(stagingBuffer);
    device.freeMemory(stagingBufferMemory);
}
