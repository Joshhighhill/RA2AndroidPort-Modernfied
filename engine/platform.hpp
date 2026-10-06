#pragma once
#include <cstdint>
namespace ra2engine {
struct InputEvent {
    // P7: Key (a key the UI consumes; `key` = one of the kKey* codes below, or 0
    // for a key that only carries a `vk`) and Text (typed UTF-8 in `text`, for
    // the save description edit).
    // Pan (a touch pan gesture; dx/dy in window pixels) is produced only by the
    // touch gesture layer.
    enum Kind { None, Quit, PointerDown, PointerUp, PointerMove, Key, Text, Pan } kind = None;
    int x = 0, y = 0;                       // pointer position (window pixels)
    enum Button { Left, Right } button = Left;  // valid on PointerDown/PointerUp
    bool shift = false;                     // Shift held at event time
    bool ctrl = false;                      // Ctrl held at event time
    bool alt = false;                       // Alt held at event time
    bool touch = false;                     // produced by a finger, not a mouse
    uint32_t t_ms = 0;                      // event timestamp (ms) for double-click
    int key = 0;                            // Key: kKey* code (0 = vk-only key)
    // Key: the Windows virtual-key code of the pressed key -- the code space
    // KeyboardMD.ini [Hotkey] uses (A..Z = 65..90, 0..9 = 48..57, F1 = 112,
    // Esc = 27, Space = 32, Home = 36, ...). 0 = unmapped key.
    int vk = 0;
    int dx = 0, dy = 0;                     // Pan: camera delta (window pixels)
    char text[32] = {0};                    // Text: NUL-terminated UTF-8
};
enum : int { kKeyBackspace = 1, kKeyEnter = 2, kKeyUp = 3, kKeyDown = 4, kKeyDelete = 5,
             kKeyF10 = 6, kKeyF1 = 7 /* P10 briefing */, kKeyEscape = 8 /* G-01 */ };

// G0: anything that yields InputEvents -- the real Platform, or the --play
// harness's scripted source. poll() returns false when no event is pending.
struct EventSource {
    virtual bool poll(InputEvent& ev) = 0;
    virtual ~EventSource() = default;
};

// Translate a key press (SDL keycode + SDL modifier mask, passed as plain ints
// so this header stays SDL-free) into the InputEvent the Platform emits. Shared
// by Platform::poll and the --play harness's `key` command, so scripted keys go
// through the exact production mapping.
void key_event_from_sdl(int sdl_keycode, int sdl_mod, InputEvent& ev);

class Platform : public EventSource {
public:
    // hidden = true creates an invisible window with the software renderer (the
    // --play harness: deterministic pixels, no visible desktop needed).
    static Platform create(const char* title, int w, int h, bool hidden = false);
    Platform(Platform&&) noexcept; Platform& operator=(Platform&&) noexcept;
    ~Platform() override;
    bool poll(InputEvent& ev) override;
    void begin_frame();
    void present();
    void* renderer() const;   // SDL_Renderer*
    int width() const; int height() const;
    // G-89: physical window size (RA2.INI [Video]) and desktop fullscreen;
    // rendering stays at the logical size given to create().
    void set_window_size(int w, int h);
    void set_fullscreen(bool on);
private:
    Platform() = default;
    struct Impl; Impl* p_ = nullptr;
};
}
