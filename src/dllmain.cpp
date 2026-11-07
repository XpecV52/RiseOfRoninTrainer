/* dllmain.cpp — Ronin 代理 DLL (dinput8.dll)
 *
 *   1) 代理转发 dinput8 全部导出，保证游戏正常初始化
 *   2) 后台线程等 Ronin.exe 主模块就绪（Steam DRM 解密完）
 *   3) 按 patch_table.c 的 AOB 表扫描并打补丁
 *
 * 原则：任何一步失败都不能让游戏起不来。
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <string.h>

#include "engine.h"
#include "patch_table.h"
#include "hook_data.h"

/* ---------------- 配置 ---------------- */
static char g_ini[MAX_PATH];
int CfgInt(const char* key, int def)
{
    return (int)GetPrivateProfileIntA("Ronin", key, def, g_ini);
}

/* ---------------- 代理 dinput8 ---------------- */
typedef HRESULT(WINAPI* PFN_DI8CREATE)(HINSTANCE, DWORD, const IID*, LPVOID*, LPUNKNOWN);

static HMODULE g_hReal = NULL;
static PFN_DI8CREATE g_pDI8Create = NULL;

static void LoadRealDinput8(void)
{
    char path[MAX_PATH];
    GetSystemDirectoryA(path, MAX_PATH);
    strncat_s(path, MAX_PATH, "\\dinput8.dll", _TRUNCATE);
    g_hReal = LoadLibraryA(path);
    if (g_hReal)
        g_pDI8Create = (PFN_DI8CREATE)GetProcAddress(g_hReal, "DirectInput8Create");
}

extern "C" __declspec(dllexport) HRESULT WINAPI
DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, const IID* riidltf,
                   LPVOID* ppvOut, LPUNKNOWN punkOuter)
{
    if (!g_pDI8Create) LoadRealDinput8();
    if (!g_pDI8Create) return E_FAIL;
    return g_pDI8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

/* ---------------- 应用补丁 ---------------- */
static void ApplyAll(const EngSection* secs, int nsec, EngReport* rep)
{
    memset(rep, 0, sizeof(*rep));
    for (int i = 0; i < g_patches_n; i++) {
        const CtPatch* pt = &g_patches[i];
        if (!pt->aob) continue;
        rep->aobs_total++;

        int hits = 0;
        uintptr_t addr = eng_scan_module(secs, nsec, pt->aob, &hits);
        if (!addr) { rep->patches_fail++; continue; }
        rep->aobs_found++;

        uintptr_t target = addr + pt->offset;

        /* patch 为空 = 只验证不修改 */
        if (!pt->patch || !*pt->patch) continue;
        if (pt->feature && *pt->feature && !CfgInt(pt->feature, 0)) {
            rep->patches_skip++;
            continue;
        }
        unsigned char bytes[64], msk[64];
        int n = eng_parse_aob(pt->patch, bytes, msk, 64);
        if (n <= 0) { rep->patches_fail++; continue; }
        if (eng_write(target, bytes, (size_t)n))
            rep->patches_ok++;
        else
            rep->patches_fail++;
    }
}

/* ---------------- hook 安装 ---------------- */
static void* g_cave = NULL;
static int   g_hooked = 0;

static int HexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void InstallHooks(const EngSection* secs, int nsec, EngReport* rep)
{
    /* 1. 先把要装的 hook 点扫出来（按 INI 开关过滤） */
    uintptr_t sites[64];
    int nsites = 0;
    uintptr_t lo = 0;
    for (int i = 0; i < g_hooks_n && nsites < 64; i++) {
        /* 该 hook 的功能开关关着就跳过 */
        if (g_hooks[i].feature && *g_hooks[i].feature &&
            !CfgInt(g_hooks[i].feature, 1)) {
            rep->hooks_skip++;
            sites[nsites++] = 0;
            continue;
        }
        int hits = 0;
        uintptr_t a = eng_scan_module(secs, nsec, g_hooks[i].aob, &hits);
        if (!a) { rep->hooks_skip++; sites[nsites++] = 0; continue; }
        if (!eng_check(a, g_hooks[i].old)) { rep->hooks_skip++; sites[nsites++] = 0; continue; }
        if (g_hooks[i].length < 5) { rep->hooks_skip++; sites[nsites++] = 0; continue; }
        sites[nsites++] = a;
        if (!lo) lo = a;
    }
    if (!lo) return;

    /* 2. 分配洞穴内存 */
    g_cave = eng_alloc_near(lo, CAVE_SIZE);
    if (!g_cave) return;

    /* 3. 复制洞穴原始字节 */
    unsigned char* dst = (unsigned char*)g_cave;
    for (int i = 0; i < CAVE_SIZE; i++) {
        int hi = HexVal(g_cave_hex[i * 2]);
        int l = HexVal(g_cave_hex[i * 2 + 1]);
        if (hi < 0 || l < 0) return;
        dst[i] = (unsigned char)((hi << 4) | l);
    }

    /* 4. 重定位：E9 rel32 指向游戏代码的，按新基址重算 */
    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
    for (int i = 0; i < g_relocs_n; i++) {
        int off = g_relocs[i].off;
        uintptr_t tgt = base + g_relocs[i].rva;
        uintptr_t from = (uintptr_t)g_cave + off;
        if (!eng_rel32_ok(from, tgt)) continue;
        int32_t rel = (int32_t)((int64_t)tgt - (int64_t)(from + 5));
        memcpy(dst + off + 1, &rel, 4);
    }

    /* 5. 逐条写跳转 */
    for (int i = 0; i < nsites && i < g_hooks_n; i++) {
        if (!sites[i]) continue;
        uintptr_t to = (uintptr_t)g_cave + g_hooks[i].cave_off;
        if (eng_write_jmp(sites[i], to)) {
            rep->hooks_ok++;
            g_hooked++;
        } else {
            rep->hooks_fail++;
        }
    }
}

/* ---------------- 主流程 ---------------- */
static DWORD WINAPI InitThread(LPVOID)
{
    /* 等 Steam DRM 把 .text 解密完 */
    Sleep(3000);

    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
    EngSection secs[32];
    int nsec = eng_parse_module(base, secs, 32);

    if (g_patches_n <= 0)
        return 0;

    EngReport rep;
    ApplyAll(secs, nsec, &rep);
    InstallHooks(secs, nsec, &rep);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        char exe[MAX_PATH];
        GetModuleFileNameA(NULL, exe, MAX_PATH);
        char* p = strrchr(exe, '\\');
        if (p) *(p + 1) = 0;
        lstrcpynA(g_ini, exe, MAX_PATH - 20);
        lstrcatA(g_ini, "Ronin_Cheats.ini");
        LoadRealDinput8();
        CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
    }
    return TRUE;
}
