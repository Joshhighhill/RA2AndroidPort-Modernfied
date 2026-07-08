#pragma once
namespace ra2engine {
struct InputEvent {
    enum Kind { None, Quit, PointerDown, PointerUp, PointerMove } kind = None;
    int x = 0, y = 0;            // pointer position (window pixels)
};
class Platform {
public:
    static Platform create(const char* title, int w, int h);
    Platform(Platform&&) noexcept; Platform& operator=(Platform&&) noexcept;
    ~Platform();
    bool poll(InputEvent& ev);
    void begin_frame();
    void present();
    void* renderer() const;   // SDL_Renderer*
    int width() const; int height() const;
private:
    Platform() = default;
    struct Impl; Impl* p_ = nullptr;
};
}
