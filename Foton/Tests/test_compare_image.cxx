// experimental/filesystem doesn't seem to be available on MacOS
// #include <iostream>
// #include <thread>
// #include <future>
// #include <experimental/filesystem>

// #include "gtest/gtest.h"

// #include "Foton/Tools/version.h"

// #include "Foton/Foton.hxx"
// #include "Foton/Entity/Entity.hxx"
// #include "Foton/Camera/Camera.hxx"
// #include "Foton/Transform/Transform.hxx"
// #include "Foton/Texture/Texture.hxx"

// #include "Foton/Systems/RenderSystem/RenderSystem.hxx"

// #include "stb_image_write.h"

// namespace fs = std::experimental::filesystem;

// TEST(save_image, test_compare_image)
// {
//     auto func = []() {
//         try {
//             auto rs = Systems::RenderSystem::Get();

//             /* An asynchronous thread requests systems to stop */
//             Foton::Initialize(true, false, {"VK_LAYER_LUNARG_standard_validation"});
            
//             /* Make a camera */
//             auto cam_entity    = Entity::Create("cam_entity");
//             auto cam_camera    = Camera::Create("cam_entity");
//             cam_camera->set_clear_color(1.0, 0.0, 0.0, 1.0);
//             auto cam_transform = Transform::Create("cam_entity");

//             cam_entity->set_camera(cam_camera);
//             cam_entity->set_transform(cam_transform);

//             /* The camera will automatically start to asynchronously render. */
//             cam_camera->wait_for_render_complete();

//             auto cam_texture = cam_camera->get_texture();
//             auto framebuffer = cam_texture->download_color_data(512, 512, 1);

//             std::vector<uint8_t> c_framebuffer(framebuffer.size());
//             for (uint32_t i = 0; i < framebuffer.size(); i++) {
//                 c_framebuffer[i] = (uint8_t)(framebuffer[i] * 255);
//             }

//             auto path = fs::path(std::string(CODE_VERSION) + std::string("/save_image"));
//             fs::create_directories(path);

//             auto red_path = (path / fs::path("/test_red_image.png")).u8string();
//             auto green_path = (path / fs::path("/test_green_image.png")).u8string();
//             auto blue_path = (path / fs::path("/test_blue_image.png")).u8string();
            
//             stbi_write_png(red_path.c_str(), 512, 512, 4, c_framebuffer.data(), 512 * 4);

//             cam_camera->set_clear_color(0.0, 1.0, 0.0, 1.0);
//             cam_camera->wait_for_render_complete();


//             framebuffer = cam_texture->download_color_data(512, 512, 1);
//             for (uint32_t i = 0; i < framebuffer.size(); i++) {
//                 c_framebuffer[i] = (uint8_t)(framebuffer[i] * 255);
//             }

//             stbi_write_png(green_path.c_str(), 512, 512, 4, c_framebuffer.data(), 512 * 4);

//             cam_camera->set_clear_color(0.0, 0.0, 1.0, 1.0);
//             cam_camera->wait_for_render_complete();

//             framebuffer = cam_texture->download_color_data(512, 512, 1);
//             for (uint32_t i = 0; i < framebuffer.size(); i++) {
//                 c_framebuffer[i] = (uint8_t)(framebuffer[i] * 255);
//             }

//             stbi_write_png(blue_path.c_str(), 512, 512, 4, c_framebuffer.data(), 512 * 4);

//             Foton::StopSystems();
//         }
//         catch (std::runtime_error &e) {
//             Foton::StopSystems();
//             FAIL() << "Runtime Error " << e.what();
//         }
//     };

//     // Must be on the main thread, runs windowing events
//     Foton::StartSystems(false, func);
//     Foton::WaitForStartupCallback();
// }