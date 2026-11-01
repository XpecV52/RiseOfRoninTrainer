/* patch_table.h — Ronin 补丁表
 *
 * 每一条 = 一个功能点：
 *   feature : ini 里的开关名 (enable_xxx)，为空则总是应用
 *   name    : 日志里显示的名字
 *   aob     : 特征码，空格分隔，?? 通配（运行时在 .text 明文里搜）
 *   offset  : 相对命中点的字节偏移
 *   orig    : 原字节校验（防止游戏更新后打错位置），可留空
 *   patch   : 要写入的新字节
 *
 * 表为空时 DLL 只写日志，不动游戏。
 * 填表数据来自 FLiNG 修改器逆向（见 ../out/REPORT.md）。
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* feature;
    const char* name;
    const char* aob;
    int         offset;
    const char* orig;
    const char* patch;
} CtPatch;

extern const CtPatch g_patches[];
extern const int g_patches_n;

#ifdef __cplusplus
}
#endif
