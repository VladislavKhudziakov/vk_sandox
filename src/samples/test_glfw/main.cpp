
#include <hal/render/vk/context.hpp>
#include <hal/window/window.hpp>

using namespace sandbox::hal::render;

int main() {
    sandbox::hal::window window(800, 600, "sandbox window");

    while (!window.closed()) {
        window.main_loop();
    }

    return 0;
}
