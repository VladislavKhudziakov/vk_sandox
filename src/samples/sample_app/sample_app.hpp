#pragma once

#include <window/window.hpp>
#include <render/vk/vulkan_dependencies.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/constants.hpp>

#include <optional>

namespace sandbox
{
    class sample_app
    {
    public:
        explicit sample_app(
            uint32_t init_window_width = 800,
            uint32_t init_window_height = 600,
            const std::string& name = "sample_app");

        void main_loop();

    protected:
        using keyboard_event = hal::keyboard_event;
        using mouse_button_event = hal::mouse_button_event;
        using cursor_positon_event = hal::cursor_positon_event;
        using scroll_event = hal::scroll_event;

        class main_camera
        {
        public:
            enum class projection_type
            {
                perspective,
                ortho
            };

            void set_fov(float rad);
            void set_zner(float n);
            void set_zfar(float f);

            void move_forward(float m);
            void move_backward(float m);
            void move_left(float m);
            void move_right(float m);

            void update(uint32_t fb_w, uint32_t fb_h);

            void rotate_x(float rad);
            void rotate_y(float rad);

            glm::mat4 get_proj_matrix() const;
            glm::mat4 get_view_matrix() const;

        private:
            projection_type m_projection_type{projection_type::perspective};

            float m_fov = glm::half_pi<float>();
            float m_znear = 0.01;
            float m_zfar = 100;

            float m_move_forward_distance{0};
            float m_move_backward_distance{0};
            float m_move_left_distance{0};
            float m_move_right_distance{0};

            float m_x_angle{0};
            float m_y_angle{0};

            glm::vec3 m_eye_position{0, 0, 0};

            glm::mat4 m_view_matrix{1};
            glm::mat4 m_proj_matrix{1};

            float m_screen_width{};
            float m_screen_height{};
        };

        virtual void update(uint64_t);

        main_camera& get_main_camera();

        std::pair<uint32_t, uint32_t> get_window_framebuffer_size() const;

        virtual void create_render_passes() = 0;
        virtual void create_framebuffers(uint32_t width, uint32_t height) = 0;
        virtual void create_sync_primitives() = 0;
        virtual void init_assets() = 0;

        virtual vk::Image get_final_image() = 0;
        virtual const std::vector<vk::Semaphore>& get_wait_semaphores() = 0;
        virtual const std::vector<vk::Fence>& get_wait_fences() = 0;

        virtual void on_key_pressed(const keyboard_event& e);
        virtual void on_mouse_clicked(const mouse_button_event& e);
        virtual void on_mouse_moved(const cursor_positon_event& e);
        virtual void on_mouse_scroll(const scroll_event& e);

    private:
        glm::ivec2 m_last_mouse_position{-1, -1};
        glm::ivec2 m_curr_mouse_position{-1, -1};

        enum camera_move_state
        {
            CAMERA_MOVE_LEFT = 1,
            CAMERA_MOVE_RIGHT = 2,
            CAMERA_MOVE_FORWARD = 4,
            CAMERA_MOVE_BACKWARD = 8
        };

        class sample_app_window_update_listener;
        friend class sample_app_window_update_listener;

        std::unique_ptr<hal::window> m_window{};

        int32_t m_camera_move_bits{0};

        float m_last_dt{0};

        std::string m_name{};

        uint32_t m_framebuffer_width{800};
        uint32_t m_framebuffer_height{600};

        uint32_t m_key_pressed_listener_handler{};
        uint32_t m_mouse_clicked_handler{};
        uint32_t m_mouse_moved_handler{};
        int32_t m_mouse_scroll_handler{};

        main_camera m_main_camera{};
    };
} // namespace sandbox
