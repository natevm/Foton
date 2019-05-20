#include "PBRT.hxx"

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

#include <pbrtParser/impl/semantic/SemanticParser.h>

#include "stb_image_write.h"

namespace Pluto {

    inline bool endsWith(const std::string &s, const std::string &suffix)
    {
        return s.substr(s.size()-suffix.size(),suffix.size()) == suffix;
    }
    
    inline glm::mat4 pbrt_affine_to_glm(pbrt::affine3f xfm)
    {
        return glm::mat4(
            -xfm.l.vx.x, xfm.l.vz.x, xfm.l.vy.x, 0.0f, // column 1 (right)
            -xfm.l.vx.y, xfm.l.vz.y, xfm.l.vy.y, 0.0f, // column 2 (forward)
            -xfm.l.vx.z, xfm.l.vz.z, xfm.l.vy.z, 0.0f, // column 3  (up)
            xfm.p.x, xfm.p.z, xfm.p.y, 1.0f           // column 4 (position)
        );

        // return glm::mat4(
        //     -xfm.l.vx.x, -xfm.l.vx.z, -xfm.l.vx.y, 0.0f, // column 1 (right)
        //     xfm.l.vz.x, xfm.l.vz.z, xfm.l.vz.y, 0.0f, // column 2 (forward)
        //     xfm.l.vy.x, xfm.l.vy.z, xfm.l.vy.y, 0.0f, // column 3 (up)
        //     xfm.p.x, xfm.p.z, xfm.p.y, 1.0f           // column 4 (position)
        // );
    }

    inline glm::mat4 pbrt_frame_to_glm(pbrt::affine3f xfm)
    {
        return glm::mat4(
            -xfm.l.vx.x, -xfm.l.vx.z, -xfm.l.vx.y, 0.0f, // column 1 (right)
            xfm.l.vz.x, xfm.l.vz.z, xfm.l.vz.y, 0.0f, // column 2 (forward)
            xfm.l.vy.x, xfm.l.vy.z, xfm.l.vy.y, 0.0f, // column 3 (up)
            xfm.p.x, xfm.p.z, xfm.p.y, 1.0f           // column 4 (position)
        );
    }

    inline Texture* get_texture_from_pbrt(
        std::unordered_map<std::shared_ptr<pbrt::Texture>, Texture*> &texture_map, 
        std::string basePath, std::shared_ptr<pbrt::Texture> pbrt_texture) 
    {
        Texture* tex = nullptr;
        
        /* Try finding the texture from the texture map. */
        auto item = texture_map.find(pbrt_texture);
        if (item != texture_map.end()) tex = item->second;

        /* If this is a new texture, import it */
        if (!tex) {
            auto texture_type = pbrt_texture->toString();
            if (texture_type.compare("ImageTexture") == 0) {
                auto image_texture = pbrt_texture->as<pbrt::ImageTexture>();
                if (endsWith(image_texture->fileName, ".png")) {
                    if (Texture::DoesItemExist(image_texture->fileName)) {
                        texture_map[pbrt_texture] = Texture::Get(image_texture->fileName);
                    } else {
                        tex = Texture::CreateFromPNG(image_texture->fileName, basePath + image_texture->fileName);
                        texture_map[pbrt_texture] = tex;
                    }
                }
            }
        }
        return tex;
    }

