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

namespace std
{
template <>
struct hash<Mesh::Vertex>
{
    size_t operator()(const Mesh::Vertex &k) const
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
    auto cube = CreateCube("DefaultCube");
    auto sphere = CreateSphere("DefaultSphere");
    auto plane = CreatePlane("DefaultPlane");
    
    if ((!plane) || (!sphere) || (!plane))
        throw std::runtime_error(std::string("Error: Unable to initialize Mesh. \
            Does the given resource directory include the required default meshes?"));
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

    std::vector<Mesh::Vertex> vertices;

    /* If the mesh has a set of shapes, merge them all into one */
    if (shapes.size() > 0)
    {
        for (const auto &shape : shapes)
        {
            for (const auto &index : shape.mesh.indices)
            {
                Mesh::Vertex vertex = Mesh::Vertex();
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
            Mesh::Vertex v = Mesh::Vertex();
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
    std::unordered_map<Mesh::Vertex, uint32_t> uniqueVertexMap = {};
    std::vector<Mesh::Vertex> uniqueVertices;
    for (int i = 0; i < vertices.size(); ++i)
    {
        Mesh::Vertex vertex = vertices[i];
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

    std::vector<Mesh::Vertex> vertices;

    /* STLs only have points and face normals, so generate colors and UVs */
    for (uint32_t i = 0; i < p.size() / 3; ++i) {
        Mesh::Vertex vertex = Mesh::Vertex();
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
    std::unordered_map<Mesh::Vertex, uint32_t> uniqueVertexMap = {};
    std::vector<Mesh::Vertex> uniqueVertices;
    for (int i = 0; i < vertices.size(); ++i)
    {
        Mesh::Vertex vertex = vertices[i];
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

    std::vector<Mesh::Vertex> vertices;

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
                
                Mesh::Vertex vertex = Mesh::Vertex();
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
    std::unordered_map<Mesh::Vertex, uint32_t> uniqueVertexMap = {};
    std::vector<Mesh::Vertex> uniqueVertices;
    for (int i = 0; i < vertices.size(); ++i)
    {
        Mesh::Vertex vertex = vertices[i];
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
    bool allow_edits, bool submit_immediately
)
{
    allowEdits = allow_edits;
    bool reading_normals = normals_.size() > 0;
    bool reading_colors = colors_.size() > 0;
    bool reading_texcoords = texcoords_.size() > 0;

    if (points_.size() == 0)
        throw std::runtime_error( std::string("Error, no points supplied. "));

    if ((points_.size() % 3) != 0)
        throw std::runtime_error( std::string("Error: length of points is not a multiple of 3."));
    
    if (reading_normals && (normals_.size() != points_.size()))
        throw std::runtime_error( std::string("Error, length mismatch. Total normals: " + std::to_string(normals_.size()) + " does not equal total points: " + std::to_string(points_.size())));

    if (reading_colors && (colors_.size() != points_.size()))
        throw std::runtime_error( std::string("Error, length mismatch. Total colors: " + std::to_string(colors_.size()) + " does not equal total points: " + std::to_string(points_.size())));
        
    if (reading_texcoords && (texcoords_.size() != points_.size()))
        throw std::runtime_error( std::string("Error, length mismatch. Total texcoords: " + std::to_string(texcoords_.size()) + " does not equal total points: " + std::to_string(points_.size())));
        
    std::vector<Mesh::Vertex> vertices;

    /* For each vertex */
    for (int i = 0; i < points_.size(); ++ i) {
        Mesh::Vertex vertex = Mesh::Vertex();
        vertex.point = points_[i];
        if (reading_normals) vertex.normal = normals_[i];
        if (reading_colors) vertex.color = colors_[i];
        if (reading_texcoords) vertex.texcoord = texcoords_[i];        
        vertices.push_back(vertex);
    }

    /* Eliminate duplicate points */
    std::unordered_map<Mesh::Vertex, uint32_t> uniqueVertexMap = {};
    std::vector<Mesh::Vertex> uniqueVertices;
    for (int i = 0; i < vertices.size(); ++i)
    {
        Mesh::Vertex vertex = vertices[i];
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
    if (!allowEdits)
        throw std::runtime_error("Error: editing this component is not allowed. \
            Edits can be enabled during creation.");
    throw std::runtime_error("Error: Not yet implemented");
}

void Mesh::edit_normal(uint32_t index, glm::vec3 new_normal)
{
    if (!allowEdits)
        throw std::runtime_error("Error: editing this component is not allowed. \
            Edits can be enabled during creation.");
    throw std::runtime_error("Error: Not yet implemented");
}

void Mesh::edit_normals(uint32_t index, std::vector<glm::vec3> new_normals)
{
    if (!allowEdits)
        throw std::runtime_error("Error: editing this component is not allowed. \
            Edits can be enabled during creation.");
    throw std::runtime_error("Error: Not yet implemented");
}

void Mesh::edit_vertex_color(uint32_t index, glm::vec4 new_color)
{
    if (!allowEdits)
        throw std::runtime_error("Error: editing this component is not allowed. \
            Edits can be enabled during creation.");
    throw std::runtime_error("Error: Not yet implemented");
}

void Mesh::edit_vertex_colors(uint32_t index, std::vector<glm::vec4> new_colors)
{
    if (!allowEdits)
        throw std::runtime_error("Error: editing this component is not allowed. \
            Edits can be enabled during creation.");
    throw std::runtime_error("Error: Not yet implemented");
}

void Mesh::edit_texture_coordinate(uint32_t index, glm::vec2 new_texcoord)
{
    if (!allowEdits)
        throw std::runtime_error("Error: editing this component is not allowed. \
            Edits can be enabled during creation.");
    throw std::runtime_error("Error: Not yet implemented");
}

void Mesh::edit_texture_coordinates(uint32_t index, std::vector<glm::vec2> new_texcoords)
{
    if (!allowEdits)
        throw std::runtime_error("Error: editing this component is not allowed. \
            Edits can be enabled during creation.");
    throw std::runtime_error("Error: Not yet implemented");
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

Mesh* Mesh::CreateCube(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    mesh->make_cube(allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreatePlane(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    mesh->make_plane(allow_edits, submit_immediately);
    return mesh;
}

Mesh* Mesh::CreateSphere(std::string name, bool allow_edits, bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    mesh->make_sphere(allow_edits, submit_immediately);
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

Mesh* Mesh::CreateFromRaw(
    std::string name,
    std::vector<glm::vec3> points, 
    std::vector<glm::vec3> normals, 
    std::vector<glm::vec4> colors, 
    std::vector<glm::vec2> texcoords, 
    bool allow_edits, 
    bool submit_immediately)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    mesh->load_raw(points, normals, colors, texcoords, allow_edits, submit_immediately);
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

void Mesh::make_cube(bool allow_edits, bool submit_immediately)
{
    allowEdits = allow_edits;
    points.assign({
        {1.0, -1.0, -1.0},  {1.0, -1.0, -1.0},  {1.0, -1.0, -1.0}, 
        {1.0, -1.0, 1.0},   {1.0, -1.0, 1.0},   {1.0, -1.0, 1.0},
        {-1.0, -1.0, 1.0},  {-1.0, -1.0, 1.0},  {-1.0, -1.0, 1.0}, 
        {-1.0, -1.0, -1.0}, {-1.0, -1.0, -1.0}, {-1.0, -1.0, -1.0},
        {1.0, 1.0, -1.0},   {1.0, 1.0, -1.0},   {1.0, 1.0, -1.0}, 
        {1.0, 1.0, 1.0},    {1.0, 1.0, 1.0},    {1.0, 1.0, 1.0},
        {-1.0, 1.0, 1.0},   {-1.0, 1.0, 1.0},   {-1.0, 1.0, 1.0}, 
        {-1.0, 1.0, -1.0},  {-1.0, 1.0, -1.0},  {-1.0, 1.0, -1.0}});

    colors.assign({
        {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0},
        {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0},
        {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0},
        {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0},
        {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0},
        {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0},
        {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0},
        {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0},
        {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0}});

    normals.assign({
        {1.0, -1.0, -1.0},  {1.0, -1.0, -1.0},  {1.0, -1.0, -1.0}, 
        {1.0, -1.0, 1.0},   {1.0, -1.0, 1.0},   {1.0, -1.0, 1.0},
        {-1.0, -1.0, 1.0},  {-1.0, -1.0, 1.0},  {-1.0, -1.0, 1.0}, 
        {-1.0, -1.0, -1.0}, {-1.0, -1.0, -1.0}, {-1.0, -1.0, -1.0},
        {1.0, 1.0, -1.0},   {1.0, 1.0, -1.0},   {1.0, 1.0, -1.0}, 
        {1.0, 1.0, 1.0},    {1.0, 1.0, 1.0},    {1.0, 1.0, 1.0},
        {-1.0, 1.0, 1.0},   {-1.0, 1.0, 1.0},   {-1.0, 1.0, 1.0}, 
        {-1.0, 1.0, -1.0},  {-1.0, 1.0, -1.0},  {-1.0, 1.0, -1.0}});

    texcoords.assign({
        {0.000199999995f,0.999800026f},    {0.000199999995f,0.666467011f}, {0.666467011f,0.333133996f},
        {0.333133996f,0.999800026f},       {0.333133996f,0.666467011f},    {0.333532989f,0.666467011f},
        {0.333133996f,0.666867018f},       {0.666467011f,0.666467011f},    {0.999800026f,0.000199999995f},
        {0.000199999995f,0.666867018f},    {0.999800026f,0.333133996f},    {0.666467011f,0.000199999995f},
        {0.000199999995f,0.333133996f},    {0.000199999995f,0.333532989f}, {0.333532989f,0.333133996f},
        {0.333133996f,0.333133996f},       {0.333133996f,0.333532989f},    {0.333532989f,0.333532989f},
        {0.333133996f,0.000199999995f},    {0.666467011f,0.333532989f},    {0.666866004f,0.000199999995f},
        {0.000199999995f,0.000199999995f}, {0.666866004f,0.333133012f},    {0.333532989f,0.000199999995f}
    });

    indices.assign({0,3,6,0,6,9,12,21,18,12,18,15,1,13,16,1,16,4,5,17,19,5,19,7,8,20,22,8,22,10,14,2,11,14,11,23,});
    compute_centroid();
    createPointBuffer(allow_edits, submit_immediately);
    createColorBuffer(allow_edits, submit_immediately);
    createNormalBuffer(allow_edits, submit_immediately);
    createTexCoordBuffer(allow_edits, submit_immediately);
    createIndexBuffer(allow_edits, submit_immediately);
}

void Mesh::make_plane(bool allow_edits, bool submit_immediately)
{
    allowEdits = allow_edits;
    points.assign({
        {-1.f, -1.f, 0.f},
        {1.f, -1.f, 0.f},
        {-1.f, 1.f, 0.f},
        {1.f, 1.f, 0.f}});

    colors.assign({
        {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0}, {1.0,1.0,1.0,1.0}
    });

    normals.assign({
        {0.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
        {0.f, 0.f, 1.f}
    });

    texcoords.assign({
        {0.f, 1.f},
        {1.f, 1.f},
        {0.f, 0.f},
        {1.f, 0.f}
    });

    indices.assign({0,1,3,0,3,2});

    compute_centroid();
    createPointBuffer(allow_edits, submit_immediately);
    createColorBuffer(allow_edits, submit_immediately);
    createNormalBuffer(allow_edits, submit_immediately);
    createTexCoordBuffer(allow_edits, submit_immediately);
    createIndexBuffer(allow_edits, submit_immediately);
}

void Mesh::make_sphere(bool allow_edits, bool submit_immediately)
{
    allowEdits = allow_edits;
    points.assign({
        { 0.0 ,  0.0 ,  -1.0 }, { 0.72 ,  -0.53 ,  -0.45 }, { 0.72 ,  -0.53 ,  -0.45 }, 
        { -0.28 ,  -0.85 ,  -0.45 }, { -0.89 ,  0.0 ,  -0.45 }, { -0.28 ,  0.85 ,  -0.45 }, 
        { 0.72 ,  0.53 ,  -0.45 }, { 0.72 ,  0.53 ,  -0.45 }, { 0.28 ,  -0.85 ,  0.45 }, 
        { -0.72 ,  -0.53 ,  0.45 }, { -0.72 ,  -0.53 ,  0.45 }, { -0.72 ,  0.53 ,  0.45 }, 
        { -0.72 ,  0.53 ,  0.45 }, { 0.28 ,  0.85 ,  0.45 }, { 0.89 ,  0.0 ,  0.45 }, 
        { 0.0 ,  0.0 ,  1.0 }, { -0.23 ,  -0.72 ,  -0.66 }, { -0.23 ,  -0.72 ,  -0.66 }, 
        { -0.16 ,  -0.5 ,  -0.85 }, { -0.08 ,  -0.24 ,  -0.97 }, { 0.2 ,  -0.15 ,  -0.97 }, 
        { 0.43 ,  -0.31 ,  -0.85 }, { 0.61 ,  -0.44 ,  -0.66 }, { 0.61 ,  -0.44 ,  -0.66 }, 
        { 0.53 ,  -0.68 ,  -0.5 }, { 0.53 ,  -0.68 ,  -0.5 }, { 0.53 ,  -0.68 ,  -0.5 }, 
        { 0.26 ,  -0.81 ,  -0.53 }, { -0.03 ,  -0.86 ,  -0.5 }, { 0.81 ,  0.3 ,  -0.5 }, 
        { 0.85 ,  0.0 ,  -0.53 }, { 0.81 ,  -0.3 ,  -0.5 }, { 0.2 ,  0.15 ,  -0.97 }, 
        { 0.43 ,  0.31 ,  -0.85 }, { 0.61 ,  0.44 ,  -0.66 }, { 0.61 ,  0.44 ,  -0.66 }, 
        { -0.75 ,  0.0 ,  -0.66 }, { -0.75 ,  0.0 ,  -0.66 }, { -0.53 ,  0.0 ,  -0.85 }, 
        { -0.25 ,  0.0 ,  -0.97 }, { -0.48 ,  -0.72 ,  -0.5 }, { -0.48 ,  -0.72 ,  -0.5 }, 
        { -0.69 ,  -0.5 ,  -0.53 }, { -0.69 ,  -0.5 ,  -0.53 }, { -0.69 ,  -0.5 ,  -0.53 }, 
        { -0.83 ,  -0.24 ,  -0.5 }, { -0.23 ,  0.72 ,  -0.66 }, { -0.23 ,  0.72 ,  -0.66 }, 
        { -0.16 ,  0.5 ,  -0.85 }, { -0.08 ,  0.24 ,  -0.97 }, { -0.83 ,  0.24 ,  -0.5 }, 
        { -0.69 ,  0.5 ,  -0.53 }, { -0.69 ,  0.5 ,  -0.53 }, { -0.69 ,  0.5 ,  -0.53 }, 
        { -0.48 ,  0.72 ,  -0.5 }, { -0.48 ,  0.72 ,  -0.5 }, { -0.03 ,  0.86 ,  -0.5 }, 
        { 0.26 ,  0.81 ,  -0.53 }, { 0.53 ,  0.68 ,  -0.5 }, { 0.53 ,  0.68 ,  -0.5 }, 
        { 0.53 ,  0.68 ,  -0.5 }, { 0.96 ,  -0.15 ,  0.25 }, { 0.95 ,  -0.31 ,  0.0 }, 
        { 0.86 ,  -0.44 ,  -0.25 }, { 0.86 ,  0.44 ,  -0.25 }, { 0.95 ,  0.31 ,  0.0 }, 
        { 0.96 ,  0.15 ,  0.25 }, { 0.16 ,  -0.96 ,  0.25 }, { 0.0 ,  -1.0 ,  0.0 }, 
        { -0.16 ,  -0.96 ,  -0.25 }, { 0.69 ,  -0.68 ,  -0.25 }, { 0.69 ,  -0.68 ,  -0.25 }, 
        { 0.59 ,  -0.81 ,  0.0 }, { 0.59 ,  -0.81 ,  0.0 }, { 0.44 ,  -0.86 ,  0.25 }, 
        { -0.86 ,  -0.44 ,  0.25 }, { -0.95 ,  -0.31 ,  0.0 }, { -0.96 ,  -0.15 ,  -0.25 }, 
        { -0.44 ,  -0.86 ,  -0.25 }, { -0.59 ,  -0.81 ,  0.0 }, { -0.59 ,  -0.81 ,  0.0 }, 
        { -0.69 ,  -0.68 ,  0.25 }, { -0.69 ,  -0.68 ,  0.25 }, { -0.69 ,  0.68 ,  0.25 }, 
        { -0.69 ,  0.68 ,  0.25 }, { -0.59 ,  0.81 ,  0.0 }, { -0.59 ,  0.81 ,  0.0 }, 
        { -0.44 ,  0.86 ,  -0.25 }, { -0.96 ,  0.15 ,  -0.25 }, { -0.95 ,  0.31 ,  0.0 }, 
        { -0.86 ,  0.44 ,  0.25 }, { 0.44 ,  0.86 ,  0.25 }, { 0.59 ,  0.81 ,  0.0 }, 
        { 0.59 ,  0.81 ,  0.0 }, { 0.69 ,  0.68 ,  -0.25 }, { 0.69 ,  0.68 ,  -0.25 }, 
        { -0.16 ,  0.96 ,  -0.25 }, { 0.0 ,  1.0 ,  0.0 }, { 0.16 ,  0.96 ,  0.25 }, 
        { 0.83 ,  -0.24 ,  0.5 }, { 0.69 ,  -0.5 ,  0.53 }, { 0.69 ,  -0.5 ,  0.53 }, 
        { 0.69 ,  -0.5 ,  0.53 }, { 0.48 ,  -0.72 ,  0.5 }, { 0.48 ,  -0.72 ,  0.5 }, 
        { 0.03 ,  -0.86 ,  0.5 }, { -0.26 ,  -0.81 ,  0.53 }, { -0.53 ,  -0.68 ,  0.5 }, 
        { -0.53 ,  -0.68 ,  0.5 }, { -0.53 ,  -0.68 ,  0.5 }, { -0.81 ,  -0.3 ,  0.5 }, 
        { -0.85 ,  0.0 ,  0.53 }, { -0.81 ,  0.3 ,  0.5 }, { -0.53 ,  0.68 ,  0.5 }, 
        { -0.53 ,  0.68 ,  0.5 }, { -0.53 ,  0.68 ,  0.5 }, { -0.26 ,  0.81 ,  0.53 }, 
        { 0.03 ,  0.86 ,  0.5 }, { 0.48 ,  0.72 ,  0.5 }, { 0.48 ,  0.72 ,  0.5 }, 
        { 0.69 ,  0.5 ,  0.53 }, { 0.69 ,  0.5 ,  0.53 }, { 0.69 ,  0.5 ,  0.53 }, 
        { 0.83 ,  0.24 ,  0.5 }, { 0.08 ,  -0.24 ,  0.97 }, { 0.16 ,  -0.5 ,  0.85 }, 
        { 0.23 ,  -0.72 ,  0.66 }, { 0.23 ,  -0.72 ,  0.66 }, { 0.75 ,  0.0 ,  0.66 }, 
        { 0.75 ,  0.0 ,  0.66 }, { 0.53 ,  0.0 ,  0.85 }, { 0.25 ,  0.0 ,  0.97 }, 
        { -0.2 ,  -0.15 ,  0.97 }, { -0.43 ,  -0.31 ,  0.85 }, { -0.61 ,  -0.44 ,  0.66 }, 
        { -0.61 ,  -0.44 ,  0.66 }, { -0.2 ,  0.15 ,  0.97 }, { -0.43 ,  0.31 ,  0.85 }, 
        { -0.61 ,  0.44 ,  0.66 }, { -0.61 ,  0.44 ,  0.66 }, { 0.08 ,  0.24 ,  0.97 }, 
        { 0.16 ,  0.5 ,  0.85 }, { 0.23 ,  0.72 ,  0.66 }, { 0.23 ,  0.72 ,  0.66 }, 
        { 0.36 ,  0.26 ,  0.89 }, { 0.64 ,  0.26 ,  0.72 }, { 0.64 ,  0.26 ,  0.72 }, 
        { 0.45 ,  0.53 ,  0.72 }, { 0.45 ,  0.53 ,  0.72 }, { -0.14 ,  0.43 ,  0.89 }, 
        { -0.05 ,  0.69 ,  0.72 }, { -0.05 ,  0.69 ,  0.72 }, { -0.36 ,  0.59 ,  0.72 }, 
        { -0.36 ,  0.59 ,  0.72 }, { -0.45 ,  0.0 ,  0.89 }, { -0.67 ,  0.16 ,  0.72 }, 
        { -0.67 ,  0.16 ,  0.72 }, { -0.67 ,  -0.16 ,  0.72 }, { -0.67 ,  -0.16 ,  0.72 }, 
        { -0.14 ,  -0.43 ,  0.89 }, { -0.36 ,  -0.59 ,  0.72 }, { -0.36 ,  -0.59 ,  0.72 }, 
        { -0.05 ,  -0.69 ,  0.72 }, { -0.05 ,  -0.69 ,  0.72 }, { 0.36 ,  -0.26 ,  0.89 }, 
        { 0.45 ,  -0.53 ,  0.72 }, { 0.45 ,  -0.53 ,  0.72 }, { 0.64 ,  -0.26 ,  0.72 }, 
        { 0.64 ,  -0.26 ,  0.72 }, { 0.86 ,  0.43 ,  0.28 }, { 0.81 ,  0.59 ,  0.0 }, 
        { 0.67 ,  0.69 ,  0.28 }, { 0.67 ,  0.69 ,  0.28 }, { -0.14 ,  0.95 ,  0.28 }, 
        { -0.31 ,  0.95 ,  0.0 }, { -0.45 ,  0.85 ,  0.28 }, { -0.95 ,  0.16 ,  0.28 }, 
        { -1.0 ,  0.0 ,  0.0 }, { -0.95 ,  -0.16 ,  0.28 }, { -0.45 ,  -0.85 ,  0.28 }, 
        { -0.31 ,  -0.95 ,  -0.0 }, { -0.14 ,  -0.95 ,  0.28 }, { 0.67 ,  -0.69 ,  0.28 }, 
        { 0.67 ,  -0.69 ,  0.28 }, { 0.81 ,  -0.59 ,  -0.0 }, { 0.86 ,  -0.43 ,  0.28 }, 
        { 0.31 ,  0.95 ,  0.0 }, { 0.45 ,  0.85 ,  -0.28 }, { 0.14 ,  0.95 ,  -0.28 }, 
        { -0.81 ,  0.59 ,  0.0 }, { -0.67 ,  0.69 ,  -0.28 }, { -0.67 ,  0.69 ,  -0.28 }, 
        { -0.86 ,  0.43 ,  -0.28 }, { -0.81 ,  -0.59 ,  0.0 }, { -0.86 ,  -0.43 ,  -0.28 }, 
        { -0.67 ,  -0.69 ,  -0.28 }, { -0.67 ,  -0.69 ,  -0.28 }, { 0.31 ,  -0.95 ,  0.0 }, 
        { 0.14 ,  -0.95 ,  -0.28 }, { 0.45 ,  -0.85 ,  -0.28 }, { 1.0 ,  0.0 ,  0.0 }, 
        { 0.95 ,  -0.16 ,  -0.28 }, { 0.95 ,  0.16 ,  -0.28 }, { 0.36 ,  0.59 ,  -0.72 }, 
        { 0.36 ,  0.59 ,  -0.72 }, { 0.14 ,  0.43 ,  -0.89 }, { 0.05 ,  0.69 ,  -0.72 }, 
        { 0.05 ,  0.69 ,  -0.72 }, { -0.45 ,  0.53 ,  -0.72 }, { -0.45 ,  0.53 ,  -0.72 }, 
        { -0.36 ,  0.26 ,  -0.89 }, { -0.64 ,  0.26 ,  -0.72 }, { -0.64 ,  0.26 ,  -0.72 }, 
        { -0.64 ,  -0.26 ,  -0.72 }, { -0.64 ,  -0.26 ,  -0.72 }, { -0.36 ,  -0.26 ,  -0.89 }, 
        { -0.45 ,  -0.53 ,  -0.72 }, { -0.45 ,  -0.53 ,  -0.72 }, { 0.67 ,  0.16 ,  -0.72 }, 
        { 0.67 ,  0.16 ,  -0.72 }, { 0.67 ,  -0.16 ,  -0.72 }, { 0.67 ,  -0.16 ,  -0.72 }, 
        { 0.45 ,  -0.0 ,  -0.89 }, { 0.05 ,  -0.69 ,  -0.72 }, { 0.05 ,  -0.69 ,  -0.72 }, 
        { 0.14 ,  -0.43 ,  -0.89 }, { 0.36 ,  -0.59 ,  -0.72 }, { 0.36 ,  -0.59 ,  -0.72 }, 
    });

    colors.assign({
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
        { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, { 1.0 ,  1.0 ,  1.0 ,  1.0 }, 
    });

    normals.assign({
        { 0.0 ,  0.0 ,  -1.0 }, { 0.72 ,  -0.53 ,  -0.45 }, { 0.72 ,  -0.53 ,  -0.45 }, 
        { -0.28 ,  -0.85 ,  -0.45 }, { -0.89 ,  0.0 ,  -0.45 }, { -0.28 ,  0.85 ,  -0.45 }, 
        { 0.72 ,  0.53 ,  -0.45 }, { 0.72 ,  0.53 ,  -0.45 }, { 0.28 ,  -0.85 ,  0.45 }, 
        { -0.72 ,  -0.53 ,  0.45 }, { -0.72 ,  -0.53 ,  0.45 }, { -0.72 ,  0.53 ,  0.45 }, 
        { -0.72 ,  0.53 ,  0.45 }, { 0.28 ,  0.85 ,  0.45 }, { 0.89 ,  0.0 ,  0.45 }, 
        { 0.0 ,  0.0 ,  1.0 }, { -0.23 ,  -0.71 ,  -0.67 }, { -0.23 ,  -0.71 ,  -0.67 }, 
        { -0.16 ,  -0.5 ,  -0.85 }, { -0.08 ,  -0.25 ,  -0.97 }, { 0.21 ,  -0.15 ,  -0.97 }, 
        { 0.43 ,  -0.31 ,  -0.85 }, { 0.6 ,  -0.44 ,  -0.67 }, { 0.6 ,  -0.44 ,  -0.67 }, 
        { 0.52 ,  -0.69 ,  -0.5 }, { 0.52 ,  -0.69 ,  -0.5 }, { 0.52 ,  -0.69 ,  -0.5 }, 
        { 0.26 ,  -0.81 ,  -0.53 }, { -0.02 ,  -0.86 ,  -0.5 }, { 0.82 ,  0.29 ,  -0.5 }, 
        { 0.85 ,  0.0 ,  -0.53 }, { 0.82 ,  -0.29 ,  -0.5 }, { 0.21 ,  0.15 ,  -0.97 }, 
        { 0.43 ,  0.31 ,  -0.85 }, { 0.6 ,  0.44 ,  -0.67 }, { 0.6 ,  0.44 ,  -0.67 }, 
        { -0.75 ,  0.0 ,  -0.67 }, { -0.75 ,  0.0 ,  -0.67 }, { -0.53 ,  0.0 ,  -0.85 }, 
        { -0.26 ,  0.0 ,  -0.97 }, { -0.49 ,  -0.71 ,  -0.5 }, { -0.49 ,  -0.71 ,  -0.5 }, 
        { -0.69 ,  -0.5 ,  -0.53 }, { -0.69 ,  -0.5 ,  -0.53 }, { -0.69 ,  -0.5 ,  -0.53 }, 
        { -0.83 ,  -0.25 ,  -0.5 }, { -0.23 ,  0.71 ,  -0.67 }, { -0.23 ,  0.71 ,  -0.67 }, 
        { -0.16 ,  0.5 ,  -0.85 }, { -0.08 ,  0.25 ,  -0.97 }, { -0.83 ,  0.25 ,  -0.5 }, 
        { -0.69 ,  0.5 ,  -0.53 }, { -0.69 ,  0.5 ,  -0.53 }, { -0.69 ,  0.5 ,  -0.53 }, 
        { -0.49 ,  0.71 ,  -0.5 }, { -0.49 ,  0.71 ,  -0.5 }, { -0.02 ,  0.86 ,  -0.5 }, 
        { 0.26 ,  0.81 ,  -0.53 }, { 0.52 ,  0.69 ,  -0.5 }, { 0.52 ,  0.69 ,  -0.5 }, 
        { 0.52 ,  0.69 ,  -0.5 }, { 0.96 ,  -0.15 ,  0.24 }, { 0.95 ,  -0.31 ,  0.0 }, 
        { 0.87 ,  -0.44 ,  -0.24 }, { 0.87 ,  0.44 ,  -0.24 }, { 0.95 ,  0.31 ,  0.0 }, 
        { 0.96 ,  0.15 ,  0.24 }, { 0.15 ,  -0.96 ,  0.24 }, { 0.0 ,  -1.0 ,  0.0 }, 
        { -0.15 ,  -0.96 ,  -0.24 }, { 0.68 ,  -0.69 ,  -0.24 }, { 0.68 ,  -0.69 ,  -0.24 }, 
        { 0.59 ,  -0.81 ,  0.0 }, { 0.59 ,  -0.81 ,  0.0 }, { 0.44 ,  -0.86 ,  0.24 }, 
        { -0.87 ,  -0.44 ,  0.24 }, { -0.95 ,  -0.31 ,  0.0 }, { -0.96 ,  -0.15 ,  -0.24 }, 
        { -0.44 ,  -0.86 ,  -0.24 }, { -0.59 ,  -0.81 ,  0.0 }, { -0.59 ,  -0.81 ,  0.0 }, 
        { -0.68 ,  -0.69 ,  0.24 }, { -0.68 ,  -0.69 ,  0.24 }, { -0.68 ,  0.69 ,  0.24 }, 
        { -0.68 ,  0.69 ,  0.24 }, { -0.59 ,  0.81 ,  0.0 }, { -0.59 ,  0.81 ,  0.0 }, 
        { -0.44 ,  0.86 ,  -0.24 }, { -0.96 ,  0.15 ,  -0.24 }, { -0.95 ,  0.31 ,  0.0 }, 
        { -0.87 ,  0.44 ,  0.24 }, { 0.44 ,  0.86 ,  0.24 }, { 0.59 ,  0.81 ,  0.0 }, 
        { 0.59 ,  0.81 ,  0.0 }, { 0.68 ,  0.69 ,  -0.24 }, { 0.68 ,  0.69 ,  -0.24 }, 
        { -0.15 ,  0.96 ,  -0.24 }, { 0.0 ,  1.0 ,  0.0 }, { 0.15 ,  0.96 ,  0.24 }, 
        { 0.83 ,  -0.25 ,  0.5 }, { 0.69 ,  -0.5 ,  0.53 }, { 0.69 ,  -0.5 ,  0.53 }, 
        { 0.69 ,  -0.5 ,  0.53 }, { 0.49 ,  -0.71 ,  0.5 }, { 0.49 ,  -0.71 ,  0.5 }, 
        { 0.02 ,  -0.86 ,  0.5 }, { -0.26 ,  -0.81 ,  0.53 }, { -0.52 ,  -0.69 ,  0.5 }, 
        { -0.52 ,  -0.69 ,  0.5 }, { -0.52 ,  -0.69 ,  0.5 }, { -0.82 ,  -0.29 ,  0.5 }, 
        { -0.85 ,  0.0 ,  0.53 }, { -0.82 ,  0.29 ,  0.5 }, { -0.52 ,  0.69 ,  0.5 }, 
        { -0.52 ,  0.69 ,  0.5 }, { -0.52 ,  0.69 ,  0.5 }, { -0.26 ,  0.81 ,  0.53 }, 
        { 0.02 ,  0.86 ,  0.5 }, { 0.49 ,  0.71 ,  0.5 }, { 0.49 ,  0.71 ,  0.5 }, 
        { 0.69 ,  0.5 ,  0.53 }, { 0.69 ,  0.5 ,  0.53 }, { 0.69 ,  0.5 ,  0.53 }, 
        { 0.83 ,  0.25 ,  0.5 }, { 0.08 ,  -0.25 ,  0.97 }, { 0.16 ,  -0.5 ,  0.85 }, 
        { 0.23 ,  -0.71 ,  0.67 }, { 0.23 ,  -0.71 ,  0.67 }, { 0.75 ,  0.0 ,  0.67 }, 
        { 0.75 ,  0.0 ,  0.67 }, { 0.53 ,  0.0 ,  0.85 }, { 0.26 ,  0.0 ,  0.97 }, 
        { -0.21 ,  -0.15 ,  0.97 }, { -0.43 ,  -0.31 ,  0.85 }, { -0.6 ,  -0.44 ,  0.67 }, 
        { -0.6 ,  -0.44 ,  0.67 }, { -0.21 ,  0.15 ,  0.97 }, { -0.43 ,  0.31 ,  0.85 }, 
        { -0.6 ,  0.44 ,  0.67 }, { -0.6 ,  0.44 ,  0.67 }, { 0.08 ,  0.25 ,  0.97 }, 
        { 0.16 ,  0.5 ,  0.85 }, { 0.23 ,  0.71 ,  0.67 }, { 0.23 ,  0.71 ,  0.67 }, 
        { 0.37 ,  0.27 ,  0.89 }, { 0.63 ,  0.27 ,  0.73 }, { 0.63 ,  0.27 ,  0.73 }, 
        { 0.45 ,  0.52 ,  0.73 }, { 0.45 ,  0.52 ,  0.73 }, { -0.14 ,  0.43 ,  0.89 }, 
        { -0.06 ,  0.68 ,  0.73 }, { -0.06 ,  0.68 ,  0.73 }, { -0.35 ,  0.59 ,  0.73 }, 
        { -0.35 ,  0.59 ,  0.73 }, { -0.46 ,  0.0 ,  0.89 }, { -0.67 ,  0.15 ,  0.73 }, 
        { -0.67 ,  0.15 ,  0.73 }, { -0.67 ,  -0.15 ,  0.73 }, { -0.67 ,  -0.15 ,  0.73 }, 
        { -0.14 ,  -0.43 ,  0.89 }, { -0.35 ,  -0.59 ,  0.73 }, { -0.35 ,  -0.59 ,  0.73 }, 
        { -0.06 ,  -0.68 ,  0.73 }, { -0.06 ,  -0.68 ,  0.73 }, { 0.37 ,  -0.27 ,  0.89 }, 
        { 0.45 ,  -0.52 ,  0.73 }, { 0.45 ,  -0.52 ,  0.73 }, { 0.63 ,  -0.27 ,  0.73 }, 
        { 0.63 ,  -0.27 ,  0.73 }, { 0.86 ,  0.43 ,  0.27 }, { 0.81 ,  0.59 ,  0.01 }, 
        { 0.68 ,  0.68 ,  0.27 }, { 0.68 ,  0.68 ,  0.27 }, { -0.15 ,  0.95 ,  0.27 }, 
        { -0.31 ,  0.95 ,  0.01 }, { -0.44 ,  0.86 ,  0.27 }, { -0.95 ,  0.15 ,  0.27 }, 
        { -1.0 ,  0.0 ,  0.01 }, { -0.95 ,  -0.15 ,  0.27 }, { -0.44 ,  -0.86 ,  0.27 }, 
        { -0.31 ,  -0.95 ,  0.01 }, { -0.15 ,  -0.95 ,  0.27 }, { 0.68 ,  -0.68 ,  0.27 }, 
        { 0.68 ,  -0.68 ,  0.27 }, { 0.81 ,  -0.59 ,  0.01 }, { 0.86 ,  -0.43 ,  0.27 }, 
        { 0.31 ,  0.95 ,  -0.01 }, { 0.44 ,  0.86 ,  -0.27 }, { 0.15 ,  0.95 ,  -0.27 }, 
        { -0.81 ,  0.59 ,  -0.01 }, { -0.68 ,  0.68 ,  -0.27 }, { -0.68 ,  0.68 ,  -0.27 }, 
        { -0.86 ,  0.43 ,  -0.27 }, { -0.81 ,  -0.59 ,  -0.01 }, { -0.86 ,  -0.43 ,  -0.27 }, 
        { -0.68 ,  -0.68 ,  -0.27 }, { -0.68 ,  -0.68 ,  -0.27 }, { 0.31 ,  -0.95 ,  -0.01 }, 
        { 0.15 ,  -0.95 ,  -0.27 }, { 0.44 ,  -0.86 ,  -0.27 }, { 1.0 ,  0.0 ,  -0.01 }, 
        { 0.95 ,  -0.15 ,  -0.27 }, { 0.95 ,  0.15 ,  -0.27 }, { 0.35 ,  0.59 ,  -0.73 }, 
        { 0.35 ,  0.59 ,  -0.73 }, { 0.14 ,  0.43 ,  -0.89 }, { 0.06 ,  0.68 ,  -0.73 }, 
        { 0.06 ,  0.68 ,  -0.73 }, { -0.45 ,  0.52 ,  -0.73 }, { -0.45 ,  0.52 ,  -0.73 }, 
        { -0.37 ,  0.27 ,  -0.89 }, { -0.63 ,  0.27 ,  -0.73 }, { -0.63 ,  0.27 ,  -0.73 }, 
        { -0.63 ,  -0.27 ,  -0.73 }, { -0.63 ,  -0.27 ,  -0.73 }, { -0.37 ,  -0.27 ,  -0.89 }, 
        { -0.45 ,  -0.52 ,  -0.73 }, { -0.45 ,  -0.52 ,  -0.73 }, { 0.67 ,  0.15 ,  -0.73 }, 
        { 0.67 ,  0.15 ,  -0.73 }, { 0.67 ,  -0.15 ,  -0.73 }, { 0.67 ,  -0.15 ,  -0.73 }, 
        { 0.46 ,  0.0 ,  -0.89 }, { 0.06 ,  -0.68 ,  -0.73 }, { 0.06 ,  -0.68 ,  -0.73 }, 
        { 0.14 ,  -0.43 ,  -0.89 }, { 0.35 ,  -0.59 ,  -0.73 }, { 0.35 ,  -0.59 ,  -0.73 }, 
    });

    texcoords.assign({
        { 0.83 ,  0.54 }, { 0.06 ,  0.31 }, { 0.65 ,  0.44 }, 
        { 0.42 ,  0.44 }, { 0.61 ,  0.19 }, { 0.1 ,  0.44 }, 
        { 0.33 ,  0.44 }, { 0.06 ,  0.07 }, { 0.56 ,  0.65 }, 
        { 0.4 ,  0.07 }, { 0.33 ,  0.65 }, { 0.4 ,  0.31 }, 
        { 0.0 ,  0.65 }, { 0.23 ,  0.65 }, { 0.27 ,  0.19 }, 
        { 0.84 ,  0.17 }, { 0.78 ,  0.37 }, { 0.43 ,  0.4 }, 
        { 0.79 ,  0.42 }, { 0.81 ,  0.49 }, { 0.88 ,  0.51 }, 
        { 0.93 ,  0.47 }, { 0.02 ,  0.29 }, { 0.97 ,  0.44 }, 
        { 0.61 ,  0.42 }, { 0.95 ,  0.38 }, { 0.05 ,  0.34 }, 
        { 0.55 ,  0.42 }, { 0.48 ,  0.43 }, { 0.05 ,  0.12 }, 
        { 0.05 ,  0.19 }, { 0.05 ,  0.26 }, { 0.88 ,  0.57 }, 
        { 0.93 ,  0.61 }, { 0.02 ,  0.08 }, { 0.97 ,  0.64 }, 
        { 0.65 ,  0.54 }, { 0.65 ,  0.19 }, { 0.71 ,  0.54 }, 
        { 0.77 ,  0.54 }, { 0.37 ,  0.43 }, { 0.72 ,  0.37 }, 
        { 0.62 ,  0.07 }, { 0.33 ,  0.43 }, { 0.67 ,  0.42 }, 
        { 0.62 ,  0.13 }, { 0.78 ,  0.71 }, { 0.1 ,  0.4 }, 
        { 0.79 ,  0.66 }, { 0.81 ,  0.6 }, { 0.62 ,  0.24 }, 
        { 0.0 ,  0.43 }, { 0.62 ,  0.3 }, { 0.67 ,  0.66 }, 
        { 0.05 ,  0.43 }, { 0.72 ,  0.71 }, { 0.15 ,  0.43 }, 
        { 0.22 ,  0.42 }, { 0.28 ,  0.42 }, { 0.05 ,  0.03 }, 
        { 0.95 ,  0.7 }, { 0.23 ,  0.22 }, { 0.17 ,  0.26 }, 
        { 0.11 ,  0.29 }, { 0.11 ,  0.08 }, { 0.17 ,  0.12 }, 
        { 0.23 ,  0.15 }, { 0.53 ,  0.6 }, { 0.49 ,  0.55 }, 
        { 0.45 ,  0.49 }, { 0.11 ,  0.34 }, { 0.65 ,  0.48 }, 
        { 0.63 ,  0.54 }, { 0.17 ,  0.37 }, { 0.59 ,  0.6 }, 
        { 0.44 ,  0.08 }, { 0.5 ,  0.12 }, { 0.56 ,  0.15 }, 
        { 0.39 ,  0.49 }, { 0.36 ,  0.55 }, { 0.5 ,  0.0 }, 
        { 0.33 ,  0.61 }, { 0.44 ,  0.03 }, { 0.01 ,  0.61 }, 
        { 0.44 ,  0.34 }, { 0.03 ,  0.55 }, { 0.5 ,  0.37 }, 
        { 0.06 ,  0.49 }, { 0.56 ,  0.22 }, { 0.5 ,  0.26 }, 
        { 0.44 ,  0.29 }, { 0.27 ,  0.6 }, { 0.17 ,  0.0 }, 
        { 0.3 ,  0.54 }, { 0.32 ,  0.48 }, { 0.11 ,  0.03 }, 
        { 0.13 ,  0.49 }, { 0.16 ,  0.55 }, { 0.2 ,  0.6 }, 
        { 0.28 ,  0.24 }, { 0.69 ,  0.28 }, { 0.29 ,  0.3 }, 
        { 0.65 ,  0.66 }, { 0.61 ,  0.66 }, { 0.73 ,  0.33 }, 
        { 0.5 ,  0.66 }, { 0.43 ,  0.67 }, { 0.39 ,  0.03 }, 
        { 0.97 ,  0.32 }, { 0.37 ,  0.67 }, { 0.39 ,  0.12 }, 
        { 0.38 ,  0.19 }, { 0.39 ,  0.26 }, { 0.04 ,  0.67 }, 
        { 0.97 ,  0.01 }, { 0.39 ,  0.34 }, { 0.11 ,  0.67 }, 
        { 0.17 ,  0.66 }, { 0.73 ,  0.0 }, { 0.28 ,  0.66 }, 
        { 0.29 ,  0.07 }, { 0.69 ,  0.05 }, { 0.33 ,  0.66 }, 
        { 0.28 ,  0.13 }, { 0.83 ,  0.22 }, { 0.81 ,  0.28 }, 
        { 0.55 ,  0.7 }, { 0.79 ,  0.33 }, { 0.32 ,  0.19 }, 
        { 0.67 ,  0.17 }, { 0.72 ,  0.17 }, { 0.79 ,  0.17 }, 
        { 0.89 ,  0.2 }, { 0.94 ,  0.24 }, { 0.35 ,  0.08 }, 
        { 0.99 ,  0.27 }, { 0.89 ,  0.13 }, { 0.94 ,  0.09 }, 
        { 0.35 ,  0.29 }, { 0.99 ,  0.06 }, { 0.83 ,  0.11 }, 
        { 0.81 ,  0.05 }, { 0.22 ,  0.7 }, { 0.79 ,  0.0 }, 
        { 0.76 ,  0.11 }, { 0.7 ,  0.11 }, { 0.33 ,  0.13 }, 
        { 0.74 ,  0.04 }, { 0.27 ,  0.71 }, { 0.88 ,  0.07 }, 
        { 0.86 ,  0.01 }, { 0.16 ,  0.71 }, { 0.93 ,  0.03 }, 
        { 0.09 ,  0.72 }, { 0.95 ,  0.17 }, { 1.0 ,  0.13 }, 
        { 0.34 ,  0.22 }, { 1.0 ,  0.2 }, { 0.34 ,  0.15 }, 
        { 0.88 ,  0.26 }, { 0.93 ,  0.3 }, { 0.41 ,  0.72 }, 
        { 0.86 ,  0.33 }, { 0.48 ,  0.71 }, { 0.76 ,  0.23 }, 
        { 0.74 ,  0.29 }, { 0.6 ,  0.71 }, { 0.7 ,  0.23 }, 
        { 0.33 ,  0.25 }, { 0.23 ,  0.09 }, { 0.17 ,  0.05 }, 
        { 0.23 ,  0.03 }, { 0.32 ,  0.6 }, { 0.13 ,  0.61 }, 
        { 0.09 ,  0.55 }, { 0.06 ,  0.61 }, { 0.44 ,  0.22 }, 
        { 0.5 ,  0.19 }, { 0.44 ,  0.15 }, { 0.39 ,  0.61 }, 
        { 0.42 ,  0.55 }, { 0.46 ,  0.61 }, { 0.65 ,  0.6 }, 
        { 0.23 ,  0.35 }, { 0.17 ,  0.32 }, { 0.23 ,  0.29 }, 
        { 0.24 ,  0.54 }, { 0.26 ,  0.48 }, { 0.19 ,  0.48 }, 
        { 0.5 ,  0.32 }, { 0.01 ,  0.49 }, { 0.57 ,  0.35 }, 
        { 0.57 ,  0.29 }, { 0.5 ,  0.05 }, { 0.57 ,  0.09 }, 
        { 0.57 ,  0.03 }, { 0.33 ,  0.49 }, { 0.56 ,  0.54 }, 
        { 0.52 ,  0.48 }, { 0.59 ,  0.48 }, { 0.17 ,  0.19 }, 
        { 0.1 ,  0.22 }, { 0.1 ,  0.15 }, { 0.91 ,  0.68 }, 
        { 0.24 ,  0.37 }, { 0.86 ,  0.64 }, { 0.84 ,  0.7 }, 
        { 0.17 ,  0.38 }, { 0.73 ,  0.66 }, { 0.05 ,  0.38 }, 
        { 0.75 ,  0.6 }, { 0.68 ,  0.6 }, { 0.67 ,  0.25 }, 
        { 0.68 ,  0.48 }, { 0.67 ,  0.13 }, { 0.75 ,  0.48 }, 
        { 0.73 ,  0.42 }, { 0.38 ,  0.38 }, { 0.0 ,  0.15 }, 
        { 0.98 ,  0.58 }, { 0.0 ,  0.22 }, { 0.98 ,  0.5 }, 
        { 0.93 ,  0.54 }, { 0.84 ,  0.38 }, { 0.5 ,  0.38 }, 
        { 0.86 ,  0.44 }, { 0.91 ,  0.4 }, { 0.57 ,  0.37 }, 
    });

    indices.assign({
        0 ,  20 ,  19 ,  1 ,  22 ,  31 ,  0 ,  19 ,  39 , 
        0 ,  39 ,  49 ,  0 ,  49 ,  32 ,  1 ,  31 ,  63 , 
        3 ,  28 ,  69 ,  4 ,  45 ,  77 ,  5 ,  54 ,  87 , 
        6 ,  58 ,  94 ,  1 ,  63 ,  70 ,  3 ,  69 ,  78 , 
        4 ,  77 ,  88 ,  5 ,  87 ,  96 ,  7 ,  95 ,  64 , 
        8 ,  103 ,  126 ,  9 ,  107 ,  134 ,  11 ,  112 ,  138 , 
        13 ,  117 ,  142 ,  14 ,  123 ,  128 ,  131 ,  140 ,  15 , 
        130 ,  144 ,  131 ,  129 ,  145 ,  130 ,  131 ,  144 ,  140 , 
        144 ,  141 ,  140 ,  130 ,  145 ,  144 ,  145 ,  147 ,  144 , 
        144 ,  147 ,  141 ,  147 ,  143 ,  141 ,  128 ,  123 ,  146 , 
        123 ,  120 ,  146 ,  145 ,  121 ,  147 ,  121 ,  118 ,  147 , 
        148 ,  119 ,  142 ,  119 ,  13 ,  142 ,  140 ,  136 ,  15 , 
        141 ,  149 ,  140 ,  143 ,  150 ,  141 ,  140 ,  149 ,  136 , 
        149 ,  137 ,  136 ,  141 ,  150 ,  149 ,  150 ,  152 ,  149 , 
        149 ,  152 ,  137 ,  152 ,  139 ,  137 ,  142 ,  117 ,  151 , 
        117 ,  116 ,  151 ,  151 ,  116 ,  153 ,  116 ,  113 ,  153 , 
        152 ,  114 ,  139 ,  115 ,  11 ,  138 ,  136 ,  132 ,  15 , 
        137 ,  154 ,  136 ,  139 ,  155 ,  137 ,  136 ,  154 ,  132 , 
        154 ,  133 ,  132 ,  137 ,  155 ,  154 ,  155 ,  157 ,  154 , 
        154 ,  157 ,  133 ,  157 ,  135 ,  133 ,  138 ,  112 ,  156 , 
        112 ,  111 ,  156 ,  156 ,  111 ,  158 ,  111 ,  110 ,  158 , 
        158 ,  110 ,  134 ,  110 ,  9 ,  134 ,  132 ,  124 ,  15 , 
        133 ,  159 ,  132 ,  135 ,  160 ,  133 ,  132 ,  159 ,  124 , 
        159 ,  125 ,  124 ,  133 ,  160 ,  159 ,  160 ,  162 ,  159 , 
        159 ,  162 ,  125 ,  162 ,  127 ,  125 ,  135 ,  108 ,  160 , 
        109 ,  106 ,  161 ,  161 ,  106 ,  163 ,  106 ,  105 ,  163 , 
        163 ,  105 ,  126 ,  105 ,  8 ,  126 ,  124 ,  131 ,  15 , 
        125 ,  164 ,  124 ,  127 ,  165 ,  125 ,  124 ,  164 ,  131 , 
        164 ,  130 ,  131 ,  125 ,  165 ,  164 ,  165 ,  167 ,  164 , 
        164 ,  167 ,  130 ,  167 ,  129 ,  130 ,  126 ,  103 ,  166 , 
        104 ,  100 ,  165 ,  165 ,  100 ,  167 ,  101 ,  99 ,  168 , 
        168 ,  99 ,  128 ,  99 ,  14 ,  128 ,  66 ,  123 ,  14 , 
        65 ,  169 ,  66 ,  64 ,  170 ,  65 ,  66 ,  169 ,  123 , 
        169 ,  120 ,  123 ,  65 ,  170 ,  169 ,  170 ,  171 ,  169 , 
        169 ,  171 ,  120 ,  172 ,  119 ,  122 ,  64 ,  95 ,  170 , 
        95 ,  92 ,  170 ,  170 ,  92 ,  171 ,  93 ,  91 ,  172 , 
        172 ,  91 ,  119 ,  91 ,  13 ,  119 ,  98 ,  117 ,  13 , 
        97 ,  173 ,  98 ,  96 ,  174 ,  97 ,  98 ,  173 ,  117 , 
        173 ,  116 ,  117 ,  97 ,  174 ,  173 ,  174 ,  175 ,  173 , 
        173 ,  175 ,  116 ,  175 ,  113 ,  116 ,  96 ,  87 ,  174 , 
        87 ,  85 ,  174 ,  174 ,  85 ,  175 ,  85 ,  83 ,  175 , 
        175 ,  83 ,  113 ,  83 ,  12 ,  113 ,  90 ,  112 ,  11 , 
        89 ,  176 ,  90 ,  88 ,  177 ,  89 ,  90 ,  176 ,  112 , 
        176 ,  111 ,  112 ,  89 ,  177 ,  176 ,  177 ,  178 ,  176 , 
        176 ,  178 ,  111 ,  178 ,  110 ,  111 ,  88 ,  77 ,  177 , 
        77 ,  76 ,  177 ,  177 ,  76 ,  178 ,  76 ,  75 ,  178 , 
        178 ,  75 ,  110 ,  75 ,  9 ,  110 ,  81 ,  109 ,  10 , 
        79 ,  179 ,  81 ,  78 ,  180 ,  79 ,  81 ,  179 ,  109 , 
        179 ,  106 ,  109 ,  79 ,  180 ,  179 ,  180 ,  181 ,  179 , 
        179 ,  181 ,  106 ,  181 ,  105 ,  106 ,  78 ,  69 ,  180 , 
        69 ,  68 ,  180 ,  180 ,  68 ,  181 ,  68 ,  67 ,  181 , 
        181 ,  67 ,  105 ,  67 ,  8 ,  105 ,  74 ,  103 ,  8 , 
        72 ,  182 ,  74 ,  70 ,  184 ,  73 ,  74 ,  182 ,  103 , 
        182 ,  102 ,  103 ,  73 ,  184 ,  183 ,  184 ,  185 ,  183 , 
        183 ,  185 ,  101 ,  185 ,  99 ,  101 ,  70 ,  63 ,  184 , 
        63 ,  62 ,  184 ,  184 ,  62 ,  185 ,  62 ,  61 ,  185 , 
        185 ,  61 ,  99 ,  61 ,  14 ,  99 ,  91 ,  98 ,  13 , 
        93 ,  186 ,  91 ,  94 ,  187 ,  93 ,  91 ,  186 ,  98 , 
        186 ,  97 ,  98 ,  93 ,  187 ,  186 ,  187 ,  188 ,  186 , 
        186 ,  188 ,  97 ,  188 ,  96 ,  97 ,  94 ,  58 ,  187 , 
        58 ,  57 ,  187 ,  187 ,  57 ,  188 ,  57 ,  56 ,  188 , 
        188 ,  56 ,  96 ,  56 ,  5 ,  96 ,  84 ,  90 ,  11 , 
        86 ,  189 ,  84 ,  87 ,  190 ,  85 ,  84 ,  189 ,  90 , 
        189 ,  89 ,  90 ,  86 ,  191 ,  189 ,  191 ,  192 ,  189 , 
        189 ,  192 ,  89 ,  192 ,  88 ,  89 ,  87 ,  54 ,  190 , 
        54 ,  51 ,  190 ,  191 ,  52 ,  192 ,  52 ,  50 ,  192 , 
        192 ,  50 ,  88 ,  50 ,  4 ,  88 ,  75 ,  82 ,  9 , 
        76 ,  193 ,  75 ,  77 ,  194 ,  76 ,  75 ,  193 ,  82 , 
        193 ,  80 ,  82 ,  76 ,  194 ,  193 ,  194 ,  195 ,  193 , 
        193 ,  195 ,  80 ,  196 ,  78 ,  79 ,  77 ,  45 ,  194 , 
        45 ,  42 ,  194 ,  194 ,  42 ,  195 ,  43 ,  40 ,  196 , 
        196 ,  40 ,  78 ,  40 ,  3 ,  78 ,  67 ,  74 ,  8 , 
        68 ,  197 ,  67 ,  69 ,  198 ,  68 ,  67 ,  197 ,  74 , 
        197 ,  72 ,  74 ,  68 ,  198 ,  197 ,  198 ,  199 ,  197 , 
        197 ,  199 ,  72 ,  199 ,  71 ,  72 ,  69 ,  28 ,  198 , 
        28 ,  27 ,  198 ,  198 ,  27 ,  199 ,  27 ,  24 ,  199 , 
        199 ,  24 ,  71 ,  24 ,  2 ,  71 ,  61 ,  66 ,  14 , 
        62 ,  200 ,  61 ,  63 ,  201 ,  62 ,  61 ,  200 ,  66 , 
        200 ,  65 ,  66 ,  62 ,  201 ,  200 ,  201 ,  202 ,  200 , 
        200 ,  202 ,  65 ,  202 ,  64 ,  65 ,  63 ,  31 ,  201 , 
        31 ,  30 ,  201 ,  201 ,  30 ,  202 ,  30 ,  29 ,  202 , 
        202 ,  29 ,  64 ,  29 ,  7 ,  64 ,  34 ,  59 ,  7 , 
        33 ,  203 ,  35 ,  32 ,  205 ,  33 ,  35 ,  203 ,  60 , 
        204 ,  57 ,  58 ,  33 ,  205 ,  203 ,  205 ,  206 ,  203 , 
        204 ,  207 ,  57 ,  207 ,  56 ,  57 ,  32 ,  49 ,  205 , 
        49 ,  48 ,  205 ,  205 ,  48 ,  206 ,  48 ,  46 ,  206 , 
        207 ,  47 ,  56 ,  47 ,  5 ,  56 ,  47 ,  54 ,  5 , 
        48 ,  208 ,  46 ,  49 ,  210 ,  48 ,  47 ,  209 ,  54 , 
        208 ,  53 ,  55 ,  48 ,  210 ,  208 ,  210 ,  211 ,  208 , 
        208 ,  211 ,  53 ,  212 ,  50 ,  52 ,  49 ,  39 ,  210 , 
        39 ,  38 ,  210 ,  210 ,  38 ,  211 ,  38 ,  36 ,  211 , 
        212 ,  37 ,  50 ,  37 ,  4 ,  50 ,  37 ,  45 ,  4 , 
        38 ,  213 ,  36 ,  39 ,  215 ,  38 ,  37 ,  214 ,  45 , 
        214 ,  42 ,  45 ,  38 ,  215 ,  213 ,  215 ,  216 ,  213 , 
        213 ,  216 ,  44 ,  216 ,  41 ,  44 ,  39 ,  19 ,  215 , 
        19 ,  18 ,  215 ,  215 ,  18 ,  216 ,  18 ,  16 ,  216 , 
        217 ,  17 ,  40 ,  17 ,  3 ,  40 ,  29 ,  34 ,  7 , 
        30 ,  218 ,  29 ,  31 ,  220 ,  30 ,  29 ,  218 ,  34 , 
        219 ,  33 ,  35 ,  30 ,  220 ,  218 ,  221 ,  222 ,  219 , 
        219 ,  222 ,  33 ,  222 ,  32 ,  33 ,  31 ,  22 ,  220 , 
        23 ,  21 ,  221 ,  221 ,  21 ,  222 ,  21 ,  20 ,  222 , 
        222 ,  20 ,  32 ,  20 ,  0 ,  32 ,  17 ,  28 ,  3 , 
        18 ,  223 ,  16 ,  19 ,  225 ,  18 ,  17 ,  224 ,  28 , 
        224 ,  27 ,  28 ,  18 ,  225 ,  223 ,  225 ,  226 ,  223 , 
        224 ,  227 ,  27 ,  227 ,  24 ,  27 ,  19 ,  20 ,  225 , 
        20 ,  21 ,  225 ,  225 ,  21 ,  226 ,  21, 23, 226,
        226, 23, 25, 22, 1, 26
    });

    compute_centroid();
    createPointBuffer(allow_edits, submit_immediately);
    createColorBuffer(allow_edits, submit_immediately);
    createNormalBuffer(allow_edits, submit_immediately);
    createTexCoordBuffer(allow_edits, submit_immediately);
    createIndexBuffer(allow_edits, submit_immediately);
}
