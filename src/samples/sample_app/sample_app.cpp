

#include "sample_app.hpp"

#include <render/vk/context.hpp>
#include <render/vk/errors_handling.hpp>
#include <window/vk_main_loop_update_listener.hpp>
#include <utils/scope_helpers.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>

using namespace sandbox;
using namespace sandbox::hal::render;

class sample_app::sample_app_window_update_listener : public hal::vk_main_loop_update_listener
{
public:
    explicit sample_app_window_update_listener(sample_app* app)
        : m_app(app)
    {
    }

    ~sample_app_window_update_listener() override = default;

    void on_swapchain_reset(size_t width, size_t height) override
    {
        width = std::max(width, size_t(1));
        height = std::max(height, size_t(1));
        m_app->m_framebuffer_width = width;
        m_app->m_framebuffer_height = height;
        m_app->m_main_camera.update(width, height);
        m_app->create_framebuffers(width, height);
    }

    vk::Image get_present_image() const override
    {
        return m_app->get_final_image();
    }


    vk::ImageLayout get_present_image_layout() const override
    {
        return vk::ImageLayout::eTransferSrcOptimal;
    }


    vk::Extent2D get_present_image_size() const override
    {
        return {
            m_app->m_framebuffer_width,
            m_app->m_framebuffer_height};
    }


    std::vector<vk::Semaphore> get_wait_semaphores() const override
    {
        return m_app->get_wait_semaphores();
    }


    void on_window_initialized() override
    {
        m_app->create_render_passes();
        m_app->create_sync_primitives();
        m_app->init_assets();
    }


    void update(uint64_t dt) override
    {
        m_app->update(dt);
    }

private:
    sample_app* m_app;
};


sample_app::sample_app(uint32_t init_window_width, uint32_t init_window_height, const std::string& name)
    : m_name(name)
    , m_framebuffer_width(init_window_width)
    , m_framebuffer_height(init_window_height)
{
}


#include <glm/gtx/string_cast.hpp>
void sandbox::sample_app::update(uint64_t dt)
{
    m_last_dt = double(dt) / 1e6;

    constexpr auto move_speed = 0.01;

    if (m_camera_move_bits & CAMERA_MOVE_LEFT) {
        m_main_camera.move_left(move_speed);
    }

    if (m_camera_move_bits & CAMERA_MOVE_RIGHT) {
        m_main_camera.move_right(move_speed);
    }

    if (m_camera_move_bits & CAMERA_MOVE_FORWARD) {
        m_main_camera.move_forward(move_speed);
    }

    if (m_camera_move_bits & CAMERA_MOVE_BACKWARD) {
        m_main_camera.move_backward(move_speed);
    }

    auto dpos = m_curr_mouse_position - m_last_mouse_position;

    constexpr float sensivity = 0.25;
    
    if (m_last_mouse_position.x >= 0) {
        if (m_curr_mouse_position.x <= 1) {
            m_main_camera.rotate_x(glm::radians(float(-1) * sensivity));
        } else if (m_curr_mouse_position.x >= m_framebuffer_width - 1) {
            m_main_camera.rotate_x(glm::radians(float(1) * sensivity));
        } else {
            m_main_camera.rotate_x(glm::radians(float(dpos.x) * sensivity));
        }

        if (m_curr_mouse_position.y <= 1) {
            m_main_camera.rotate_y(glm::radians(float(-1) * sensivity));
        } else if (m_curr_mouse_position.y >= m_framebuffer_height - 1) {
            m_main_camera.rotate_y(glm::radians(float(1) * sensivity));
        } else {
            m_main_camera.rotate_y(glm::radians(float(dpos.y) * sensivity));
        }
    }

    m_last_mouse_position = m_curr_mouse_position;
    
    m_main_camera.update(m_framebuffer_width, m_framebuffer_height);

    const auto& fences = get_wait_fences();
    if (!fences.empty()) {
        VK_CALL(avk::context::device()->waitForFences(fences, VK_TRUE, UINT64_MAX));
        avk::context::device()->resetFences(fences);
    }
}


sandbox::sample_app::main_camera& sandbox::sample_app::get_main_camera()
{
    return m_main_camera;
}


void sandbox::sample_app::main_loop()
{
    m_window = hal::window::create(m_framebuffer_width, m_framebuffer_height, m_name, std::make_unique<sample_app_window_update_listener>(this));

    m_key_pressed_listener_handler = m_window->subscribe_listenner([this](const hal::keyboard_event& e) {
        on_key_pressed(e);
    });

    m_mouse_moved_handler = m_window->subscribe_listenner([this](const hal::cursor_positon_event& e) {
        on_mouse_moved(e);
    });

    m_mouse_clicked_handler = m_window->subscribe_listenner([this](const hal::mouse_button_event& e) {
        on_mouse_clicked(e);
    });

    m_mouse_scroll_handler = m_window->subscribe_listenner([this](const hal::scroll_event& e) {
        on_mouse_scroll(e);
    });

    auto _ = utils::on_scope_exit([this]() {
        m_window->unsubscribe_listenner<keyboard_event>(m_key_pressed_listener_handler);
        m_window->unsubscribe_listenner<cursor_positon_event>(m_mouse_moved_handler);
        m_window->unsubscribe_listenner<mouse_button_event>(m_mouse_clicked_handler);
        m_window->unsubscribe_listenner<scroll_event>(m_mouse_scroll_handler);
    });

    try {
        while (!m_window->closed()) {
            m_window->main_loop();
        }
        avk::context::device()->waitIdle();

    } catch (const std::exception& e) {
        spdlog::error(e.what());
        avk::context::device()->waitIdle();
    }
}


