#include "platform.hpp"

#include <SDL.h>
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
                ev.kind = InputEvent::None;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            ev.kind = InputEvent::PointerDown;
            ev.x = e.button.x;
            ev.y = e.button.y;
            break;
        case SDL_MOUSEBUTTONUP:
            ev.kind = InputEvent::PointerUp;
            ev.x = e.button.x;
            ev.y = e.button.y;
            break;
        case SDL_MOUSEMOTION:
            ev.kind = InputEvent::PointerMove;
            ev.x = e.motion.x;
            ev.y = e.motion.y;
            break;
        case SDL_FINGERDOWN:
            ev.kind = InputEvent::PointerDown;
            ev.x = static_cast<int>(e.tfinger.x * p_->w);
            ev.y = static_cast<int>(e.tfinger.y * p_->h);
            break;
        case SDL_FINGERUP:
            ev.kind = InputEvent::PointerUp;
            ev.x = static_cast<int>(e.tfinger.x * p_->w);
            ev.y = static_cast<int>(e.tfinger.y * p_->h);
            break;
        case SDL_FINGERMOTION:
            ev.kind = InputEvent::PointerMove;
            ev.x = static_cast<int>(e.tfinger.x * p_->w);
            ev.y = static_cast<int>(e.tfinger.y * p_->h);
            break;
        default:
            ev.kind = InputEvent::None;
            break;
    }
    return true;
}

void Platform::begin_frame() {
    // Dark blue clear color.
    SDL_SetRenderDrawColor(p_->renderer, 10, 20, 60, 255);
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
