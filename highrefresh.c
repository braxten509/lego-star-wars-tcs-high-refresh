/*
 * LEGO Star Wars: The Complete Saga - high refresh rate mod (dinput8.dll proxy).
 *
 * 1. Runs the game at the display's refresh rate instead of 60 FPS.
 * 2. Replaces the game's Sleep()-based frame limiter (uneven pacing) with DXVK's.
 * 3. Makes character jumps/falls follow the exact 60 FPS trajectory at any
 *    higher frame rate. The engine moves a character by v*dt and then applies
 *    gravity (explicit Euler), so a jump peaks at v0^2/2g + v0*dt/2: the peak
 *    shrinks as dt shrinks. Adding a*(dt - 1/60)*dt/2 to the height on every
 *    airborne gravity step reproduces the 60 FPS curve exactly.
 *
 * Every patch checks the original bytes first, so an unknown executable is
 * left untouched.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAMETIME_ADDRESS      0x00925018 /* float: seconds simulated this frame */
#define LIMITER_FPS_ADDRESS    0x0075ec2c /* float 85.0: the game's own frame cap */
#define GRAVITY_STEP_ADDRESS   0x004ac3ad /* velocity.y += acceleration * FRAMETIME */
#define GRAVITY_RESUME_ADDRESS 0x004ac3bd

static const unsigned char gravity_step_original[16] = {
    0xd9, 0x05, 0x18, 0x50, 0x92, 0x00, /* fld  dword [FRAMETIME]   */
    0xd8, 0x4c, 0x24, 0x2c,             /* fmul dword [esp+0x2c]    */
    0xd8, 0x46, 0x6c,                   /* fadd dword [esi+0x6c]    */
    0xd9, 0x5e, 0x6c,                   /* fstp dword [esi+0x6c]    */
};

const float highrefresh_reference_frametime = 1.0f / 60.0f;
const float highrefresh_half = 0.5f;

/* esi = character, [esp+0x2c] = acceleration, [esi+0x60] = height,
   [esi+0x6c] = vertical velocity, [esi+0x281] = ground contact flags. */
__asm__(
    ".intel_syntax noprefix\n"
    ".globl _highrefresh_gravity_step\n"
    "_highrefresh_gravity_step:\n"
    "  fld dword ptr ds:[0x00925018]\n"
    "  fmul dword ptr [esp+0x2c]\n"
    "  fadd dword ptr [esi+0x6c]\n"
    "  fstp dword ptr [esi+0x6c]\n"
    "  cmp byte ptr [esi+0x281], 0\n"
    "  jne 1f\n"
    "  fld dword ptr [_highrefresh_reference_frametime]\n"
    "  fld dword ptr ds:[0x00925018]\n"
    "  fcomi st, st(1)\n"
    "  jae 2f\n"
    "  fsubrp st(1), st\n"                    /* st0 = dt - 1/60 */
    "  fmul dword ptr ds:[0x00925018]\n"
    "  fmul dword ptr [esp+0x2c]\n"
    "  fmul dword ptr [_highrefresh_half]\n"
    "  fadd dword ptr [esi+0x60]\n"
    "  fstp dword ptr [esi+0x60]\n"
    "  jmp 1f\n"
    "2:\n"
    "  fstp st(0)\n"
    "  fstp st(0)\n"
    "1:\n"
    "  push 0x004ac3bd\n"
    "  ret\n"
    ".att_syntax\n");
extern void highrefresh_gravity_step(void);

static void write_memory(void *address, const void *source, size_t size) {
    DWORD previous;
    VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &previous);
    memcpy(address, source, size);
    VirtualProtect(address, size, previous, &previous);
    FlushInstructionCache(GetCurrentProcess(), address, size);
}

static BOOL readable(const void *address, size_t size) {
    MEMORY_BASIC_INFORMATION info;
    if (!VirtualQuery(address, &info, sizeof info)) return FALSE;
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return FALSE;
    return (const char *)address + size <= (const char *)info.BaseAddress + info.RegionSize;
}

#ifdef HIGHREFRESH_PROBE
#pragma pack(push,1)
typedef struct {unsigned frame; void *obj; float y, vy, dt; unsigned char ctx, mv, grounded, pad; float x, z;} Rec;
#pragma pack(pop)
static FILE *out;
static unsigned char *tramp;
static void __cdecl record(void *obj) {
 Rec r; r.frame=*(unsigned*)0x0297dc00; r.obj=obj; r.y=*(float*)((char*)obj+0x60); r.vy=*(float*)((char*)obj+0x6c); r.dt=*(float*)0x00925018;
 r.ctx=*((unsigned char*)obj+0x7b5); r.mv=*((unsigned char*)obj+0x7b8); r.grounded=*((unsigned char*)obj+0x281); r.pad=0; r.x=*(float*)((char*)obj+0x5c); r.z=*(float*)((char*)obj+0x64);
 if(out){fwrite(&r,sizeof r,1,out); if((r.frame&63)==0) fflush(out);}
}

