#include "OBJ.hxx"

#include "Pluto/Pluto.hxx"

#include <thread>
#include <iostream>
#include <map>
#include <string>

#include "Pluto/Tools/Options.hxx"

#include "Pluto/Libraries/GLFW/GLFW.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Libraries/OpenVR/OpenVR.hxx"

#include "Pluto/Systems/EventSystem/EventSystem.hxx"
#include "Pluto/Systems/PythonSystem/PythonSystem.hxx"
#include "Pluto/Systems/RenderSystem/RenderSystem.hxx"
#include "Pluto/Systems/PhysicsSystem/PhysicsSystem.hxx"

#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Texture/Texture.hxx"
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Material/Material.hxx"
#include "Pluto/Mesh/Mesh.hxx"
#include "Pluto/Light/Light.hxx"
#include "Pluto/Entity/Entity.hxx"

#include "Pluto/Prefabs/Prefabs.hxx"

#include <tiny_obj_loader.h>
#include <glm/glm.hpp>

#include "stb_image_write.h"

struct OBJTextureInfo {
    std::string path = "";
    bool is_bump = false;
};

struct OBJTextureInfoCompare {
    bool operator() (const OBJTextureInfo& lhs, const OBJTextureInfo& rhs) const
    {
        return lhs.path < rhs.path;
    }
};

