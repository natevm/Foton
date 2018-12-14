// TEMPORARY!!
#pragma optimize("", off)

#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#endif

#define TINYGLTF_IMPLEMENTATION
// #define TINYGLTF_NO_FS
// #define TINYGLTF_NO_STB_IMAGE_WRITE


#include "./Mesh.hxx"
#include <Pluto/Tools/HashCombiner.hxx>
#include <tiny_stl.h>
#include <tiny_gltf.h>

Mesh Mesh::meshes[MAX_MESHES];
std::map<std::string, uint32_t> Mesh::lookupTable;

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

bool Mesh::load_obj(std::string objPath)
{
    struct stat st;
    if (stat(objPath.c_str(), &st) != 0)
    {
        std::cout << objPath + " does not exist!" << std::endl;
        return false;
    }

    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, objPath.c_str()))
    {
        std::cout << "Error: Unable to load " << objPath << std::endl;
        return false;
    }

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
    createPointBuffer();
    createColorBuffer();
    createIndexBuffer();
    createNormalBuffer();
    createTexCoordBuffer();

    return true;
}


bool Mesh::load_stl(std::string stlPath) {
    struct stat st;
    if (stat(stlPath.c_str(), &st) != 0)
    {
        std::cout << stlPath + " does not exist!" << std::endl;
        return false;
    }

    std::vector<float> p;
    std::vector<float> n;

    if (!read_stl(stlPath, p, n) ) {
        std::cout << "Error: Unable to load " << stlPath << std::endl;
        return false;
    };

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
    createPointBuffer();
    createColorBuffer();
    createIndexBuffer();
    createNormalBuffer();
    createTexCoordBuffer();

    return true;
}

bool Mesh::load_glb(std::string glbPath)
{
    struct stat st;
    if (stat(glbPath.c_str(), &st) != 0)
    {
        std::cout << glbPath + " does not exist!" << std::endl;
        return false;
    }

    // read file
    unsigned char *file_buffer = NULL;
	uint32_t file_size = 0;
	{
		FILE *fp = fopen(glbPath.c_str(), "rb");
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
    {
        std::cout << "Error: Unable to load " << glbPath << " " << err << std::endl;
        return false;
    }

    std::vector<Mesh::Vertex> vertices;

    // TODO: add support for multiple meshes per entity
    assert(model.meshes.size() == 1);

    for (const auto &mesh : model.meshes) {
        // TODO: add support for multiple primitives per mesh
        assert(mesh.primitives.size() == 1);
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
    createPointBuffer();
    createColorBuffer();
    createIndexBuffer();
    createNormalBuffer();
    createTexCoordBuffer();

    return true;
}

/* Static Factory Implementations */
Mesh* Mesh::Get(std::string name) {
    return StaticFactory::Get(name, "Mesh", lookupTable, meshes, MAX_MESHES);
}

Mesh* Mesh::Get(uint32_t id) {
    return StaticFactory::Get(id, "Mesh", lookupTable, meshes, MAX_MESHES);
}

Mesh* Mesh::CreateCube(std::string name)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    if (!mesh->make_cube()) return nullptr;
    return mesh;
}

Mesh* Mesh::CreatePlane(std::string name)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    if (!mesh->make_plane()) return nullptr;
    return mesh;
}

Mesh* Mesh::CreateSphere(std::string name)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh) return nullptr;
    if (!mesh->make_sphere()) return nullptr;
    return mesh;
}

Mesh* Mesh::CreateFromOBJ(std::string name, std::string objPath)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh->load_obj(objPath)) return nullptr;
    return mesh;
}

Mesh* Mesh::CreateFromSTL(std::string name, std::string stlPath)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh->load_stl(stlPath)) return nullptr;
    return mesh;
}

Mesh* Mesh::CreateFromGLB(std::string name, std::string glbPath)
{
    auto mesh = StaticFactory::Create(name, "Mesh", lookupTable, meshes, MAX_MESHES);
    if (!mesh->load_glb(glbPath)) return nullptr;
    return mesh;
}

bool Mesh::Delete(std::string name) {
    return StaticFactory::Delete(name, "Mesh", lookupTable, meshes, MAX_MESHES);
}

bool Mesh::Delete(uint32_t id) {
    return StaticFactory::Delete(id, "Mesh", lookupTable, meshes, MAX_MESHES);
}

Mesh* Mesh::GetFront() {
    return meshes;
}

uint32_t Mesh::GetCount() {
    return MAX_MESHES;
}
