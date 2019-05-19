#pragma optimize("", off)

#include "Pluto.hxx"

bool Initialized = false;
#include <thread>
#include <iostream>
#include <map>
#include <string>

#include "Tools/Options.hxx"

#include "Libraries/GLFW/GLFW.hxx"
#include "Libraries/Vulkan/Vulkan.hxx"
#include "Libraries/OpenVR/OpenVR.hxx"

#include "Systems/EventSystem/EventSystem.hxx"
#include "Systems/PythonSystem/PythonSystem.hxx"
#include "Systems/RenderSystem/RenderSystem.hxx"
#include "Systems/PhysicsSystem/PhysicsSystem.hxx"

#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Texture/Texture.hxx"
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Material/Material.hxx"
#include "Pluto/Mesh/Mesh.hxx"
#include "Pluto/Light/Light.hxx"
#include "Pluto/Entity/Entity.hxx"

#include <tiny_obj_loader.h>
#include <glm/glm.hpp>

#include <pbrtParser/impl/semantic/SemanticParser.h>

namespace Pluto {

    std::thread callback_thread;
    void StartSystems(bool use_python, std::function<void()> callback)
    {
        auto glfw = Libraries::GLFW::Get();
        auto vulkan = Libraries::Vulkan::Get();

        auto python_system = Systems::PythonSystem::Get();
        auto event_system = Systems::EventSystem::Get();
        auto render_system = Systems::RenderSystem::Get();
        auto physics_system = Systems::PhysicsSystem::Get();

        if (use_python) {
            python_system->initialize();
        }
        event_system->initialize();
        render_system->initialize();
        physics_system->initialize();
        
        if (use_python) {
            python_system->start();
        }
        render_system->start();
        physics_system->start();

        //Initialize();
        if (callback)
        {
            if (callback_thread.joinable()) callback_thread.join();
            callback_thread = std::thread(callback);
        }

        event_system->start();
    }

    void WaitForStartupCallback()
    {
        if (callback_thread.joinable()) callback_thread.join();
    }

    void Initialize(
        bool useGLFW,
        bool useOpenVR,
        std::set<std::string> validation_layers,
        std::set<std::string> instance_extensions,
        std::set<std::string> device_extensions,
        std::set<std::string> device_features
    ) {
        if (Initialized) throw std::runtime_error("Error: Pluto already initialized");

        auto glfw = Libraries::GLFW::Get();
        // if (useGLFW && !glfw->is_initialized()) throw std::runtime_error("Error: GLFW was not initialized");

        auto vulkan = Libraries::Vulkan::Get();
        // if (!vulkan->is_initialized()) throw std::runtime_error("Error: Vulkan was not initialized");
        
        auto event_system = Systems::EventSystem::Get();
        // if (!event_system->running) throw std::runtime_error("Error: Event system is not running");

        auto render_system = Systems::RenderSystem::Get();
        // if (!render_system->running) throw std::runtime_error("Error: Event system is not running");
        
        while (!event_system->running) ;

        Initialized = true;

        event_system->use_openvr(useOpenVR);
        render_system->use_openvr(useOpenVR);

        if (useGLFW) event_system->create_window("TEMP", 1, 1, false, false, false, false);
        vulkan->create_instance(validation_layers.size() > 0, validation_layers, instance_extensions, useOpenVR);
        
        auto surface = (useGLFW) ? glfw->create_vulkan_surface(vulkan, "TEMP") : vk::SurfaceKHR();
        vulkan->create_device(device_extensions, device_features, 8, surface, useOpenVR);
        
        /* Initialize Component Factories. Order is important. */
        Transform::Initialize();
        Light::Initialize();
        Camera::Initialize();
        Entity::Initialize();
        Texture::Initialize();
        Mesh::Initialize();
        Material::Initialize();
        Light::CreateShadowCameras();

        auto skybox = Entity::Create("Skybox");
        auto sphere = Mesh::CreateSphere("SkyboxSphere");
        auto transform = Transform::Create("SkyboxTransform");
        auto material = Material::Create("SkyboxMaterial");
        material->show_environment();
        transform->set_scale(100000, 100000, 100000);
        skybox->set_mesh(sphere);
        skybox->set_material(material);
        skybox->set_transform(transform);

        if (useOpenVR) {
            auto ovr = Libraries::OpenVR::Get();
            ovr->create_eye_textures();

            ovr->get_left_eye_hidden_area_entity();
            ovr->get_right_eye_hidden_area_entity();
        }
        if (useGLFW) event_system->destroy_window("TEMP");
    }

