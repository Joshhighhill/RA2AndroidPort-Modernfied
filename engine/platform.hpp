#pragma once
#include <cstdint>
namespace ra2engine {
struct InputEvent {
    // P7: Key (a non-Escape key the UI consumes; `key` = one of the kKey*
    // codes below) and Text (typed UTF-8 in `text`, for the save description
    // edit). Escape stays Quit, unchanged.
    enum Kind { None, Quit, PointerDown, PointerUp, PointerMove, Key, Text } kind = None;
    int x = 0, y = 0;                       // pointer position (window pixels)
    enum Button { Left, Right } button = Left;  // valid on PointerDown/PointerUp
    bool shift = false;                     // Shift held at event time
    uint32_t t_ms = 0;                      // event timestamp (ms) for double-click
    int key = 0;                            // Key: kKey* code
    char text[32] = {0};                    // Text: NUL-terminated UTF-8
};
enum : int { kKeyBackspace = 1, kKeyEnter = 2, kKeyUp = 3, kKeyDown = 4, kKeyDelete = 5,
             kKeyF10 = 6, kKeyF1 = 7 /* P10 briefing */ };
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