    inline Material* get_material_from_pbrt(
        std::unordered_map<std::shared_ptr<pbrt::Material>, Material*> &material_map,
        std::unordered_map<std::shared_ptr<pbrt::Texture>, Texture*> &texture_map, 
        std::string basePath, std::string mat_name, std::shared_ptr<pbrt::Material> pbrt_material) 
    {
        std::string mat_type = (pbrt_material) ? pbrt_material->toString() : "";
        Material* mat = nullptr;
        if (pbrt_material) {
            auto item = material_map.find(pbrt_material);
            if (item != material_map.end())
                mat = item->second;
        }

        if (!mat) {
            /* Many different types of materials. */
            mat = Material::Create(mat_name);
            material_map[pbrt_material] = mat;
            {
                if (mat_type.compare("DisneyMaterial") == 0) {
                    auto disney_mat = pbrt_material->as<pbrt::DisneyMaterial>();
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
                    auto mix_mat = pbrt_material->as<pbrt::MixMaterial>();
                    std::cout<<"Warning, Mix Material unsupported" << std::endl;
                } else if (mat_type.compare("MetalMaterial") == 0) {
                    std::cout<<"Warning, MetalMaterial not completely supported. Approximating..." << std::endl;
                    auto metal_mat = pbrt_material->as<pbrt::MetalMaterial>();
                    mat->set_base_color(metal_mat->k.x, metal_mat->k.y, metal_mat->k.z, 1.0 );
                    mat->set_ior(metal_mat->eta.x); // ior per channel not supported
                    mat->set_roughness(metal_mat->roughness);
                    /* Other possible parameters:
                    uroughness, vroughness, remaproughness
                    */

                    if (metal_mat->map_bump) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, metal_mat->map_bump);
                        // if (tex) TODO 
                    }
                    if (metal_mat->map_roughness) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, metal_mat->map_roughness);
                        if (tex) mat->set_roughness_texture(tex);
                        // if (tex) TODO 
                    }
                    if (metal_mat->map_uRoughness) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, metal_mat->map_uRoughness);
                        // if (tex) TODO 
                    }
                    if (metal_mat->map_vRoughness) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, metal_mat->map_vRoughness);
                        // if (tex) TODO 
                    }
                } else if (mat_type.compare("TranslucentMaterial") == 0) {
                    std::cout<<"Warning, TranslucentMaterial not completely supported. Approximating..." << std::endl;
                    auto translucent_mat = pbrt_material->as<pbrt::TranslucentMaterial>();
                    // Per color transmission not supported.
                    float max_transparency = std::max(std::max(translucent_mat->transmit.x, translucent_mat->transmit.y), translucent_mat->transmit.z);
                    float max_reflection = std::max(std::max(translucent_mat->reflect.x, translucent_mat->reflect.y), translucent_mat->reflect.z);
                    mat->set_base_color(translucent_mat->kd.x * max_reflection, 
                        translucent_mat->kd.y * max_reflection, 
                        translucent_mat->kd.z * max_reflection, 
                        max_transparency);

                    if (translucent_mat->map_kd) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, translucent_mat->map_kd);
                        if (tex) mat->set_base_color_texture(tex);
                    }
                    /* Other possible parameters:
                        ks, roughness (?), remaproughness
                    */
                } else if (mat_type.compare("PlasticMaterial") == 0) {
                    auto plastic_mat = pbrt_material->as<pbrt::PlasticMaterial>();
                    mat->set_base_color(plastic_mat->kd.x, plastic_mat->kd.y, plastic_mat->kd.z, 1.0);
                    mat->set_roughness(plastic_mat->roughness);

                    if (plastic_mat->map_bump) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, plastic_mat->map_bump);
                        // if (tex) TODO 
                    }
                    if (plastic_mat->map_ks) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, plastic_mat->map_ks);
                        // if (tex) TODO 
                    }
                    if (plastic_mat->map_roughness) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, plastic_mat->map_roughness);
                        if (tex) mat->set_roughness_texture(tex);
                    }
                    if (plastic_mat->map_kd) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, plastic_mat->map_kd);
                        if (tex) mat->set_base_color_texture(tex);
                    }
                    /* Other possible parameters:
                        ks, remaproughness
                    */
                } else if (mat_type.compare("SubstrateMaterial") == 0) {
                    auto substrate_mat = pbrt_material->as<pbrt::SubstrateMaterial>();
                    mat->set_base_color(substrate_mat->kd.x, substrate_mat->kd.y, substrate_mat->kd.z, 1.0);
                    mat->set_roughness(std::max(substrate_mat->uRoughness, substrate_mat->vRoughness));
                    if (substrate_mat->uRoughness != substrate_mat->vRoughness) {
                        std::cout<<"Warning, SubstrateMaterial different u and v roughness parameters not currently supported. Approximating..."<<std::endl;
                    }
                    if (substrate_mat->map_bump) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, substrate_mat->map_bump);
                        // if (tex) TODO 
                    }
                    if (substrate_mat->map_ks) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, substrate_mat->map_ks);
                        // if (tex) TODO 
                    }
                    if (substrate_mat->map_uRoughness) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, substrate_mat->map_uRoughness);
                        // if (tex) TODO 
                    }
                    if (substrate_mat->map_vRoughness) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, substrate_mat->map_vRoughness);
                        // if (tex) TODO 
                    }
                    if (substrate_mat->map_kd) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, substrate_mat->map_kd);
                        if (tex) mat->set_base_color_texture(tex);
                    }
                    /* Other possible parameters:
                        ks, remaproughness
                    */

                } else if (mat_type.compare("SubSurfaceMaterial") == 0) {
                    std::cout<<"Warning, SubSurfaceMaterial not supported." << std::endl;
                    auto subsurface_mat = pbrt_material->as<pbrt::SubSurfaceMaterial>();
                    mat->set_roughness(std::max(subsurface_mat->uRoughness, subsurface_mat->vRoughness));
                    if (subsurface_mat->uRoughness != subsurface_mat->vRoughness) {
                        std::cout<<"Warning, SubSurfaceMaterial different u and v roughness parameters not currently supported. Approximating..."<<std::endl;
                    }

                    /* Other possible parameters:
                        sigma_a, sigma_prime_s, scale, ks, remaproughness
                    */
                } else if (mat_type.compare("MirrorMaterial") == 0) {
                    auto mirror_mat = pbrt_material->as<pbrt::MirrorMaterial>();
                    mat->set_base_color(mirror_mat->kr.x, mirror_mat->kr.y, mirror_mat->kr.z, 1.0);
                    mat->set_roughness(0.0);
                    mat->set_metallic(1.0);

                    if (mirror_mat->map_bump) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, mirror_mat->map_bump);
                        //if (tex) TODO
                    }
                    /* Other possible parameters: 
                    */
                } else if (mat_type.compare("FourierMaterial") == 0) {
                    std::cout<<"Warning, FourierMaterial not supported. " << std::endl;
                    auto fourier_mat = pbrt_material->as<pbrt::FourierMaterial>();
                    /* Other possible parameters: 
                        fileName
                    */
                } else if (mat_type.compare("MatteMaterial") == 0) {
                    auto matte_mat = pbrt_material->as<pbrt::MatteMaterial>();
                    mat->set_roughness(1.0);
                    mat->set_base_color(matte_mat->kd.x, matte_mat->kd.y, matte_mat->kd.z, 1.0);
                    if (matte_mat->sigma > 0.f) {
                        std::cout<<"Warning: MatteMaterial sigma not currently supported" << std::endl;
                    }

                    if (matte_mat->map_bump) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, matte_mat->map_bump);
                        //if (tex) TODO
                    }
                    if (matte_mat->map_sigma) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, matte_mat->map_sigma);
                        //if (tex) TODO
                    }
                    if (matte_mat->map_kd) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, matte_mat->map_kd);
                        if (tex) mat->set_base_color_texture(tex);
                    }
                    /* Other possible parameters: 
                        sigma
                    */
                } else if (mat_type.compare("GlassMaterial") == 0) {
                    auto glass_mat = pbrt_material->as<pbrt::GlassMaterial>();
                    mat->set_ior(glass_mat->index);
                    mat->set_base_color(glass_mat->kt.x, glass_mat->kt.y, glass_mat->kt.z, 1.0);
                    mat->set_roughness(0.0);
                    mat->set_metallic(.2f); // Makes the glass shiny
                    mat->set_transmission(1.0);
                    /* Other possible parameters:
                        kr
                    */
                } else if (mat_type.compare("UberMaterial") == 0) {
                    auto uber_mat = pbrt_material->as<pbrt::UberMaterial>();
                    
                    mat->set_base_color(uber_mat->kd.x, uber_mat->kd.y, uber_mat->kd.z, uber_mat->alpha);
                    mat->set_roughness(uber_mat->roughness);
                    mat->set_ior(uber_mat->index);

                    if (uber_mat->map_kd) {
                        std::cout<<"UBERMAT DIFFFUSE TEXTURE" << std::endl;
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, uber_mat->map_kd);
                        if (tex) mat->set_base_color_texture(tex);
                    }

                    if (uber_mat->map_alpha) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, uber_mat->map_alpha);
                        //if (tex) TODO
                    }
                    if (uber_mat->map_bump) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, uber_mat->map_bump);
                        //if (tex) TODO
                    }
                    if (uber_mat->map_opacity) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, uber_mat->map_opacity);
                        //if (tex) TODO
                    }
                    if (uber_mat->map_roughness) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, uber_mat->map_roughness);
                        if (tex) mat->set_roughness_texture(tex);
                    }
                    if (uber_mat->map_shadowAlpha) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, uber_mat->map_shadowAlpha);
                        //if (tex) TODO
                    }
                    if (uber_mat->map_uRoughness) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, uber_mat->map_uRoughness);
                        //if (tex) TODO
                    }
                    if (uber_mat->map_vRoughness) {
                        Texture* tex = get_texture_from_pbrt(texture_map, basePath, uber_mat->map_vRoughness);
                        //if (tex) TODO
                    }

                    /* Other possible parameters:
                        ks, kr, kt, opacity, uroughness, vroughness, remaproughness
                    */
                }
            }
        }

        return mat;
    }

    std::vector<Entity*> ImportPBRT(std::string name, std::string inFileName, std::string basePath,
        glm::vec3 position = glm::vec3(0.0f), 
        glm::vec3 scale = glm::vec3(1.0f),
        glm::quat rotation = glm::angleAxis(0.0f, glm::vec3(1.0f, 0.0f, 0.0f)))
    {
        std::shared_ptr<pbrt::Scene> scene;

        struct stat st;
        if (stat(inFileName.c_str(), &st) != 0)
            throw std::runtime_error( std::string(inFileName + " does not exist!"));
        if (endsWith(inFileName,".pbrt"))
            scene = pbrt::importPBRT(inFileName, basePath);
        else if (endsWith(inFileName,".pbf"))
            scene = pbrt::Scene::loadFrom(inFileName);
        else
            throw std::runtime_error("un-recognized input file extension");

        if (!scene) throw std::runtime_error( "pbrt scene was nullptr!");
        if (!scene->world) throw std::runtime_error( "pbrt scene->world was nullptr!");
        
        scene->makeSingleLevel();

        /* Setup Cameras */
        auto pbrt_camera = scene->cameras[0]; // TODO: Add support for more cameras
        auto camera_prefab = Prefabs::CreatePrefabCamera("fps", scene->film->resolution.x, scene->film->resolution.y, glm::radians(pbrt_camera->fov), 4, 1.0, true);
        
        auto frame = pbrt_camera->frame;
        glm::mat4 cam_xfm = pbrt_frame_to_glm(frame);
        std::cout<<"Camera frame is " << glm::to_string(cam_xfm) << std::endl;

        camera_prefab.transform->set_transform(cam_xfm); // This doesn't seem to work... Weird scaling issues.
        camera_prefab.transform->set_scale(1.0); // Forces correct camera scaling after transform decomposition
        
        /* Load the world */
        auto world = scene->world;
        auto instances = world->instances;

        std::unordered_map<std::shared_ptr<pbrt::Material>, Material*> material_map;
        std::unordered_map<std::shared_ptr<pbrt::Texture>, Texture*> texture_map;
        std::vector<Entity*> entities;

        /* For each instance */
        for (uint32_t inst_id = 0; inst_id < instances.size(); ++ inst_id) {
            auto instance = instances[inst_id];
            auto object = instance->object;
            auto xfm = instance->xfm;
            
            /* Extract the pbrt transform */
            glm::mat4 glm_xfm = pbrt_affine_to_glm(xfm);            
            auto shapes = object->shapes;

            std::cout<<"instance " << instance->toString() << std::endl;
            std::cout<<"instance->object " << object->toString() << std::endl;
            
            /* For each shape in this instance */
            for (uint32_t i = 0; i < shapes.size(); ++i) 
            {
                auto shape = shapes[i];
                std::cout<<"instance->object->shape[" << i << "]" << shape->toString() << std::endl;
                std::string mesh_name = name + "_" + std::to_string(inst_id) + "_" + std::to_string(i);
                std::string mat_name = name + "_" + std::to_string(inst_id) + "_" + std::to_string(i);
                std::string mesh_type = shape->toString();
                
                /* Create a transform component */
                auto tfm = Transform::Create(name + "_" + std::to_string(inst_id) + "_" + std::to_string(i));
                tfm->set_transform(glm_xfm);

                /* Look up / Create material component */
                Material* mat = get_material_from_pbrt(material_map, texture_map, basePath, mat_name, shape->material);

                /* Create mesh and (optional) light component */
                /* Many different types of shapes */

                Mesh* mesh  = nullptr;
                Light* light = nullptr;
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

                            /* In order for Pluto lights to work, the transform must be roughly in the center of the mesh.
                                So we need to compute the centroid, move the transform, then take away the translation from the points
                            */
                            glm::vec3 centroid(0.0);
                            for (int pos_idx = 0; pos_idx < positions.size(); pos_idx += 1)
                            {
                                centroid += positions[pos_idx];
                            }
                            centroid /= positions.size();

                            for (uint32_t pos_idx = 0; pos_idx < triangle_mesh->vertex.size(); ++pos_idx)
                            {
                                positions[pos_idx] -= centroid;
                            }
                            auto local_to_world = tfm->get_local_to_world_matrix();
                            auto additional_pos = local_to_world * glm::vec4(centroid.x, centroid.y, centroid.z, 1.0);

                            tfm->add_position(additional_pos.x, additional_pos.y, additional_pos.z);

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

                            if (triangle_mesh->areaLight) {
                                /* TODO: Determine if the mesh is a quad*/
                                std::cout<<triangle_mesh->areaLight->toString() << std::endl;
                                light = Light::Create(mesh_name);
                                // light->();
                                light->cast_shadows(true);
                                if (triangle_mesh->areaLight->toString().compare("DiffuseAreaLightRGB") == 0) {
                                    auto rgb_area_light = triangle_mesh->areaLight->as<pbrt::DiffuseAreaLightRGB>();
                                    light->set_color(rgb_area_light->L.x, rgb_area_light->L.y, rgb_area_light->L.z);
                                } else if (triangle_mesh->areaLight->toString().compare("DiffuseAreaLightBB") == 0) {
                                    auto bb_area_light = triangle_mesh->areaLight->as<pbrt::DiffuseAreaLightBB>();
                                    light->set_temperature(bb_area_light->temperature);
                                    light->set_intensity(bb_area_light->scale);
                                }
                            }
                        }
                    } else if (mesh_type.compare("Sphere") == 0) {
                        auto sphere = shape->as<pbrt::Sphere>();
                        mesh = Mesh::CreateSphere(mesh_name);
                        auto xfm = sphere->transform; // Need to add support for this...
                        auto local_to_world = tfm->get_local_to_world_matrix();
                        glm::mat4 glm_sphere_xfm = pbrt_affine_to_glm(xfm);

                        /* Accounting for radius using scale to allow analytical area lights to scale as well */
                        auto new_sphere_xfm = glm::scale(local_to_world * glm_sphere_xfm, glm::vec3(sphere->radius, sphere->radius, sphere->radius));
                        tfm->set_transform(new_sphere_xfm);

                        if (sphere->areaLight) {
                            std::cout<<sphere->areaLight->toString() << std::endl;
                            light = Light::Create(mesh_name);
                            light->use_sphere();
                            light->cast_shadows(true);
                            if (sphere->areaLight->toString().compare("DiffuseAreaLightRGB") == 0) {
                                auto rgb_area_light = sphere->areaLight->as<pbrt::DiffuseAreaLightRGB>();
                                light->set_color(rgb_area_light->L.x, rgb_area_light->L.y, rgb_area_light->L.z);
                            } else if (sphere->areaLight->toString().compare("DiffuseAreaLightBB") == 0) {
                                auto bb_area_light = sphere->areaLight->as<pbrt::DiffuseAreaLightBB>();
                                light->set_temperature(bb_area_light->temperature);
                                light->set_intensity(bb_area_light->scale);
                            }
                        }
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

                /* Add the entity to the list */
                entities.push_back(
                    Entity::Create(
                        name + "_" + std::to_string(inst_id) + "_" + std::to_string(i),
                        tfm, nullptr, mat, light, mesh, nullptr, nullptr)
                );                
            }
        }

        return entities;
    }
}

// Code for saving image once loaded. Might just ignore this
// auto instances = world->instances;
// for (uint32_t i = 0; i < instances.size(); ++i )
// {
//     auto instance = instances[i];
//     std::cout<<instance->toString()<<std::endl;
// }

// camera_prefab.camera->wait_for_render_complete();
// auto cam_texture = camera_prefab.camera->get_texture();
// auto framebuffer = cam_texture->download_color_data(scene->film->resolution.x, scene->film->resolution.y, 1);
// std::vector<uint8_t> c_framebuffer(framebuffer.size());
// for (uint32_t i = 0; i < framebuffer.size(); i++) {
//     c_framebuffer[i] = (uint8_t)(framebuffer[i] * 255);
// }
// stbi_write_png(scene->film->fileName.c_str(), scene->film->resolution.x, scene->film->resolution.y, 4, 
//     c_framebuffer.data(), scene->film->resolution.x * 4);