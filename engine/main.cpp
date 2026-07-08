#include "platform.hpp"

#include <SDL.h>

int main(int, char**) {
    ra2engine::Platform platform = ra2engine::Platform::create("RA2 Engine", 1024, 768);

    bool running = true;
    while (running) {
        ra2engine::InputEvent ev;
        while (platform.poll(ev)) {
            if (ev.kind == ra2engine::InputEvent::Quit) {
                running = false;
            }
        }

        platform.begin_frame();
        platform.present();

        SDL_Delay(16);  // ~60fps
    }

    return 0;
}
