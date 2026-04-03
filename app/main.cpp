#include <Genesis/Genesis.h>

#include <cstdlib>
#include <iostream>

int main() {
    try {
        // Create window
        Genesis::Window window(1280, 720, "Genesis Engine");

        // Create and initialize the renderer
        Genesis::Renderer renderer;
        renderer.init(window);

        // Main loop
        while (!window.shouldClose()) {
            window.pollEvents();
            renderer.drawFrame();
        }

        // Clean shutdown
        renderer.shutdown();

    } catch (const std::exception& e) {
        Genesis::Logger::error("Fatal: {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
