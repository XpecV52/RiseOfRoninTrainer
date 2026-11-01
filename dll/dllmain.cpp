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
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "engine.h"
#include "patch_table.h"
#include "hook_data.h"

#define RONIN_BUILD_TAG "0.1-skeleton"

/* ---------------- 日志 ---------------- */
static char g_log[MAX_PATH];
static CRITICAL_SECTION g_logcs;
static int g_log_ready = 0;
static int g_verbose = 0;          /* 0 = 不写任何日志（release 默认） */

static void LogInit(void)
{
    InitializeCriticalSection(&g_logcs);
    g_log_ready = 1;
}

void Log(const char* fmt, ...)
{
    if (!g_verbose || !g_log_ready) return;
    EnterCriticalSection(&g_logcs);
    FILE* f = NULL;
    if (fopen_s(&f, g_log, "a") == 0 && f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        va_list ap;
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fputc('\n', f);
        fclose(f);
    }
    LeaveCriticalSection(&g_logcs);
}

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
        if (!addr) {
            Log("  [MISS] %-8s  %s", pt->name, pt->aob);
            rep->patches_fail++;
            continue;
        }
        rep->aobs_found++;

        uintptr_t target = addr + pt->offset;
        int orig_ok = eng_check(target, pt->orig);
        Log("  [HIT ] %-8s @%p hits=%d orig=%s", pt->name, (void*)target, hits,
            orig_ok ? "ok" : "MISMATCH");

        /* patch 为空 = 只验证不修改 */
        if (!pt->patch || !*pt->patch) continue;
        if (pt->feature && *pt->feature && !CfgInt(pt->feature, 0)) {
            Log("         skip (ini off)");
            rep->patches_skip++;
            continue;
        }
        unsigned char bytes[64], msk[64];
        int n = eng_parse_aob(pt->patch, bytes, msk, 64);
        if (n <= 0) { rep->patches_fail++; continue; }
        if (eng_write(target, bytes, (size_t)n)) {
            Log("         PATCHED %d B", n);
            rep->patches_ok++;
        } else {
            Log("         WRITE FAILED");
            rep->patches_fail++;
        }
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
    if (!CfgInt("enable_hooks", 0)) {
        Log("hooks: disabled (enable_hooks=0)");
        return;
    }

    /* 1. 先把所有 hook 点扫出来 */
    uintptr_t sites[64];
    int nsites = 0;
    uintptr_t lo = 0;
    for (int i = 0; i < g_hooks_n && nsites < 64; i++) {
        int hits = 0;
        uintptr_t a = eng_scan_module(secs, nsec, g_hooks[i].aob, &hits);
        if (!a) { Log("  hook[%d] AOB MISS", i); rep->hooks_skip++; sites[nsites++] = 0; continue; }
        if (!eng_check(a, g_hooks[i].old)) {
            Log("  hook[%d] orig mismatch @%p", i, (void*)a);
            rep->hooks_skip++; sites[nsites++] = 0; continue;
        }
        if (g_hooks[i].length < 5) {
            Log("  hook[%d] 原长度 %d < 5，无法写 E9，跳过", i, g_hooks[i].length);
            rep->hooks_skip++; sites[nsites++] = 0; continue;
        }
        sites[nsites++] = a;
        if (!lo) lo = a;
    }
    if (!lo) { Log("hooks: 没有可用站点"); return; }

    /* 2. 分配洞穴内存 */
    uintptr_t target = lo;
    g_cave = eng_alloc_near(target, CAVE_SIZE);
    if (!g_cave) { Log("hooks: eng_alloc_near 失败"); return; }
    Log("hooks: cave @%p (target %p, delta %lld)", g_cave, (void*)target,
        (long long)((intptr_t)g_cave - (intptr_t)target));

    /* 3. 复制洞穴原始字节 */
    unsigned char* dst = (unsigned char*)g_cave;
    for (int i = 0; i < CAVE_SIZE; i++) {
        int hi = HexVal(g_cave_hex[i * 2]);
        int l = HexVal(g_cave_hex[i * 2 + 1]);
        if (hi < 0 || l < 0) { Log("hooks: cave hex 解析失败 @%d", i); return; }
        dst[i] = (unsigned char)((hi << 4) | l);
    }

    /* 4. 重定位：E9 rel32 指向游戏代码的，按新基址重算 */
    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
    int nrel = 0;
    for (int i = 0; i < g_relocs_n; i++) {
        int off = g_relocs[i].off;
        uintptr_t tgt = base + g_relocs[i].rva;
        uintptr_t from = (uintptr_t)g_cave + off;
        if (!eng_rel32_ok(from, tgt)) {
            Log("hooks: reloc[%d] 超出 rel32 范围", i);
            continue;
        }
        int32_t rel = (int32_t)((int64_t)tgt - (int64_t)(from + 5));
        memcpy(dst + off + 1, &rel, 4);
        nrel++;
    }
    Log("hooks: 重定位 %d/%d 条", nrel, g_relocs_n);

    /* 5. 逐条写跳转 */
    int maxh = CfgInt("max_hooks", 1);
    if (maxh <= 0 || maxh > g_hooks_n) maxh = g_hooks_n;
    Log("hooks: 限制安装 %d 条", maxh);
    for (int i = 0; i < nsites && i < g_hooks_n && i < maxh; i++) {
        if (!sites[i]) continue;
        uintptr_t to = (uintptr_t)g_cave + g_hooks[i].cave_off;
        if (eng_write_jmp(sites[i], to)) {
            Log("  hook[%d] installed @%p -> +%#x", i, (void*)sites[i], g_hooks[i].cave_off);
            rep->hooks_ok++;
            g_hooked++;
        } else {
            Log("  hook[%d] write_jmp 失败 @%p", i, (void*)sites[i]);
            rep->hooks_fail++;
        }
    }
    Log("hooks: 安装 %d 条", g_hooked);
}