    void StopSystems()
    {
        auto glfw = Libraries::GLFW::Get();
        auto vulkan = Libraries::Vulkan::Get();

        auto python_system = Systems::PythonSystem::Get();
        auto event_system = Systems::EventSystem::Get();
        auto render_system = Systems::RenderSystem::Get();
        auto physics_system = Systems::PhysicsSystem::Get();

        render_system->stop();
        python_system->stop();
        physics_system->stop();
        event_system->stop();


        /* All systems must be stopped before we can cleanup (Causes DeviceLost otherwise) */
        Pluto::CleanUp();
        
        std::cout<<"Shutting down Pluto"<<std::endl;
        
        Py_Finalize();
    }

    void CleanUp()
    {
        /* Note: CleanUp is safe to call more than once */
        Initialized = false;

        Mesh::CleanUp();
        Material::CleanUp();
        Transform::CleanUp();
        Light::CleanUp();
        Camera::CleanUp();
        Entity::CleanUp();
    }

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

        std::set<std::string> texture_paths;
        std::map<std::string, Texture*> texture_map;

        for (uint32_t i = 0; i < materials.size(); ++i) {
            materialComponents.push_back(Material::Create(materials[i].name));

            if (materials[i].alpha_texname.length() > 0)
                texture_paths.insert(materials[i].alpha_texname);

            if (materials[i].diffuse_texname.length() > 0)
                texture_paths.insert(materials[i].diffuse_texname);

            if (materials[i].bump_texname.length() > 0)
                texture_paths.insert(materials[i].bump_texname);

            materialComponents[i]->set_base_color(materials[i].diffuse[0], materials[i].diffuse[1], materials[i].diffuse[2], 1.0);
            materialComponents[i]->set_roughness(1.0);
            materialComponents[i]->set_metallic(0.0);
        }

        for (auto &path : texture_paths)
        {
            texture_map[path] = Texture::CreateFromPNG(mtl_base_dir + path, mtl_base_dir + path);
            // Maybe think of a better name here? Could accidentally conflict...
        }