std::pair<uint32_t, uint32_t> sandbox::sample_app::get_window_framebuffer_size() const
{
    return std::make_pair(m_framebuffer_width, m_framebuffer_height);
}


void sandbox::sample_app::on_key_pressed(const keyboard_event& e)
{
    switch (e.name) {
        case keyboard_event::key_name::key_A:
            switch (e.state) {
                case hal::press_state::press:
                    m_camera_move_bits |= CAMERA_MOVE_LEFT;
                    break;
                case hal::press_state::release:
                    m_camera_move_bits &= ~CAMERA_MOVE_LEFT;
                    break;
            }
            break;
        case keyboard_event::key_name::key_D:
            switch (e.state) {
                case hal::press_state::press:
                    m_camera_move_bits |= CAMERA_MOVE_RIGHT;
                    break;
                case hal::press_state::release:
                    m_camera_move_bits &= ~CAMERA_MOVE_RIGHT;
                    break;
            }
            break;
        case keyboard_event::key_name::key_W:
            switch (e.state) {
                case hal::press_state::press:
                    m_camera_move_bits |= CAMERA_MOVE_FORWARD;
                    break;
                case hal::press_state::release:
                    m_camera_move_bits &= ~CAMERA_MOVE_FORWARD;
                    break;
            }
            break;
        case keyboard_event::key_name::key_S:
            switch (e.state) {
                case hal::press_state::press:
                    m_camera_move_bits |= CAMERA_MOVE_BACKWARD;
                    break;
                case hal::press_state::release:
                    m_camera_move_bits &= ~CAMERA_MOVE_BACKWARD;
                    break;
            }
            break;
    }
}


void sandbox::sample_app::on_mouse_clicked(const mouse_button_event& e)
{
}

void sandbox::sample_app::on_mouse_moved(const cursor_positon_event& e)
{
    m_curr_mouse_position = {e.x, e.y};
}


void sandbox::sample_app::on_mouse_scroll(const scroll_event& e)
{
}


void sandbox::sample_app::main_camera::set_fov(float rad)
{
    m_fov = rad;
}


void sandbox::sample_app::main_camera::set_zner(float n)
{
    m_znear = n;
}


void sandbox::sample_app::main_camera::set_zfar(float f)
{
    m_zfar = f;
}


void sandbox::sample_app::main_camera::move_forward(float m)
{
    m_move_forward_distance = m;
}


void sandbox::sample_app::main_camera::move_backward(float m)
{
    m_move_backward_distance = m;
}


void sandbox::sample_app::main_camera::move_left(float m)
{
    m_move_left_distance = m;
}


void sandbox::sample_app::main_camera::move_right(float m)
{
    m_move_right_distance = m;
}


void sandbox::sample_app::main_camera::update(uint32_t fb_w, uint32_t fb_h)
{
    m_screen_width = fb_w;
    m_screen_height = fb_h;

    glm::vec3 target = glm::vec3{0, 0, 1};

    auto m = glm::yawPitchRoll(-m_x_angle, m_y_angle, 0.0f);

    target = glm::normalize(m * glm::vec4{target, 1});

    m_eye_position += target * m_move_forward_distance;
    m_eye_position -= target * m_move_backward_distance;

    auto left_vector = cross(target, {0, 1, 0});

    m_eye_position -= left_vector * m_move_left_distance;
    m_eye_position += left_vector * m_move_right_distance;

    m_move_forward_distance = 0;
    m_move_backward_distance = 0;
    m_move_left_distance = 0;
    m_move_right_distance = 0;
}


void sandbox::sample_app::main_camera::rotate_x(float rad)
{
    m_x_angle += rad;
}


void sandbox::sample_app::main_camera::rotate_y(float rad)
{
    m_y_angle = std::clamp<float>(m_y_angle + rad, -glm::half_pi<float>(), glm::half_pi<float>());
}


glm::mat4 sandbox::sample_app::main_camera::get_proj_matrix() const
{
    switch (m_projection_type) {
        case sandbox::sample_app::main_camera::projection_type::perspective:
            return glm::perspectiveFov(m_fov, m_screen_width, m_screen_height, m_znear, m_zfar);
        case sandbox::sample_app::main_camera::projection_type::ortho:
            return glm::ortho(-m_screen_width / 2, m_screen_width / 2, -m_screen_height / 2, m_screen_height / 2, m_znear, m_zfar);
        default:
            return glm::mat4{1};
    }
}


glm::mat4 sandbox::sample_app::main_camera::get_view_matrix() const
{
    glm::vec3 target =  glm::vec3{0, 0, 1};

    auto m = glm::yawPitchRoll(-m_x_angle, m_y_angle, 0.0f);

    target = m * glm::vec4{target, 1};

    glm::mat4 view_m = glm::lookAt(m_eye_position, m_eye_position + target, {0, 1, 0});

    return view_m;
}
