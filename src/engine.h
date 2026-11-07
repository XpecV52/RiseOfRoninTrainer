/* engine.h — Ronin DLL 精简运行时 (x64, 无外部依赖)
 *
 * 只做三件事：解析模块节表 / AOB 扫描 / 打字节补丁。
 * 比 ct2dll 那套少了 blob 洞穴和 import hook，够用且不易崩。
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { uintptr_t base; size_t size; int exec; const char* name; } EngSection;

/* 解析已加载模块的 PE 头，填充节表；返回节数 */
int eng_parse_module(uintptr_t base, EngSection* secs, int max);

/* 在数据里搜 AOB（空格分隔的十六进制，?? 通配）。成功返回 1 */
int eng_pattern_scan(const unsigned char* data, size_t size, const char* pattern, size_t* out_off);

/* 在整个模块的节里搜；返回命中的绝对地址，0 = 未命中；hits 返回命中数 */
uintptr_t eng_scan_module(const EngSection* secs, int nsec, const char* pattern, int* hits);

/* 临时去掉写保护并写字节；返回 1 成功 */
int eng_write(uintptr_t addr, const void* bytes, size_t n);

/* 校验地址处是否等于期望字节（orig 为空则跳过）；返回 1 匹配 */
int eng_check(uintptr_t addr, const char* orig_hex);

/* 把 "48 8B ?? C3" 解析成字节+掩码；返回长度 */
int eng_parse_aob(const char* pattern, unsigned char* out, unsigned char* mask, int maxlen);

/* 在 target 的 ±2GB 内分配可执行内存；失败返回 NULL */
void* eng_alloc_near(uintptr_t target, size_t size);

/* 在 addr 处写 5 字节 E9 相对跳转，指向 dst；返回 1 成功 */
int eng_write_jmp(uintptr_t addr, uintptr_t dst);

/* 校验 addr 与 dst 能否用 rel32 互达 */
int eng_rel32_ok(uintptr_t from, uintptr_t to);

typedef struct {
    int aobs_total, aobs_found;
    int patches_ok, patches_skip, patches_fail;
    int hooks_ok, hooks_fail, hooks_skip;
} EngReport;

#ifdef __cplusplus
}
#endif
