/* engine.c — Ronin DLL 精简运行时实现 (x64, 无外部依赖) */
#include "engine.h"
#include <windows.h>
#include <string.h>
#include <stdio.h>

int eng_parse_module(uintptr_t base, EngSection* secs, int max)
{
    if (!base || !secs) return 0;
    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    const IMAGE_NT_HEADERS64* nt = (const IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

    int n = 0;
    const IMAGE_SECTION_HEADER* s = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections && n < max; i++, s++) {
        char* nm = (char*)LocalAlloc(LPTR, 9);
        if (nm) { memcpy(nm, s->Name, 8); nm[8] = 0; }
        secs[n].base = base + s->VirtualAddress;
        secs[n].size = s->Misc.VirtualSize ? s->Misc.VirtualSize : s->SizeOfRawData;
        secs[n].exec = (s->Characteristics & IMAGE_SCN_MEM_EXECUTE) ? 1 : 0;
        secs[n].name = nm;
        n++;
    }
    return n;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int eng_parse_aob(const char* pattern, unsigned char* out, unsigned char* mask, int maxlen)
{
    int n = 0;
    const char* p = pattern;
    while (*p && n < maxlen) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (p[0] == '?' && p[1] == '?') {
            out[n] = 0;
            mask[n] = 0;
            p += 2;
            n++;
            continue;
        }
        int hi = hexval(p[0]);
        int lo = (p[1] ? hexval(p[1]) : -1);
        if (hi < 0 || lo < 0) break;
        out[n] = (unsigned char)((hi << 4) | lo);
        mask[n] = 0xFF;
        p += 2;
        n++;
    }
    return n;
}

int eng_pattern_scan(const unsigned char* data, size_t size, const char* pattern, size_t* out_off)
{
    unsigned char pat[256], msk[256];
    int len = eng_parse_aob(pattern, pat, msk, 256);
    if (len <= 0 || (size_t)len > size) return 0;
    for (size_t i = 0; i + len <= size; i++) {
        int ok = 1;
        for (int k = 0; k < len; k++) {
            if (msk[k] && data[i + k] != pat[k]) { ok = 0; break; }
        }
        if (ok) {
            if (out_off) *out_off = i;
            return 1;
        }
    }
    return 0;
}

uintptr_t eng_scan_module(const EngSection* secs, int nsec, const char* pattern, int* hits)
{
    uintptr_t first = 0;
    int cnt = 0;
    for (int i = 0; i < nsec; i++) {
        size_t off = 0;
        if (eng_pattern_scan((const unsigned char*)secs[i].base, secs[i].size, pattern, &off)) {
            if (!first) first = secs[i].base + off;
            cnt++;
        }
    }
    if (hits) *hits = cnt;
    return first;
}

int eng_write(uintptr_t addr, const void* bytes, size_t n)
{
    if (!addr || !bytes || !n) return 0;
    DWORD old = 0;
    if (!VirtualProtect((LPVOID)addr, n, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy((void*)addr, bytes, n);
    VirtualProtect((LPVOID)addr, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)addr, n);
    return 1;
}

int eng_check(uintptr_t addr, const char* orig_hex)
{
    if (!orig_hex || !*orig_hex) return 1;
    unsigned char pat[64], msk[64];
    int len = eng_parse_aob(orig_hex, pat, msk, 64);
    if (len <= 0) return 1;
    const unsigned char* p = (const unsigned char*)addr;
    for (int i = 0; i < len; i++) {
        if (msk[i] && p[i] != pat[i]) return 0;
    }
    return 1;
}

/* ---------------- hook 支持 ---------------- */

int eng_rel32_ok(uintptr_t from, uintptr_t to)
{
    int64_t d = (int64_t)to - (int64_t)(from + 5);
    return d >= -2147483647LL && d <= 2147483647LL;
}

void* eng_alloc_near(uintptr_t target, size_t size)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    uintptr_t gran = si.dwAllocationGranularity ? si.dwAllocationGranularity : 0x10000;
    uintptr_t lo = (target > 0x7FFF0000ULL) ? target - 0x7FF00000ULL : gran;
    uintptr_t hi = target + 0x7FF00000ULL;

    /* 从目标附近向外扩，先试更近的步长 */
    for (uintptr_t step = 0; step < 0x7FF00000ULL; step += gran) {
        uintptr_t cands[2];
        cands[0] = (target > step) ? (target - step) : 0;
        cands[1] = target + step;
        for (int i = 0; i < 2; i++) {
            uintptr_t c = cands[i];
            if (c < lo || c > hi) continue;
            c &= ~(gran - 1);
            if (!c) continue;
            void* p = VirtualAlloc((LPVOID)c, size, MEM_COMMIT | MEM_RESERVE,
                                   PAGE_EXECUTE_READWRITE);
            if (p) {
                if (eng_rel32_ok(target, (uintptr_t)p)) return p;
                VirtualFree(p, 0, MEM_RELEASE);
            }
        }
    }
    return NULL;
}

int eng_write_jmp(uintptr_t addr, uintptr_t dst)
{
    if (!eng_rel32_ok(addr, dst)) return 0;
    unsigned char code[5];
    code[0] = 0xE9;
    int32_t rel = (int32_t)((int64_t)dst - (int64_t)(addr + 5));
    memcpy(code + 1, &rel, 4);
    return eng_write(addr, code, 5);
}
