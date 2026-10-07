// W2-AR-09: Android lifecycle inside GameScene (textually included in the
// class body). Desktop never receives these events.
//
// Backgrounded (Home, a call, the app switcher): the game is PAUSED by opening
// the in-game options menu (the update() pause the original's modal menu
// loop gives), and the session is written to Saved Games/AUTOSAVE.SAV --
// the low-memory killer may end a 0.7-1.4 GB process while it is in the
// background. On return the menu is up, so play resumes only when the player
// chooses. The autosave is removed when the game ends normally (this
// scene's destructor), so a leftover one means the process was killed, and
// run_menu offers to resume it at the next launch. [Q4: a platform norm,
// not a traced behaviour of the original.]
static std::string autosave_path() {
    const std::string d = save_games_dir();
    return d.empty() ? std::string() : d + "/AUTOSAVE.SAV";
}
bool lifecycle_event(const ra2engine::InputEvent& ev) {
    switch (ev.kind) {
        case ra2engine::InputEvent::AppBackground: {
            if (!menu_.open_ && !sld_.is_open() && !gf_abort_.open()) menu_.open();   // pause
            const std::string p = autosave_path();
            if (!p.empty() && !game_over_for_autosave()) {
                const Uint32 t0 = SDL_GetTicks();
                const bool ok = save_game(p, "AUTOSAVE");
                autosaved_ = autosaved_ || ok;
                SDL_Log("[lifecycle] background: paused, autosave %s (%u ms) -> %s", ok ? "ok" : "FAILED",
                        SDL_GetTicks() - t0, p.c_str());
            }
            return true;
        }
        case ra2engine::InputEvent::AppForeground:
            SDL_Log("[lifecycle] foreground: game stays paused in the options menu");
            return true;
        case ra2engine::InputEvent::AppLowMemory:
        case ra2engine::InputEvent::RenderReset:
            return true;   // the driver already trimmed / cleared the texture cache
        default:
            return false;
    }
}
// No autosave once the outcome is decided (the result screen follows).
bool game_over_for_autosave() const {
    return gf_outcome_ != 0 || (campaign_ && campaign_->outcome() != 0);
}
void lifecycle_teardown() {
    if (!autosaved_) return;
    const std::string p = autosave_path();
    if (!p.empty()) {
        std::error_code ec;
        std::filesystem::remove(p, ec);
    }
}
bool autosaved_ = false;
