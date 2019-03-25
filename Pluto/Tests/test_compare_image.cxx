#include <iostream>
#include <thread>
#include <future>

#include "gtest/gtest.h"

#include "Pluto/Pluto.hxx"
#include "Pluto/Entity/Entity.hxx"
#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Transform/Transform.hxx"
#include "Pluto/Texture/Texture.hxx"

#include "Pluto/Systems/RenderSystem/RenderSystem.hxx"

#include "stb_image_write.h"

TEST(save_image, test_compare_image)
{
    auto func = []() {
        try {
            auto rs = Systems::RenderSystem::Get();

            /* An asynchronous thread requests systems to stop */
            Pluto::Initialize();
            
            /* Make a camera */
            auto cam_entity    = Entity::Create("cam_entity");
            auto cam_camera    = Camera::Create("cam_entity");
            cam_camera->set_clear_color(1.0, 0.0, 0.0, 1.0);
            auto cam_transform = Transform::Create("cam_entity");

            cam_entity->set_camera(cam_camera);
            cam_entity->set_transform(cam_transform);

            /* The camera will automatically start to asynchronously render. */
            cam_camera->wait_for_render_complete();

            auto cam_texture = cam_camera->get_texture();
            auto framebuffer = cam_texture->download_color_data(512, 512, 1);

            std::vector<uint8_t> c_framebuffer(framebuffer.size());
            for (uint32_t i = 0; i < framebuffer.size(); i++) {
                c_framebuffer[i] = (uint8_t)(framebuffer[i] * 255);
            }

            stbi_write_png("test_red_image.png", 512, 512, 4, c_framebuffer.data(), 512 * 4);


            Pluto::StopSystems();
        }
        catch (std::runtime_error &e) {
            Pluto::StopSystems();
            FAIL() << "Runtime Error " << e.what();
        }
    };

    // Must be on the main thread, runs windowing events
    Pluto::StartSystems(false, func);
    Pluto::WaitForStartupCallback();
}