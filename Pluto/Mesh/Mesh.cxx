#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#endif

#include "./Mesh.hxx"
#include <Pluto/Tools/HashCombiner.hxx>

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
