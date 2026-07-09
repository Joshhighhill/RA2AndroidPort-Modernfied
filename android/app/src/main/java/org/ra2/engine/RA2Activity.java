package org.ra2.engine;

import org.libsdl.app.SDLActivity;

/**
 * Entry activity for the RA2 engine.
 *
 * SDLActivity loads libSDL2.so + libmain.so (see getLibraries()) and calls
 * SDL_main -- our engine/main.cpp main(), which on Android launches straight
 * into the interactive scene (touch drives tap-to-select / tap-to-move).
 *
 * No menus, no overrides: the stock SDLActivity behaviour is exactly what the
 * slice needs.
 */
public class RA2Activity extends SDLActivity {
}
