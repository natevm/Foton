#include "./Prefabs.hxx"

#include "Pluto/Entity/Entity.hxx"
#include "Pluto/Camera/Camera.hxx"
#include "Pluto/Entity/Entity.hxx"

#include "Pluto/Systems/EventSystem/EventSystem.hxx"

#include "Pluto/Libraries/GLFW/GLFW.hxx"

#include <stdexcept>

CameraPrefab Prefabs::camera_prefab;

Prefabs::Prefabs() { }
Prefabs::~Prefabs() { }

CameraPrefab Prefabs::CreateArcBallCamera(uint32_t width, uint32_t height, float fov, uint32_t msaa_samples, float target, bool enable_depth_prepass) {
    auto es = Systems::EventSystem::Get();
    es->create_window("prefab_window", width, height);
    camera_prefab.camera = Camera::Create("prefab_camera", false, width, height, msaa_samples, 1, enable_depth_prepass);
    camera_prefab.transform = Transform::Create("prefab_camera");
    camera_prefab.entity = Entity::Create("prefab_camera");
    camera_prefab.entity->set_camera(camera_prefab.camera);
    camera_prefab.entity->set_transform(camera_prefab.transform);
    
    camera_prefab.camera->set_view(glm::lookAt(glm::vec3(0.0f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f), glm::vec3(0.0f, 0.0f, 1.0f)));
    camera_prefab.camera->set_perspective_projection(fov, width, height, .1);
    camera_prefab.target = target;

    camera_prefab.initialized = true;
    es->connect_camera_to_window("prefab_window", camera_prefab.camera);
    return camera_prefab;
}

bool last_cursor_pos_initialized = false;
glm::vec2 last_cursor_pos(2);
glm::vec2 curr_cursor_pos(2);
glm::vec2 dr(0);
glm::vec2 dp(0);

glm::vec3 screen_to_arcball(const glm::vec2 p){
    const float dist = glm::dot(p, p);
	// If we're on/in the sphere return the point on it
	if (dist <= 1.f){
		return glm::vec3(p.x, std::sqrt(1.f - dist), p.y);
	} else {
		// otherwise we project the point onto the sphere
		const glm::vec2 proj = glm::normalize(p);
		return glm::vec3(proj.x, 0.f, proj.y);
	}
}

glm::quat get_rotation_between(glm::vec3 u, glm::vec3 v)
{
  float k_cos_theta = glm::dot(u, v);
  float k = sqrt(glm::length2(u) * glm::length2(v));
  return glm::normalize(glm::quat(k_cos_theta + k, cross(u, v)));
}

void Prefabs::Update()
{
    using namespace Libraries;
    auto g = GLFW::Get();

    if (camera_prefab.initialized) {
        if (!last_cursor_pos_initialized) {
            last_cursor_pos = g->get_cursor_pos("prefab_window");
            last_cursor_pos_initialized = true;
        } else {
            last_cursor_pos = curr_cursor_pos;
        }

        // Get GLFW event data
        curr_cursor_pos                = g->get_cursor_pos("prefab_window");
        auto right_mouse_action = g->get_button_action("prefab_window", 1);
        auto left_mouse_action  = g->get_button_action("prefab_window", 0);
        auto left_mouse_mods    = g->get_button_mods("prefab_window", 0);

        auto left_arrow_action  = g->get_key_action("prefab_window", GLFW::get_key_code("LEFT"));
        auto right_arrow_action = g->get_key_action("prefab_window", GLFW::get_key_code("RIGHT"));
        auto up_arrow_action    = g->get_key_action("prefab_window", GLFW::get_key_code("UP"));
        auto down_arrow_action  = g->get_key_action("prefab_window", GLFW::get_key_code("DOWN"));

        auto w_action  = g->get_key_action("prefab_window", GLFW::get_key_code("W"));
        auto s_action = g->get_key_action("prefab_window", GLFW::get_key_code("S"));
        auto a_action    = g->get_key_action("prefab_window", GLFW::get_key_code("A"));
        auto d_action  = g->get_key_action("prefab_window", GLFW::get_key_code("D"));
        auto q_action  = g->get_key_action("prefab_window", GLFW::get_key_code("Q"));
        auto e_action  = g->get_key_action("prefab_window", GLFW::get_key_code("E"));

        auto window_extent = g->get_size("prefab_window");

        // # Compute delta position/rotation from event data
        if ((right_mouse_action == 1) || ((left_mouse_action == 1) && (left_mouse_mods == 2))) {
            dp[0] += (-.005f * (curr_cursor_pos[0] - last_cursor_pos[0]));
            dp[1] += (.005f * (curr_cursor_pos[1] - last_cursor_pos[1]));
        }

        if ((left_mouse_action == 1) && (left_mouse_mods != 2)) {
            dr[0] += (-.1f * (curr_cursor_pos[0] - last_cursor_pos[0]));
            dr[1] += (.1f * (curr_cursor_pos[1] - last_cursor_pos[1]));
        }

        if (left_arrow_action >= 1) {
            dr[0] += .5f;
        }

        if (right_arrow_action >= 1) {
            dr[0] -= .5f;
        }

        if (up_arrow_action >= 1) {
            dr[1] -= .5f;
        }

        if (down_arrow_action >= 1) {
            dr[1] += .5f;
        }

        auto up = camera_prefab.transform->get_up();
        auto right = camera_prefab.transform->get_right();
        auto forward = camera_prefab.transform->get_forward();
        auto p = camera_prefab.transform->get_position();


        if (a_action >= 1) p += right * .05f;
        if (d_action >= 1) p -= right * .05f;
        if (w_action >= 1) p -= forward * .05f;
        if (s_action >= 1) p += forward * .05f;            
        if (q_action >= 1) p -= up * .05f;
        if (e_action >= 1) p += up * .05f;

        dp *= .9f;
        dr *= .9f;
        
        // Move the camera
        right = camera_prefab.transform->get_right();
        up = camera_prefab.transform->get_up();

        p += (right * (float)dp[0]) + (up * (float)dp[1]);

        camera_prefab.transform->set_position(p);

        // Arcball camera controls 

        auto last_clamped = glm::clamp((curr_cursor_pos / window_extent) *  3.0f - 1.5f, glm::vec2{-1, -1}, glm::vec2{1, 1});
        auto curr_clamped = glm::clamp(((curr_cursor_pos + glm::vec2(dr.x, -dr.y))  / window_extent) *  3.0f - 1.5f, glm::vec2{-1, -1}, glm::vec2{1, 1});

        last_clamped.x *= -1;
        curr_clamped.x *= -1;

        auto current_rotation = camera_prefab.transform->get_rotation();
        glm::vec3 mouse_prev_ball = current_rotation * screen_to_arcball(last_clamped);
        glm::vec3 mouse_cur_ball = current_rotation * screen_to_arcball(curr_clamped);

        auto arcball = get_rotation_between(mouse_prev_ball, mouse_cur_ball);

        glm::quat identity_quaternion(1.0f, 0.0f, 0.0f, 0.0f);
        auto rotation = glm::slerp(identity_quaternion, arcball, glm::length(dr));
        camera_prefab.transform->rotate_around(camera_prefab.transform->get_position() + camera_prefab.transform->get_forward() * camera_prefab.target, arcball);
    }
}

