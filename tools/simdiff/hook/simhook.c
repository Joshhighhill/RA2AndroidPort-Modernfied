/*
 * simhook.c -- per-logic-frame state dumper for the ORIGINAL gamemd.exe.
 *
 * Part of the simdiff differential harness (docs/re/simdiff-harness.md).
 * Target build: Yuri's Revenge 1.000 gamemd.exe, PE timestamp 0x3B846665,
 * 4,808,976 bytes (the one in D:\Red Alert 2). Every address below is for
 * THAT build; the DLL refuses to patch anything if the timestamp or the
 * bytes at the hook site differ.
 *
 * Loaded into a COPY of gamemd.exe by simlaunch.exe (CreateProcess suspended
 * + LoadLibraryA remote thread). Nothing on disk is modified. In memory it:
 *   1. presets the session seed (0xA8DD00, read only by Init_Random
 *      FUN_0052F960) so a skirmish is seeded deterministically;
 *   2. redirects the main loop's `CALL Queue_AI` (0x55DA6D -> 0x646D00) to a
 *      stub that calls Queue_AI and then dumps the frame -- i.e. after
 *      LogicClass::AI and event execution, before ++Frame;
 *   3. optionally clicks through the shell (Single Player -> Skirmish ->
 *      Start Game) by posting BN_CLICKED to the shell dialogs.
 *
 * Record format (text, one file): see docs/re/simdiff-harness.md section 3.
 * Build: tools/simdiff/hook/build.ps1 (MSVC x86, /O1, no CRT dependency
 * beyond the static CRT).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef unsigned int u32;
typedef unsigned char u8;

/* ---- addresses (gamemd.exe YR 1.000) -------------------------------- */
#define A_FRAME        0x00A8DCEC  /* int Frame                              */
#define A_SEED         0x00A8DCFC  /* session Seed                           */
#define A_PRESETSEED   0x00A8DD00  /* preset seed, read at 0x52FB14 only     */
#define A_SESSIONMODE  0x00A8A1A0  /* SessionClass.GameMode (5 = skirmish)   */
#define A_SCENARIO     0x00A8A198  /* ScenarioClass*; +0x218 RandomClass     */
#define A_SCENNAME     0x00A8A848  /* char ScenarioName[0x200] (YRpp-0x1098) */
#define A_RNG2         0x00885B08  /* global RandomClass (twin)              */
#define A_CURCRC       0x00AC4164  /* CurrentFrameCRC                        */
#define A_SYNCDRAW     0x00AF91D0  /* FUN_006504D0's RNG sink                */
#define A_HOOKCALL     0x0055DA6D  /* E8 rel32 -> 0x646D00 inside main loop  */
#define A_QUEUEAI      0x00646D00
#define A_CRC          0x0064D520  /* ComputeFrameCRC                        */
#define A_FACING_CUR   0x004C92F0  /* FacingClass::Current(DirStruct*)       */
#define A_RAND_SEED    0x0065C110  /* RandomClass::RandomClass(seed) body    */
#define A_RAND_NEXT    0x0065C1C0  /* RandomClass::operator()()              */
#define A_RAND_RANGE   0x0065C220  /* RandomClass::operator()(lo,hi)         */
#define A_CP_LAUNCHER  0x0049F4D0  /* CopyProtect IsLauncherRunning (mutex)  */
#define A_CP_NOTIFY    0x0049F530  /* CopyProtect NotifyLauncher (event)     */

/* DynamicVectorClass: +4 Items, +0x10 Count */
#define V_INF    0x00A82D50
#define V_UNIT   0x008B3070
#define V_AIR    0x00A8D2F8
#define V_BLD    0x00A8DAA8
#define V_LOGIC  0x0087E6F8
#define V_HOUSE  0x00A7F190
#define V_LAYER0 0x0089F2D0        /* 5 x LayerClass, stride 0x18 */

#define RNG_BYTES 1012             /* u8 disabled; pad; i32 idx1; i32 idx2; u32 tab[250] */

/* object field offsets (identical to YRpp's 1.001 layout for these) */
#define O_UID      0x10
#define O_HEALTH   0x6C
#define O_LOC      0x9C
#define O_OWNER    0x21C
#define O_FACING   0x388           /* PrimaryFacing (body)  */
#define O_TURRET   0x3A0           /* SecondaryFacing (units) */
#define VF_WHATAMI 0x2C
#define VF_OWNERH  0x3C
#define VF_GETTYPE 0x88
#define VF_MISSION 0x184
#define H_ARRAYIDX 0x30
#define T_ID       0x24

enum { RTTI_UNIT = 1, RTTI_AIRCRAFT = 2, RTTI_ANIM = 4, RTTI_BUILDING = 6, RTTI_INFANTRY = 15 };

