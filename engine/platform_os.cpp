#include "platform_os.hpp"

#ifdef __ANDROID__
#include <SDL.h>
#include <SDL_system.h>
#include <jni.h>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#endif

namespace ra2engine {

#ifdef _WIN32
namespace {
std::string utf8_of(const wchar_t* w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s(static_cast<std::size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    return s;
}
}  // namespace

std::string win_registry_install_dir() {
    for (REGSAM view : {static_cast<REGSAM>(KEY_WOW64_32KEY), static_cast<REGSAM>(KEY_WOW64_64KEY)}) {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Westwood\\Red Alert 2", 0, KEY_READ | view, &key) !=
            ERROR_SUCCESS)
            continue;
        wchar_t buf[1024] = {0};
        DWORD len = sizeof buf - sizeof(wchar_t), type = 0;
        const LONG rc = RegQueryValueExW(key, L"InstallPath", nullptr, &type, reinterpret_cast<BYTE*>(buf), &len);
        RegCloseKey(key);
        if (rc != ERROR_SUCCESS || type != REG_SZ) continue;
        std::string p = utf8_of(buf);
        // InstallPath names the executable (...\Ra2.exe); keep its folder.
        const std::size_t sep = p.find_last_of("\\/");
        const std::size_t dot = p.rfind('.');
        if (dot != std::string::npos && sep != std::string::npos && dot > sep) p.resize(sep);
        if (!p.empty()) return p;
    }
    return {};
}

std::string os_browse_folder(const char* title) {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    wchar_t wtitle[256];
    MultiByteToWideChar(CP_UTF8, 0, title ? title : "", -1, wtitle, 256);
    BROWSEINFOW bi{};
    bi.lpszTitle = wtitle;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    std::string out;
    if (PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi)) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) out = utf8_of(path);
        CoTaskMemFree(pidl);
    }
    if (SUCCEEDED(co)) CoUninitialize();
    return out;
}

void os_attach_parent_console() {
    if (GetConsoleWindow()) return;   // already a console process
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* f = nullptr;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
    }
}
bool os_log_to_file_if_no_console(const std::string& path) {
    if (GetConsoleWindow()) return false;   // a console process (ra2engine.exe)
    const HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    if (h != nullptr && h != INVALID_HANDLE_VALUE && GetFileType(h) != FILE_TYPE_UNKNOWN)
        return false;   // stderr already goes somewhere (a pipe / file)
    FILE* f = nullptr;
    if (freopen_s(&f, path.c_str(), "w", stderr) != 0) return false;
    freopen_s(&f, path.c_str(), "a", stdout);
    setvbuf(stderr, nullptr, _IONBF, 0);
    return true;
}
bool os_android_open_importer() { return false; }
#else
bool os_log_to_file_if_no_console(const std::string&) { return false; }
std::string win_registry_install_dir() { return {}; }
std::string os_browse_folder(const char*) { return {}; }
void os_attach_parent_console() {}
#ifdef __ANDROID__
bool os_android_open_importer() {
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject act = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !act) return false;
    jclass cls = env->GetObjectClass(act);
    jmethodID m = cls ? env->GetMethodID(cls, "openImporter", "()V") : nullptr;
    bool ok = false;
    if (m) {
        env->CallVoidMethod(act, m);
        ok = !env->ExceptionCheck();
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (cls) env->DeleteLocalRef(cls);
    env->DeleteLocalRef(act);
    return ok;
}
#else
bool os_android_open_importer() { return false; }
#endif
#endif

}  // namespace ra2engine
