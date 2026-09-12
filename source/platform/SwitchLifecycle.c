#include <stdbool.h>
#include <switch.h>

static bool g_exit_locked = false;
static bool g_romfs_initialized = false;
static bool g_hidsys_initialized = false;
static bool g_pl_initialized = false;
static bool g_setsys_initialized = false;
static bool g_set_initialized = false;

void userAppInit(void) {
    // Borealis で実際に使う service だけを初期化する。
    appletLockExit();
    g_exit_locked = true;
    g_romfs_initialized = R_SUCCEEDED(romfsInit());
    g_hidsys_initialized = R_SUCCEEDED(hidsysInitialize());
    g_pl_initialized = R_SUCCEEDED(plInitialize(PlServiceType_User));
    g_setsys_initialized = R_SUCCEEDED(setsysInitialize());
    g_set_initialized = R_SUCCEEDED(setInitialize());
}

void userAppExit(void) {
    if (g_set_initialized) {
        setExit();
    }
    if (g_setsys_initialized) {
        setsysExit();
    }
    if (g_pl_initialized) {
        plExit();
    }
    if (g_hidsys_initialized) {
        hidsysExit();
    }
    if (g_romfs_initialized) {
        romfsExit();
    }
    if (g_exit_locked) {
        appletUnlockExit();
    }
}