static void install_probe(void) {
    out = fopen("phys.bin", "wb");
    unsigned char *p = tramp = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    *p++ = 0x60; *p++ = 0xff; *p++ = 0x74; *p++ = 0x24; *p++ = 0x24;
    *p++ = 0xe8; *(int *)p = (int)((unsigned char *)record - (p + 4)); p += 4;
    *p++ = 0x83; *p++ = 0xc4; *p++ = 0x04; *p++ = 0x61;
    memcpy(p, (void *)0x004abac0, 9); p += 9;
    *p++ = 0xe9; *(int *)p = (int)((unsigned char *)0x004abac9 - (p + 4)); p += 4;
    unsigned char jmp[9] = {0xe9, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90};
    *(int *)(jmp + 1) = (int)(tramp - (unsigned char *)0x004abac5);
    write_memory((void *)0x004abac0, jmp, 9);
}
#endif

static void install_patches(void) {
#ifdef HIGHREFRESH_PROBE
    install_probe();
    if (getenv("LSW_NO_FIX")) return;
#endif
    const float current_limit = *(const float *)LIMITER_FPS_ADDRESS;
    if (current_limit == 85.0f) {
        const float unlimited = 100000.0f;
        write_memory((void *)LIMITER_FPS_ADDRESS, &unlimited, sizeof unlimited);
    }
    unsigned char jump[16];
    memset(jump, 0x90, sizeof jump);
    jump[0] = 0xe9;
    *(int *)(jump + 1) = (int)((unsigned char *)highrefresh_gravity_step - (unsigned char *)(GRAVITY_STEP_ADDRESS + 5));
    write_memory((void *)GRAVITY_STEP_ADDRESS, jump, sizeof jump);
}

/* The Steam executable is encrypted on disk; wait until it has unpacked itself. */
static DWORD WINAPI patch_thread(void *unused) {
    (void)unused;
    for (int attempt = 0; attempt < 6000; attempt++) {
        if (readable((void *)GRAVITY_STEP_ADDRESS, sizeof gravity_step_original) &&
            readable((void *)LIMITER_FPS_ADDRESS, sizeof(float)) &&
            memcmp((void *)GRAVITY_STEP_ADDRESS, gravity_step_original, sizeof gravity_step_original) == 0) {
            install_patches();
            break;
        }
        Sleep(5);
    }
    return 0;
}

/* The game always asks for a 60 Hz mode, which makes DXVK limit it to 60 FPS.
   Keep a dxvk.conf next to the game that paces it at the display's real rate.
   A dxvk.conf that this mod did not write is left alone. */
#define CONFIG_MARKER "# written by the LEGO Star Wars high refresh mod"
static void set_frame_rate_target(void) {
    DEVMODEW mode;
    memset(&mode, 0, sizeof mode);
    mode.dmSize = sizeof mode;
    if (!EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &mode)) return;
#ifdef HIGHREFRESH_PROBE
    if (getenv("LSW_FPS")) mode.dmDisplayFrequency = atoi(getenv("LSW_FPS"));
#endif
    if (mode.dmDisplayFrequency < 60) return;
    char wanted[128], existing[128] = "";
    snprintf(wanted, sizeof wanted, CONFIG_MARKER "\nd3d9.maxFrameRate = %lu\n", (unsigned long)mode.dmDisplayFrequency);
    FILE *file = fopen("dxvk.conf", "rb");
    if (file) {
        size_t length = fread(existing, 1, sizeof existing - 1, file);
        existing[length] = 0;
        fclose(file);
        if (strncmp(existing, CONFIG_MARKER, strlen(CONFIG_MARKER)) != 0) return;
        if (strcmp(existing, wanted) == 0) return;
    }
    file = fopen("dxvk.conf", "wb");
    if (!file) return;
    fputs(wanted, file);
    fclose(file);
}

static HMODULE system_dinput8;

HRESULT WINAPI DirectInput8Create(HINSTANCE instance, DWORD version, REFIID interface_id, void **out, void *outer) {
    if (!system_dinput8) {
        WCHAR path[MAX_PATH];
        GetSystemDirectoryW(path, MAX_PATH);
        lstrcatW(path, L"\\dinput8.dll");
        system_dinput8 = LoadLibraryW(path);
    }
    if (!system_dinput8) return E_FAIL;
    typedef HRESULT (WINAPI *Create)(HINSTANCE, DWORD, REFIID, void **, void *);
    Create create = (Create)GetProcAddress(system_dinput8, "DirectInput8Create");
    return create ? create(instance, version, interface_id, out, outer) : E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        set_frame_rate_target();
        HANDLE thread = CreateThread(NULL, 0, patch_thread, NULL, 0, NULL);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
