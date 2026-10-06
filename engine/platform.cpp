#include "platform.hpp"

#include <SDL.h>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace ra2engine {

struct Platform::Impl {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    int w = 0;
    int h = 0;
};

Platform Platform::create(const char* title, int w, int h) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        throw std::runtime_error(SDL_GetError());
    }

    SDL_Window* window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h,
        SDL_WINDOW_SHOWN);
    if (!window) {
        std::string err = SDL_GetError();
        SDL_Quit();
        throw std::runtime_error(err);
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::string err = SDL_GetError();
        SDL_DestroyWindow(window);
        SDL_Quit();
        throw std::runtime_error(err);
    }

    // Fix a LOGICAL render resolution (w x h, the size the engine's scene
    // layout/camera code is written against) decoupled from the window's
    // actual PHYSICAL pixel size. SDL scales all rendering to fill the real
    // window/framebuffer and transparently remaps mouse/finger event
    // coordinates into logical space for us. This matters because:
    //  - Desktop: SDL_CreateWindow honors (w, h) directly, so physical ==
    //    logical and this is a no-op in practice; resizing the window still
    //    scales correctly instead of clipping.
    //  - Android: SDL_CreateWindow is forced fullscreen at the device's real
    //    resolution (the requested w/h is ignored), so without a logical
    //    size the scene would render at 1280x720 in the corner of a larger
    //    framebuffer and touch coordinates (normalized 0..1, scaled by the
    //    logical w/h in Platform::poll) would no longer line up with what's
    //    drawn. Setting the logical size keeps rendering AND input in the
    //    same fixed coordinate space on both platforms.
    SDL_RenderSetLogicalSize(renderer, w, h);

    Platform p;
    p.p_ = new Impl{window, renderer, w, h};
    return p;
}

Platform::Platform(Platform&& other) noexcept : p_(other.p_) {
    other.p_ = nullptr;
}

Platform& Platform::operator=(Platform&& other) noexcept {
    if (this != &other) {
        if (p_) {
            SDL_DestroyRenderer(p_->renderer);
            SDL_DestroyWindow(p_->window);
            delete p_;
            SDL_Quit();
        }
        p_ = other.p_;
        other.p_ = nullptr;
    }
    return *this;
}

Platform::~Platform() {
    if (p_) {
        SDL_DestroyRenderer(p_->renderer);
        SDL_DestroyWindow(p_->window);
        delete p_;
        SDL_Quit();
    }
}

bool Platform::poll(InputEvent& ev) {
    SDL_Event e;
    if (!SDL_PollEvent(&e)) {
        ev.kind = InputEvent::None;
        return false;
    }

    switch (e.type) {
        case SDL_QUIT:
            ev.kind = InputEvent::Quit;
            break;
        case SDL_KEYDOWN:
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                ev.kind = InputEvent::Quit;
            } else {
                // P7: the few keys the save/load UI needs; everything else stays None.
                switch (e.key.keysym.sym) {
                    case SDLK_BACKSPACE: ev.key = kKeyBackspace; break;
                    case SDLK_RETURN: case SDLK_KP_ENTER: ev.key = kKeyEnter; break;
                    case SDLK_UP: ev.key = kKeyUp; break;
                    case SDLK_DOWN: ev.key = kKeyDown; break;
                    case SDLK_DELETE: ev.key = kKeyDelete; break;
                    case SDLK_F10: ev.key = kKeyF10; break;
                    case SDLK_F1: ev.key = kKeyF1; break;
                    default: ev.key = 0; break;
                }
                ev.kind = ev.key ? InputEvent::Key : InputEvent::None;
            }
            break;
        case SDL_TEXTINPUT:
            ev.kind = InputEvent::Text;
            std::snprintf(ev.text, sizeof ev.text, "%s", e.text.text);
            break;
        case SDL_MOUSEBUTTONDOWN:
            ev.kind = InputEvent::PointerDown;
            ev.x = e.button.x;
            ev.y = e.button.y;
            ev.button = (e.button.button == SDL_BUTTON_RIGHT) ? InputEvent::Right
                                                              : InputEvent::Left;
            ev.shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
            ev.t_ms = e.button.timestamp;
            break;
        case SDL_MOUSEBUTTONUP:
            ev.kind = InputEvent::PointerUp;
            ev.x = e.button.x;
            ev.y = e.button.y;
            ev.button = (e.button.button == SDL_BUTTON_RIGHT) ? InputEvent::Right
                                                              : InputEvent::Left;
            ev.shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
            ev.t_ms = e.button.timestamp;
            break;
        case SDL_MOUSEMOTION:
            ev.kind = InputEvent::PointerMove;
            ev.x = e.motion.x;
            ev.y = e.motion.y;
            ev.t_ms = e.motion.timestamp;
            break;
        case SDL_FINGERDOWN:
            ev.kind = InputEvent::PointerDown;
            ev.x = static_cast<int>(e.tfinger.x * p_->w);
            ev.y = static_cast<int>(e.tfinger.y * p_->h);
            ev.button = InputEvent::Left;  // touch maps to the left/primary button
            ev.t_ms = e.tfinger.timestamp;
            break;
        case SDL_FINGERUP:
            ev.kind = InputEvent::PointerUp;
            ev.x = static_cast<int>(e.tfinger.x * p_->w);
            ev.y = static_cast<int>(e.tfinger.y * p_->h);
            ev.button = InputEvent::Left;
            ev.t_ms = e.tfinger.timestamp;
            break;
        case SDL_FINGERMOTION:
            ev.kind = InputEvent::PointerMove;
            ev.x = static_cast<int>(e.tfinger.x * p_->w);
            ev.y = static_cast<int>(e.tfinger.y * p_->h);
            ev.t_ms = e.tfinger.timestamp;
            break;
        default:
            ev.kind = InputEvent::None;
            break;
    }
    return true;
}

void Platform::begin_frame() {
    // G-88: black, like the original (unmapped / shrouded area is black).
    SDL_SetRenderDrawColor(p_->renderer, 0, 0, 0, 255);
    SDL_RenderClear(p_->renderer);
}

void Platform::present() {
    SDL_RenderPresent(p_->renderer);
}

void* Platform::renderer() const {
    return p_->renderer;
}

int Platform::width() const {
    return p_->w;
}

int Platform::height() const {
    return p_->h;
}

}  // namespace ra2engine