namespace Pluto {
    std::vector<Entity*> ImportOBJ(std::string filepath, std::string mtl_base_dir, glm::vec3 position, glm::vec3 scale, glm::quat rotation)
    {
        struct stat st;
        if (stat(filepath.c_str(), &st) != 0)
            throw std::runtime_error( std::string(filepath + " does not exist!"));

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::map<std::string, int> material_map;
        std::string err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filepath.c_str(), mtl_base_dir.c_str()))
            throw std::runtime_error( std::string("Error: Unable to load " + filepath));

        if (err.size() > 0)
            std::cout<< err << std::endl;

        std::vector<Material*> materialComponents;
        std::vector<Transform*> transformComponents;
        std::vector<Entity*> entities;

        std::set<OBJTextureInfo, OBJTextureInfoCompare> texture_paths;
        std::map<std::string, Texture*> texture_map;

        if (materials.size() >= MAX_MATERIALS) {
            throw std::runtime_error(std::string("Error, total materials found is ") 
                + std::to_string(materials.size()) 
                + std::string(" but max supported materials is set to ")
                + std::to_string(MAX_MATERIALS) 
            );
        }

        for (uint32_t i = 0; i < materials.size(); ++i) {
            materialComponents.push_back(Material::Create(materials[i].name));
            int illum_group = materials[i].illum;

            // Meaning of illum group
            // 0. Color on and Ambient off
            // 1. Color on and Ambient on
            // 2. Highlight on
            // 3. Reflection on and Ray trace on
            // 4. Transparency: Glass on, Reflection: Ray trace on
            // 5. Reflection: Fresnel on and Ray trace on
            // 6. Transparency: Refraction on, Reflection: Fresnel off and Ray trace on
            // 7. Transparency: Refraction on, Reflection: Fresnel on and Ray trace on
            // 8. Reflection on and Ray trace off
            // 9. Transparency: Glass on, Reflection: Ray trace off
            // 10. Casts shadows onto invisible surfaces

            if (materials[i].alpha_texname.length() > 0)
                texture_paths.insert({materials[i].alpha_texname, false});

            if (materials[i].ambient_texname.length() > 0)
                texture_paths.insert({materials[i].ambient_texname, false});
            
            if (materials[i].bump_texname.length() > 0)
                texture_paths.insert({materials[i].bump_texname, true});

            if (materials[i].displacement_texname.length() > 0)
                texture_paths.insert({materials[i].displacement_texname, false});
            
            if (materials[i].diffuse_texname.length() > 0)
                texture_paths.insert({materials[i].diffuse_texname, false});

            if (materials[i].emissive_texname.length() > 0)
                texture_paths.insert({materials[i].emissive_texname, false});

            if (materials[i].metallic_texname.length() > 0)
                texture_paths.insert({materials[i].metallic_texname, false});

            if (materials[i].normal_texname.length() > 0)
                texture_paths.insert({materials[i].normal_texname, false});

            if (materials[i].reflection_texname.length() > 0)
                texture_paths.insert({materials[i].reflection_texname, false});

            if (materials[i].roughness_texname.length() > 0)
                texture_paths.insert({materials[i].roughness_texname, false});

            if (materials[i].sheen_texname.length() > 0)
                texture_paths.insert({materials[i].sheen_texname, false});

            if (materials[i].specular_highlight_texname.length() > 0)
                texture_paths.insert({materials[i].specular_highlight_texname, false});

            if (materials[i].specular_texname.length() > 0)
                texture_paths.insert({materials[i].specular_texname, false});

            materialComponents[i]->set_base_color(materials[i].diffuse[0], materials[i].diffuse[1], materials[i].diffuse[2]);
            materialComponents[i]->set_roughness(1.0);
            materialComponents[i]->set_metallic(0.0);

            if (illum_group == 6) {
                materialComponents[i]->set_transmission(1.0);
            }
        }

        for (auto &pathobj : texture_paths)
        {
            if (pathobj.is_bump)
                texture_map[pathobj.path] = Texture::CreateFromBumpPNG(mtl_base_dir + pathobj.path, mtl_base_dir + pathobj.path);
            else 
                texture_map[pathobj.path] = Texture::CreateFromPNG(mtl_base_dir + pathobj.path, mtl_base_dir + pathobj.path);
            // Maybe think of a better name here? Could accidentally conflict...
        }

        for (uint32_t i = 0; i < materials.size(); ++i) {

            if (materials[i].alpha_texname.length() > 0) {
                materialComponents[i]->set_alpha_texture(texture_map[materials[i].alpha_texname]);
            }

            if (materials[i].diffuse_texname.length() > 0) {
                materialComponents[i]->set_base_color_texture(texture_map[materials[i].diffuse_texname]);
            }
            
            if (materials[i].bump_texname.length() > 0) {
                materialComponents[i]->set_bump_texture(texture_map[materials[i].bump_texname]);
            }

            if (materials[i].normal_texname.length() > 0) {
                materialComponents[i]->set_bump_texture(texture_map[materials[i].normal_texname]);
            }

            if (materials[i].displacement_texname.length() > 0) {
                materialComponents[i]->set_bump_texture(texture_map[materials[i].displacement_texname]);
            }

            if (materials[i].roughness_texname.length() > 0) {
                materialComponents[i]->set_roughness_texture(texture_map[materials[i].roughness_texname]);
            }

            // if (materials[i].metallic_texname.length() > 0) {
            //     materialComponents[i]->set_metallic_texture(texture_map[materials[i].metallic_texname]);
            // }

            // TODO:

            // if (materials[i].ambient_texname.length() > 0)
            //     texture_paths.insert(materials[i].ambient_texname);

            // if (materials[i].emissive_texname.length() > 0)
            //     texture_paths.insert(materials[i].emissive_texname);

            // if (materials[i].reflection_texname.length() > 0)
            //     texture_paths.insert(materials[i].reflection_texname);

            // if (materials[i].sheen_texname.length() > 0)
            //     texture_paths.insert(materials[i].sheen_texname);

            // if (materials[i].specular_highlight_texname.length() > 0)
            //     texture_paths.insert(materials[i].specular_highlight_texname);

            // if (materials[i].specular_texname.length() > 0)
            //     texture_paths.insert(materials[i].specular_texname);
        }

        for (uint32_t i = 0; i < shapes.size(); ++i) {

            /* Determine how many materials are in this shape... */
            std::set<uint32_t> material_ids;
            for (uint32_t j = 0; j < shapes[i].mesh.material_ids.size(); ++j) {
                material_ids.insert(shapes[i].mesh.material_ids[j]);
            }

            uint32_t mat_offset = 0;

            /* Create a model for each found material id for the given shape. */
            for (auto material_id : material_ids) 
            {
                mat_offset++;

                std::vector<glm::vec4> positions; 
                std::vector<glm::vec4> colors; 
                std::vector<glm::vec4> normals; 
                std::vector<glm::vec2> texcoords; 

                /* For each face */
                size_t index_offset = 0;
                for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++) {
                    int fv = shapes[i].mesh.num_face_vertices[f];

                    /* Skip any faces which don't use the current material */
                    if (shapes[i].mesh.material_ids[f] != material_id) {
                        index_offset += fv;
                        continue;
                    }

                    // Loop over vertices in the face.
                    for (size_t v = 0; v < fv; v++) {
                        auto index = shapes[i].mesh.indices[index_offset + v];
                        positions.push_back(glm::vec4(
                            attrib.vertices[3 * index.vertex_index + 0],
                            attrib.vertices[3 * index.vertex_index + 1],
                            attrib.vertices[3 * index.vertex_index + 2],
                            1.0f
                        ));

                        if (attrib.colors.size() != 0) {
                            colors.push_back(glm::vec4(
                                attrib.colors[3 * index.vertex_index + 0],
                                attrib.colors[3 * index.vertex_index + 1],
                                attrib.colors[3 * index.vertex_index + 2],
                                1.0f
                            ));
                        }

                        if (attrib.normals.size() != 0) {
                            normals.push_back(glm::vec4(
                                attrib.normals[3 * index.normal_index + 0],
                                attrib.normals[3 * index.normal_index + 1],
                                attrib.normals[3 * index.normal_index + 2],
                                0.0f
                            ));
                        }

                        if (attrib.texcoords.size() != 0) {
                            texcoords.push_back(glm::vec2(
                                attrib.texcoords[2 * index.texcoord_index + 0],
                                attrib.texcoords[2 * index.texcoord_index + 1]
                            ));
                        }
                    }
                    index_offset += fv;
                }

                /* Some shapes with multiple materials report sizes which aren't a multiple of 3... This is a kludge... */
                if (positions.size() % 3 != 0) positions.resize(positions.size() - (positions.size() % 3));
                if (colors.size() % 3 != 0) colors.resize(colors.size() - (colors.size() % 3));
                if (normals.size() % 3 != 0) normals.resize(normals.size() - (normals.size() % 3));
                if (texcoords.size() % 3 != 0) texcoords.resize(texcoords.size() - (texcoords.size() % 3));

                /* We need at least one point to render... */
                if (positions.size() < 3) continue;

                auto entity = Entity::Create(shapes[i].name + "_" + std::to_string(mat_offset));
                auto transform = Transform::Create(shapes[i].name + "_" + std::to_string(mat_offset));
                transform->set_position(position);
                transform->set_scale(scale);
                transform->set_rotation(rotation);
                entities.push_back(entity);
                transformComponents.push_back(transform);
                entity->set_transform(transform);

                // Since there can be multiple material ids per shape, we have to separate these shapes into
                // separate entities...
                entity->set_material(materialComponents[material_id]);

                auto mesh = Mesh::CreateFromRaw(shapes[i].name + "_" + std::to_string(mat_offset), positions, normals, colors, texcoords);
                entity->set_mesh(mesh);
            }
        }

        return entities;
    }
}