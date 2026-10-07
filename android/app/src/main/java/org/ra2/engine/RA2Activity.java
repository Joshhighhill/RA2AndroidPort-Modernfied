package org.ra2.engine;

import android.content.Intent;

import org.libsdl.app.SDLActivity;

/**
 * The game activity. SDLActivity loads libSDL2.so + libmain.so and calls
 * SDL_main -- engine/main.cpp main(), which runs run_menu() (main menu ->
 * skirmish / campaign / load). When the player quits, main() returns, SDL
 * finishes this activity and onDestroy() ends the process (W2-AR-17: no stale
 * C++ statics on the next launch).
 *
 * The launcher is SetupActivity, which checks for the user's game files and
 * offers the in-app import before starting this activity (W2-AR-03/-04).
 */
public class RA2Activity extends SDLActivity {
    @Override
    protected void onDestroy() {
        super.onDestroy();   // SDLActivity joins the SDL (engine) thread here
        // W2-AR-17: the engine keeps process-wide state in C++ statics; once
        // the player has quit, end the process so the next launch re-enters
        // main() with fresh statics instead of reusing this process. Done
        // after SDL's own teardown (an exit() from the SDL thread crashed).
        if (isFinishing()) android.os.Process.killProcess(android.os.Process.myPid());
    }

    /**
     * Called from native code (the engine's missing-files page, IMPORT
     * button) on the SDL thread: open the importer. startActivity() is a
     * synchronous binder call and SetupActivity lives in its own process, so
     * the engine quitting right after this does not lose or kill it.
     */
    public void openImporter() {
        Intent i = new Intent(this, SetupActivity.class);
        i.putExtra(SetupActivity.EXTRA_IMPORT, true);
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(i);
    }
}
