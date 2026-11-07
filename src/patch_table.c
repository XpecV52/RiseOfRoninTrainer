/* patch_table.c — 字节补丁表（minimal 分支）
 *
 * 只有一项：物品不减。
 *   原指令 sub r8,rbx  (44 2B C3)
 *   改成 3 字节 nop    (66 66 90)   等长替换，不需要跳转。
 *
 * 其余功能都是 hook 型，见 hook_data.h。
 */
#include "patch_table.h"

const CtPatch g_patches[] = {
    { "enable_inf_item", "物品不减", "44 2B C3 47 89 84 13 E4 C3 10 00 89 54 24 24",
      0, "44 2B C3", "66 66 90" },
};

const int g_patches_n = 1;
