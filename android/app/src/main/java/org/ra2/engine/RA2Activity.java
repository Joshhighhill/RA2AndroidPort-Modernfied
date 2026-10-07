package org.ra2.engine;

import android.content.Intent;

import org.libsdl.app.SDLActivity;

/**
 * The game activity. SDLActivity loads libSDL2.so + libmain.so and calls
 * SDL_main -- engine/main.cpp main(), which runs run_menu() (main menu ->
 * skirmish / campaign / load) and ends the process with exit() when the
 * player quits (W2-AR-17: no stale statics on the next launch).
 *
 * The launcher is SetupActivity, which checks for the user's game files and
 * offers the in-app import before starting this activity (W2-AR-03/-04).
 */
public class RA2Activity extends SDLActivity {
    /**
     * Called from native code (the engine's missing-files page, IMPORT
     * button) on the SDL thread: open the importer. startActivity() is a
     * synchronous binder call, and SetupActivity lives in its own process, so
     * the engine's exit() right after this does not lose or kill it.
     */
    public void openImporter() {
        Intent i = new Intent(this, SetupActivity.class);
        i.putExtra(SetupActivity.EXTRA_IMPORT, true);
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(i);
    }
}