        for (uint32_t i = 0; i < materials.size(); ++i) {

            // if (materials[i].alpha_texname.length() > 0)
                // texture_paths.insert(materials[i].alpha_texname);

            if (materials[i].diffuse_texname.length() > 0) {
                materialComponents[i]->set_base_color_texture(texture_map[materials[i].diffuse_texname]);
            }
            
            // if (materials[i].bump_texname.length() > 0)
                // texture_paths.insert(materials[i].bump_texname);
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

                std::vector<glm::vec3> positions; 
                std::vector<glm::vec4> colors; 
                std::vector<glm::vec3> normals; 
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
                        positions.push_back(glm::vec3(
                            attrib.vertices[3 * index.vertex_index + 0],
                            attrib.vertices[3 * index.vertex_index + 1],
                            attrib.vertices[3 * index.vertex_index + 2]
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
                            normals.push_back(glm::vec3(
                                attrib.normals[3 * index.normal_index + 0],
                                attrib.normals[3 * index.normal_index + 1],
                                attrib.normals[3 * index.normal_index + 2]
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

    inline bool endsWith(const std::string &s, const std::string &suffix)
    {
        return s.substr(s.size()-suffix.size(),suffix.size()) == suffix;
    }
    
    std::vector<Entity*> ImportPBRT(std::string name, std::string inFileName, std::string basePath)
    {
        struct stat st;
        if (stat(inFileName.c_str(), &st) != 0)
            throw std::runtime_error( std::string(inFileName + " does not exist!"));

        std::shared_ptr<pbrt::Scene> scene;
        if (endsWith(inFileName,".pbrt"))
            scene = pbrt::importPBRT(inFileName, basePath);
        else if (endsWith(inFileName,".pbf"))
            scene = pbrt::Scene::loadFrom(inFileName);
        else
            throw std::runtime_error("un-recognized input file extension");
        
        if (!scene) throw std::runtime_error( "pbrt scene was nullptr!");
        if (!scene->world) throw std::runtime_error( "pbrt scene->world was nullptr!");
        
        scene->makeSingleLevel();
        
        auto world = scene->world;
        auto instances = world->instances;

        std::unordered_map<std::string, Material*> material_map;
        std::vector<Entity*> entities;

        for (uint32_t inst_id = 0; inst_id < instances.size(); ++ inst_id) {
            auto instance = instances[inst_id];
            auto object = instance->object;
            auto xfm = instance->xfm;
            
            /* Extract the pbrt transform */
            glm::mat4 glm_xfm;
            glm_xfm[0] = glm::vec4(xfm.l.vx.x, xfm.l.vx.y, xfm.l.vx.z, xfm.p.x);
            glm_xfm[1] = glm::vec4(xfm.l.vy.x, xfm.l.vy.y, xfm.l.vy.z, xfm.p.y);
            glm_xfm[2] = glm::vec4(xfm.l.vz.x, xfm.l.vz.y, xfm.l.vz.z, xfm.p.z);
            glm_xfm[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            
            auto shapes = object->shapes;

            std::cout<<"instance " << instance->toString() << std::endl;
            std::cout<<"instance->object " << object->toString() << std::endl;
            
            for (uint32_t i = 0; i < shapes.size(); ++i) 
            {
                auto shape = shapes[i];
                std::cout<<"instance->object->shape[" << i << "]" << shape->toString() << std::endl;
                
                auto tfm = Transform::Create(name + "_" + std::to_string(inst_id) + "_" + std::to_string(i));
                tfm->set_transform(glm_xfm);

                auto pbrtmat = shape->material;
                std::string mat_type = pbrtmat->toString();
                std::string mesh_type = shape->toString();


                /* Many different types of materials. */
                std::string mat_name = name + "_" + std::to_string(inst_id) + "_" + std::to_string(i);
                auto mat = Material::Create(mat_name);
                {
                    if (mat_type.compare("DisneyMaterial") == 0) {
                        auto disney_mat = pbrtmat->as<pbrt::DisneyMaterial>();
                        mat->set_base_color(disney_mat->color.x, disney_mat->color.y, disney_mat->color.z, 1.0 );
                        mat->set_metallic(disney_mat->metallic);
                        mat->set_roughness(disney_mat->roughness);
                        mat->set_ior(disney_mat->eta);
                        mat->set_transmission(disney_mat->specTrans);
                        mat->set_transmission_roughness(disney_mat->diffTrans); // Not sure if this is correct.
                        
                        /* Other possible parameters:
                        anisotropic, clearCoat, clearCoatGloss, diffTrans, 
                        flatness, sheen, sheenTint, specularTint, thin
                        */
                    } else if (mat_type.compare("MixMaterial") == 0) {
                        auto mix_mat = pbrtmat->as<pbrt::MixMaterial>();
                        std::cout<<"Warning, Mix Material unsupported" << std::endl;
                        mat->set_base_color(1.0, 0.0, 1.0, 1.0);
                    } else if (mat_type.compare("MetalMaterial") == 0) {
                        std::cout<<"Warning, MetalMaterial not completely supported. Approximating..." << std::endl;
                        auto metal_mat = pbrtmat->as<pbrt::MetalMaterial>();
                        mat->set_base_color(metal_mat->k.x, metal_mat->k.y, metal_mat->k.z, 1.0 );
                        mat->set_ior(metal_mat->eta.x); // ior per channel not supported
                        mat->set_roughness(metal_mat->roughness);
                        /* Other possible parameters:
                        uroughness, vroughness, remaproughness
                        */
                    } else if (mat_type.compare("TranslucentMaterial") == 0) {
                        std::cout<<"Warning, TranslucentMaterial not completely supported. Approximating..." << std::endl;
                        auto translucent_mat = pbrtmat->as<pbrt::TranslucentMaterial>();
                        // Per color transmission not supported.
                        float max_transparency = std::max(std::max(translucent_mat->transmit.x, translucent_mat->transmit.y), translucent_mat->transmit.z);
                        float max_reflection = std::max(std::max(translucent_mat->reflect.x, translucent_mat->reflect.y), translucent_mat->reflect.z);
                        mat->set_base_color(translucent_mat->kd.x * max_reflection, 
                            translucent_mat->kd.y * max_reflection, 
                            translucent_mat->kd.z * max_reflection, 
                            max_transparency);
                        /* Other possible parameters:
                            ks, roughness (?), remaproughness
                        */
                    } else if (mat_type.compare("PlasticMaterial") == 0) {
                        auto plastic_mat = pbrtmat->as<pbrt::PlasticMaterial>();
                        mat->set_base_color(plastic_mat->kd.x, plastic_mat->kd.y, plastic_mat->kd.z, 1.0);
                        mat->set_roughness(plastic_mat->roughness);
                        /* Other possible parameters:
                            ks, remaproughness
                        */
                    } else if (mat_type.compare("SubstrateMaterial") == 0) {
                        auto substrate_mat = pbrtmat->as<pbrt::SubstrateMaterial>();
                        mat->set_base_color(substrate_mat->kd.x, substrate_mat->kd.y, substrate_mat->kd.z, 1.0);
                        mat->set_roughness(std::max(substrate_mat->uRoughness, substrate_mat->vRoughness));
                        if (substrate_mat->uRoughness != substrate_mat->vRoughness) {
                            std::cout<<"Warning, SubstrateMaterial different u and v roughness parameters not currently supported. Approximating..."<<std::endl;
                        }
                        /* Other possible parameters:
                            ks, remaproughness
                        */
                    } else if (mat_type.compare("SubSurfaceMaterial") == 0) {
                        std::cout<<"Warning, SubSurfaceMaterial not supported." << std::endl;
                        auto subsurface_mat = pbrtmat->as<pbrt::SubSurfaceMaterial>();
                        mat->set_roughness(std::max(subsurface_mat->uRoughness, subsurface_mat->vRoughness));
                        if (subsurface_mat->uRoughness != subsurface_mat->vRoughness) {
                            std::cout<<"Warning, SubSurfaceMaterial different u and v roughness parameters not currently supported. Approximating..."<<std::endl;
                        }
                        /* Other possible parameters:
                            sigma_a, sigma_prime_s, scale, ks, remaproughness
                        */
                    } else if (mat_type.compare("MirrorMaterial") == 0) {
                        auto mirror_mat = pbrtmat->as<pbrt::MirrorMaterial>();
                        mat->set_base_color(mirror_mat->kr.x, mirror_mat->kr.y, mirror_mat->kr.z, 1.0);
                        mat->set_roughness(0.0);
                        mat->set_metallic(1.0);
                        /* Other possible parameters: 
                            map_bump
                        */
                    } else if (mat_type.compare("FourierMaterial") == 0) {
                        std::cout<<"Warning, FourierMaterial not supported. " << std::endl;
                        auto fourier_mat = pbrtmat->as<pbrt::FourierMaterial>();
                        /* Other possible parameters: 
                            fileName
                        */
                    } else if (mat_type.compare("MatteMaterial") == 0) {
                        auto matte_mat = pbrtmat->as<pbrt::MatteMaterial>();
                        mat->set_roughness(1.0);
                        mat->set_base_color(matte_mat->kd.x, matte_mat->kd.y, matte_mat->kd.z, 1.0);
                        if (matte_mat->sigma > 0.f) {
                            std::cout<<"Warning: MatteMaterial sigma not currently supported" << std::endl;
                        }
                        /* Other possible parameters: 
                            sigma
                        */
                    } else if (mat_type.compare("GlassMaterial") == 0) {
                        auto glass_mat = pbrtmat->as<pbrt::GlassMaterial>();
                        mat->set_ior(glass_mat->index);
                        mat->set_base_color(glass_mat->kt.x, glass_mat->kt.y, glass_mat->kt.z, 1.0);
                        mat->set_roughness(0.0);
                        mat->set_metallic(.2f); // Makes the glass shiny
                        mat->set_transmission(1.0);
                        /* Other possible parameters:
                            kr
                        */
                    } else if (mat_type.compare("UberMaterial") == 0) {
                        auto uber_mat = pbrtmat->as<pbrt::UberMaterial>();
                        
                        mat->set_base_color(uber_mat->kd.x, uber_mat->kd.y, uber_mat->kd.z, uber_mat->alpha);
                        mat->set_roughness(uber_mat->roughness);
                        mat->set_ior(uber_mat->index);
                        /* Other possible parameters:
                            ks, kr, kt, opacity, uroughness, vroughness, remaproughness
                        */
                    }
                }

                /* Many different types of shapes */
                std::string mesh_name = name + "_" + std::to_string(inst_id) + "_" + std::to_string(i);
                Mesh* mesh  = nullptr;
                {
                    if (mesh_type.compare("TriangleMesh") == 0) {
                        auto triangle_mesh = shape->as<pbrt::TriangleMesh>();
                        
                        if (triangle_mesh->vertex.size() <= 0) {
                            std::cout<<"Error: mesh " << mesh_name << " has no vertices!" << std::endl;
                            mesh = Mesh::Get("DefaultMesh");
                        }
                        else if (triangle_mesh->index.size() <= 0) {
                            std::cout<<"Error: mesh " << mesh_name << " has no indices!" << std::endl;
                            mesh = Mesh::Get("DefaultMesh");
                        }
                        else {
                            std::vector<uint32_t> indices(3 * triangle_mesh->getNumPrims());
                            for (uint32_t prim_idx = 0; prim_idx < triangle_mesh->getNumPrims(); ++prim_idx)
                            {
                                indices[prim_idx * 3 + 0] = triangle_mesh->index[prim_idx].x;
                                indices[prim_idx * 3 + 1] = triangle_mesh->index[prim_idx].y;
                                indices[prim_idx * 3 + 2] = triangle_mesh->index[prim_idx].z;
                            }
                            
                            std::vector<glm::vec3> positions(triangle_mesh->vertex.size());
                            for (uint32_t pos_idx = 0; pos_idx < triangle_mesh->vertex.size(); ++pos_idx)
                            {
                                auto pos = triangle_mesh->vertex[pos_idx];
                                positions[pos_idx] = glm::vec3(pos.x, pos.y, pos.z);
                            }

                            std::vector<glm::vec3> normals(triangle_mesh->normal.size());
                            for (uint32_t norm_idx = 0; norm_idx < triangle_mesh->normal.size(); ++norm_idx)
                            {
                                auto norm = triangle_mesh->normal[norm_idx];
                                normals[norm_idx] = glm::vec3(norm.x, norm.y, norm.z);
                            }

                            std::vector<glm::vec2> texcoords(triangle_mesh->texcoord.size());
                            for (uint32_t tc_idx = 0; tc_idx < triangle_mesh->texcoord.size(); ++tc_idx)
                            {
                                auto tc = triangle_mesh->texcoord[tc_idx];
                                texcoords[tc_idx] = glm::vec2(tc.x, tc.y);
                            }

                            // PBRT doesnt support vertex colors
                            std::vector<glm::vec4> colors;
                            mesh = Mesh::CreateFromRaw(mesh_name, positions, normals, colors, texcoords, indices, true);
                            if (normals.size() == 0)
                                mesh->compute_smooth_normals();
                        }
                    } else if (mesh_type.compare("Sphere") == 0) {
                        auto sphere = shape->as<pbrt::Sphere>();
                        mesh = Mesh::CreateSphere(mesh_name, sphere->radius);
                        sphere->transform; // Need to add support for this...
                    } else if (mesh_type.compare("Curve") == 0) {
                        auto curve = shape->as<pbrt::Curve>();
                        mesh = Mesh::Get("DefaultMesh");
                        std::cout<<"Warning: curve mesh not yet supported"<<std::endl;
                    } else if (mesh_type.compare("Disk") == 0) {
                        auto disk = shape->as<pbrt::Disk>();
                        mesh = Mesh::CreateCappedCylinder(mesh_name, disk->radius, disk->height);
                        disk->transform; // Need to add support for this...
                    }
                }

                entities.push_back(
                    Entity::Create(
                        name + "_" + std::to_string(inst_id) + "_" + std::to_string(i),
                        tfm, nullptr, mat, nullptr, mesh, nullptr, nullptr)
                );                
            }
        }

        // auto instances = world->instances;
        // for (uint32_t i = 0; i < instances.size(); ++i )
        // {
        //     auto instance = instances[i];
        //     std::cout<<instance->toString()<<std::endl;
        // }
        


        
        return entities;
    }
}
