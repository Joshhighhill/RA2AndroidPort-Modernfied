#pragma once
// W2-AR-22: small OS hooks kept out of main.cpp (which must not see
// <windows.h>). Every function is a harmless no-op / empty result on
// platforms that lack the facility.
#include <string>
namespace ra2engine {
// The retail install's registry InstallPath folder (HKLM SOFTWARE/Westwood/
// Red Alert 2, 32- then 64-bit view), "" when absent or not on Windows.
std::string win_registry_install_dir();
// A native folder picker; "" when cancelled or unsupported (non-Windows).
std::string os_browse_folder(const char* title);
// Desktop "attach to a console when there is one" for a GUI-subsystem build,
// so --help/--version/--play still print when run from a terminal.
void os_attach_parent_console();
// W2-AR-04 (Android): open the in-app game-file importer (SetupActivity,
// via RA2Activity.openImporter()); false off Android or on a JNI failure.
bool os_android_open_importer();
}  // namespace ra2engine
