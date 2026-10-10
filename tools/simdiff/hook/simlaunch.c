/*
 * simlaunch.c -- start a COPY of gamemd.exe suspended, inject simhook.dll,
 * resume, and wait for it to exit. 32-bit (must match the target).
 *
 *   simlaunch.exe <game-dir> <exe-name> <simhook.dll> [timeout-seconds]
 *
 * Configuration reaches the DLL through the inherited environment
 * (SIMHOOK_OUT, SIMHOOK_SEED, SIMHOOK_FRAMES, SIMHOOK_EVERY,
 * SIMHOOK_AUTOSTART, SIMHOOK_L0); see docs/re/simdiff-harness.md.
 * No file is patched; the injection lives only in the child's memory.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    char exe[MAX_PATH], dll[MAX_PATH], cmd[MAX_PATH + 16];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    void *mem;
    HANDLE th;
    DWORD code = 0, wait, timeout_ms;
    size_t len;

    if (argc < 4) {
        fprintf(stderr, "usage: simlaunch <game-dir> <exe-name> <simhook.dll> [timeout-s]\n");
        return 2;
    }
    timeout_ms = argc > 4 ? (DWORD)atoi(argv[4]) * 1000u : 20u * 60u * 1000u;
    snprintf(exe, sizeof exe, "%s\\%s", argv[1], argv[2]);
    if (!GetFullPathNameA(argv[3], sizeof dll, dll, NULL)) return 2;
    snprintf(cmd, sizeof cmd, "\"%s\"", exe);

    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessA(exe, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, argv[1], &si, &pi)) {
        fprintf(stderr, "CreateProcess(%s) failed: %lu\n", exe, GetLastError());
        return 1;
    }
    len = strlen(dll) + 1;
    mem = VirtualAllocEx(pi.hProcess, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem || !WriteProcessMemory(pi.hProcess, mem, dll, len, NULL)) {
        fprintf(stderr, "remote alloc/write failed: %lu\n", GetLastError());
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }
    th = CreateRemoteThread(pi.hProcess, NULL, 0,
                            (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"),
                            mem, 0, NULL);
    if (!th) {
        fprintf(stderr, "CreateRemoteThread failed: %lu\n", GetLastError());
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }
    WaitForSingleObject(th, 30000);
    GetExitCodeThread(th, &code);
    CloseHandle(th);
    if (!code) {
        fprintf(stderr, "LoadLibraryA(%s) failed in target\n", dll);
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }
    printf("injected %s (module %08lX), resuming pid %lu\n", dll, code, pi.dwProcessId);
    fflush(stdout);
    ResumeThread(pi.hThread);
    wait = WaitForSingleObject(pi.hProcess, timeout_ms);
    if (wait == WAIT_TIMEOUT) {
        fprintf(stderr, "timeout: terminating game\n");
        TerminateProcess(pi.hProcess, 3);
        return 3;
    }
    GetExitCodeProcess(pi.hProcess, &code);
    printf("game exited with %lu\n", code);
    return 0;
}