/* ---------------- 主流程 ---------------- */
static DWORD WINAPI InitThread(LPVOID)
{
    /* 等 Steam DRM 把 .text 解密完 */
    Sleep(3000);

    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
    Log("=== Ronin Cheats %s ===", RONIN_BUILD_TAG);
    Log("module base = %p   ini = %s", (void*)base, g_ini);

    EngSection secs[32];
    int nsec = eng_parse_module(base, secs, 32);
    Log("sections = %d", nsec);
    for (int i = 0; i < nsec; i++)
        Log("  [%d] %-8s base=%p size=%#zx exec=%d", i,
            secs[i].name ? secs[i].name : "?", (void*)secs[i].base, secs[i].size, secs[i].exec);

    /* 自检：.text 是否已解密（搜一个极常见的序言） */
    int probe = 0;
    eng_scan_module(secs, nsec, "48 89 5C 24 08 48 89 74 24 10", &probe);
    Log("decrypt probe: %d hits  (%s)", probe, probe ? ".text 已明文" : ".text 仍是密文或特征过窄");

    if (g_patches_n <= 0) {
        Log("patch table empty");
        return 0;
    }

    EngReport rep;
    ApplyAll(secs, nsec, &rep);
    InstallHooks(secs, nsec, &rep);
    Log("done: aob %d/%d, ok %d, skip %d, fail %d",
        rep.aobs_found, rep.aobs_total, rep.patches_ok, rep.patches_skip, rep.patches_fail);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        LogInit();
        char exe[MAX_PATH];
        GetModuleFileNameA(NULL, exe, MAX_PATH);
        char* p = strrchr(exe, '\\');
        if (p) *(p + 1) = 0;
        _snprintf_s(g_ini, MAX_PATH, _TRUNCATE, "%sRonin_Cheats.ini", exe);
        _snprintf_s(g_log, MAX_PATH, _TRUNCATE, "%sRonin_Cheats.log", exe);
        g_verbose = CfgInt("log_verbose", 0);      /* release 默认 0：不写日志 */
        Log("DLL_PROCESS_ATTACH pid=%lu", GetCurrentProcessId());
        LoadRealDinput8();
        CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
    }
    return TRUE;
}