#define RD32(a) (*(volatile u32 *)(a))

typedef void (__cdecl *VoidFn)(void);
typedef u32 (__fastcall *ThisNoArg)(void *self);
typedef void *(__fastcall *ThisOut)(void *self, void *edx, void *out);
typedef void (__fastcall *ThisU32)(void *self, void *edx, u32 v);
typedef int (__fastcall *ThisRange)(void *self, void *edx, int lo, int hi);

static FILE *g_out;
static char g_outpath[MAX_PATH];
static u32 g_seed = 0x2A2A2A2A;
static int g_max_frames = 3000;
static int g_every = 1;
static int g_autostart = 1;
static int g_l0 = 0;
static int g_frames_seen = 0;
static char g_logpath[MAX_PATH];

static void hlog(const char *fmt, ...)
{
    FILE *f = fopen(g_logpath, "a");
    va_list ap;
    if (!f) return;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static u32 fnv1a(const u8 *p, size_t n)
{
    u32 h = 2166136261u;
    size_t i;
    for (i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
    return h;
}

static void rng_state(const u8 *r, int *i1, int *i2, u32 *h, int *dis)
{
    *dis = r[0];
    *i1 = *(const int *)(r + 4);
    *i2 = *(const int *)(r + 8);
    *h = fnv1a(r + 12, 250 * 4);
}

static u32 vcall0(void *obj, u32 off)
{
    u32 fn = *(u32 *)(*(u32 *)obj + off);
    return ((ThisNoArg)fn)(obj);
}

static unsigned facing16(u8 *obj, u32 off)
{
    u32 tmp = 0;
    u32 *r = (u32 *)((ThisOut)A_FACING_CUR)(obj + off, 0, &tmp);
    return (unsigned)(*r & 0xFFFF);
}

/* The CRC the game would compute in a multiplayer session at this point.
 * FUN_0064D520 consumes two Scenario->Random draws (one in the CRC, one in
 * FUN_006504D0); both RNGs and the touched globals are restored, so the
 * skirmish's own random stream is left exactly as it was. */
static u32 frame_crc(void)
{
    static u8 s1[RNG_BYTES], s2[RNG_BYTES];
    u8 *scen = *(u8 **)A_SCENARIO;
    u32 crc, c0 = RD32(A_CURCRC), d0 = RD32(A_SYNCDRAW);
    memcpy(s1, scen + 0x218, RNG_BYTES);
    memcpy(s2, (void *)A_RNG2, RNG_BYTES);
    ((VoidFn)A_CRC)();
    crc = RD32(A_CURCRC);
    memcpy(scen + 0x218, s1, RNG_BYTES);
    memcpy((void *)A_RNG2, s2, RNG_BYTES);
    *(volatile u32 *)A_CURCRC = c0;
    *(volatile u32 *)A_SYNCDRAW = d0;
    return crc;
}

static void dump_frame(void)
{
    u8 *scen = *(u8 **)A_SCENARIO;
    int frame = (int)RD32(A_FRAME);
    int i, n, a1, a2, d1, b1, b2, d2;
    u32 h1, h2, crc;
    int census[64];
    u8 **items;

    if (!scen) return;
    crc = frame_crc();
    rng_state(scen + 0x218, &a1, &a2, &h1, &d1);
    rng_state((u8 *)A_RNG2, &b1, &b2, &h2, &d2);
    fprintf(g_out, "F %d crc %08X rng %d %d %08X rng2 %d %d %08X vec %d %d %d %d logic %d",
            frame, crc, a1, a2, h1, b1, b2, h2,
            (int)RD32(V_INF + 0x10), (int)RD32(V_UNIT + 0x10), (int)RD32(V_AIR + 0x10),
            (int)RD32(V_BLD + 0x10), (int)RD32(V_LOGIC + 0x10));
    fprintf(g_out, " layers");
    for (i = 0; i < 5; ++i) fprintf(g_out, " %d", (int)RD32(V_LAYER0 + 0x18 * i + 0x10));

    memset(census, 0, sizeof census);
    items = *(u8 ***)(V_LOGIC + 4);
    n = (int)RD32(V_LOGIC + 0x10);
    for (i = 0; i < n; ++i) {
        u32 w = vcall0(items[i], VF_WHATAMI);
        census[w & 63]++;
    }
    fprintf(g_out, " cen");
    for (i = 0; i < 64; ++i) if (census[i]) fprintf(g_out, " %d:%d", i, census[i]);
    fputc('\n', g_out);

    /* per-techno records in Logic order */
    for (i = 0; i < n; ++i) {
        u8 *o = items[i];
        u32 w = vcall0(o, VF_WHATAMI);
        u8 *type, *house;
        int owner, x, y, z, hp, mission;
        unsigned f, t;
        if (w != RTTI_UNIT && w != RTTI_AIRCRAFT && w != RTTI_BUILDING && w != RTTI_INFANTRY) continue;
        type = (u8 *)vcall0(o, VF_GETTYPE);
        house = (u8 *)vcall0(o, VF_OWNERH);
        owner = house ? *(int *)(house + H_ARRAYIDX) : -1;
        x = *(int *)(o + O_LOC); y = *(int *)(o + O_LOC + 4); z = *(int *)(o + O_LOC + 8);
        hp = *(int *)(o + O_HEALTH);
        mission = (int)(signed char)(vcall0(o, VF_MISSION) & 0xFF);
        f = facing16(o, O_FACING);
        t = (w == RTTI_UNIT) ? facing16(o, O_TURRET) : 0xFFFFu;
        fprintf(g_out, "O %u %u %s %d %d %d %d %u %d %d %d\n",
                (unsigned)w, *(u32 *)(o + O_UID), type ? (char *)(type + T_ID) : "?",
                owner, x, y, z, f, (int)(t == 0xFFFFu ? -1 : (int)t), hp, mission);
    }
}

static void l0_dump(void)
{
    /* L0 rung: seed a private RandomClass with the game's own seeding code
     * and record raw + ranged draws, so the clone's GameRandom can be checked
     * without running a scenario. */
    static u8 r[RNG_BYTES];
    char path[MAX_PATH];
    FILE *f;
    int i;
    snprintf(path, sizeof path, "%s.l0", g_outpath);
    f = fopen(path, "w");
    if (!f) return;
    memset(r, 0, sizeof r);
    ((ThisU32)A_RAND_SEED)(r, 0, g_seed);
    fprintf(f, "L0 seed %08X draws %d\n", g_seed, g_l0);
    for (i = 0; i < g_l0; ++i) {
        u32 v = ((ThisNoArg)A_RAND_NEXT)(r);
        fprintf(f, "%08X\n", v);
    }
    /* ranged draws over a fixed ladder of ranges, continuing the stream */
    {
        static const int ranges[][2] = { {0, 1}, {0, 2}, {0, 99}, {-5, 5}, {0, 255}, {1, 6}, {0, 1000}, {0, 0x7FFF} };
        for (i = 0; i < g_l0; ++i) {
            const int *rg = ranges[i % 8];
            int v = ((ThisRange)A_RAND_RANGE)(r, 0, rg[0], rg[1]);
            fprintf(f, "R %d %d %d\n", rg[0], rg[1], v);
        }
    }
    fclose(f);
}

static void __cdecl hk_queue_ai(void)
{
    int frame;
    ((VoidFn)A_QUEUEAI)();
    frame = (int)RD32(A_FRAME);
    if (RD32(A_SESSIONMODE) == 0) return; /* campaign/shell: not our run */
    if (!g_out) {
        char *scen = (char *)A_SCENNAME;
        g_out = fopen(g_outpath, "w");
        if (!g_out) return;
        setvbuf(g_out, NULL, _IOFBF, 1 << 20);
        fprintf(g_out, "# simdiff v1 src=gamemd-1.000 seed=%08X session=%d scenario=%.64s every=%d\n",
                RD32(A_SEED), (int)RD32(A_SESSIONMODE), scen, g_every);
        hlog("first dumped frame %d seed %08X scenario %.64s", frame, RD32(A_SEED), scen);
    }
    if (g_every <= 1 || frame % g_every == 0) {
        __try {
            dump_frame();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            fprintf(g_out, "# exception while dumping frame %d\n", frame);
        }
    }
    ++g_frames_seen;
    if ((g_frames_seen & 63) == 0) fflush(g_out);
    if (frame >= g_max_frames) {
        fprintf(g_out, "# end frame %d\n", frame);
        fclose(g_out);
        g_out = NULL;
        hlog("done at frame %d", frame);
        TerminateProcess(GetCurrentProcess(), 0);
    }
}

/* ---- shell automation ------------------------------------------------ */
typedef struct { int id; HWND dlg; } FindCtx;

static BOOL CALLBACK find_child(HWND h, LPARAM lp)
{
    FindCtx *c = (FindCtx *)lp;
    HWND it = GetDlgItem(h, c->id);
    if (it && IsWindowVisible(h) && IsWindowEnabled(it)) { c->dlg = h; return FALSE; }
    return TRUE;
}

static BOOL CALLBACK find_top(HWND h, LPARAM lp)
{
    FindCtx *c = (FindCtx *)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId()) return TRUE;
    find_child(h, lp);
    if (c->dlg) return FALSE;
    EnumChildWindows(h, find_child, lp);
    return c->dlg ? FALSE : TRUE;
}

static HWND find_dialog_with(int id)
{
    FindCtx c; c.id = id; c.dlg = NULL;
    EnumWindows(find_top, (LPARAM)&c);
    return c.dlg;
}

static DWORD WINAPI shell_thread(LPVOID p)
{
    /* dialog 226 (main) -> 1667 Single Player; dialog 256 -> 1401 Skirmish;
     * dialog 258 (skirmish setup) -> 1559 Start Game. Settings come from
     * RA2MD.INI [Skirmish] in the game copy. */
    static const int steps[] = { 1667, 1401, 1559 };
    int s = 0, tries = 0;
    (void)p;
    while (s < 3 && tries < 2400) {
        HWND d = find_dialog_with(steps[s]);
        Sleep(250);
        ++tries;
        if (!d) continue;
        Sleep(1500); /* let the shell's slide-in animation finish */
        hlog("shell: click %d", steps[s]);
        PostMessageA(d, WM_COMMAND, MAKEWPARAM(steps[s], BN_CLICKED), (LPARAM)GetDlgItem(d, steps[s]));
        /* wait for the dialog to go away before looking for the next one */
        {
            int w = 0;
            while (w++ < 80 && find_dialog_with(steps[s]) == d) Sleep(250);
        }
        ++s;
    }
    hlog("shell thread exit (step %d)", s);
    return 0;
}

static int env_int(const char *name, int def)
{
    char b[64];
    DWORD n = GetEnvironmentVariableA(name, b, sizeof b);
    return (n && n < sizeof b) ? (int)strtoul(b, NULL, 0) : def;
}

static void patch_ret1(u8 *fn)
{
    static const u8 ret1[3] = { 0xB0, 0x01, 0xC3 }; /* mov al,1 ; ret */
    DWORD old;
    if (VirtualProtect(fn, 3, PAGE_EXECUTE_READWRITE, &old)) {
        memcpy(fn, ret1, 3);
        VirtualProtect(fn, 3, old, &old);
        FlushInstructionCache(GetCurrentProcess(), fn, 3);
    }
}

static int install(void)
{
    u8 *base = (u8 *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    u8 *site = (u8 *)A_HOOKCALL;
    static const u8 expect[5] = { 0xE8, 0x8E, 0x92, 0x0E, 0x00 };
    DWORD old;
    int rel;
    if (nt->FileHeader.TimeDateStamp != 0x3B846665u) {
        hlog("refusing: exe timestamp %08X is not YR 1.000 (3B846665)", nt->FileHeader.TimeDateStamp);
        return 0;
    }
    if (memcmp(site, expect, 5) != 0) {
        hlog("refusing: unexpected bytes at hook site %08X", A_HOOKCALL);
        return 0;
    }
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old)) return 0;
    rel = (int)((u8 *)hk_queue_ai - (site + 5));
    memcpy(site + 1, &rel, 4);
    VirtualProtect(site, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    *(volatile u32 *)A_PRESETSEED = g_seed;
    /* Run without the YURI.exe launcher (same idea as yrpp-spawner's
     * CopyProtection patches at 1.001 0x49F5C0/0x49F620): IsLauncherRunning
     * and NotifyLauncher return true. Validate (0x49F6B0) already does. */
    patch_ret1((u8 *)A_CP_LAUNCHER);
    patch_ret1((u8 *)A_CP_NOTIFY);
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)h; (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DWORD n = GetEnvironmentVariableA("SIMHOOK_OUT", g_outpath, sizeof g_outpath);
        if (!n || n >= sizeof g_outpath) strcpy(g_outpath, "simdump.txt");
        snprintf(g_logpath, sizeof g_logpath, "%s.log", g_outpath);
        g_seed = (u32)env_int("SIMHOOK_SEED", (int)g_seed);
        if (g_seed == 0) g_seed = 1; /* 0 means "not preset" to Init_Random */
        g_max_frames = env_int("SIMHOOK_FRAMES", g_max_frames);
        g_every = env_int("SIMHOOK_EVERY", 1);
        g_autostart = env_int("SIMHOOK_AUTOSTART", 1);
        g_l0 = env_int("SIMHOOK_L0", 0);
        hlog("simhook attach: seed %08X frames %d every %d autostart %d", g_seed, g_max_frames, g_every, g_autostart);
        if (!install()) return TRUE; /* stay loaded but inert */
        hlog("hook installed");
        if (g_l0 > 0) l0_dump();
        if (g_autostart) CloseHandle(CreateThread(NULL, 0, shell_thread, NULL, 0, NULL));
    }
    return TRUE;
}
