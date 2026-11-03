# 浪人崛起 内置 DLL 修改器

**中文** | [English](README_EN.md)

## 做什么

浪人崛起 内置 dll 修改器，开游戏即激活。

## 功能

1. 无限物品使用
2. 自动石火
3. BUFF不减
4. 精力无消耗
5. 无视套装件数需求

## 配置

`Ronin_Cheats.ini` 共 3 项：

| 配置项 | 取值 | 说明 |
|---|---|---|
| `enable_inf_item` | `1` / `0` | 无限物品使用 |
| `enable_hooks` | `1` / `0` | 其余 4 项的总开关，只能全开或全关 |
| `max_hooks` | `0` / `N` | `0` = 全部安装；`N` = 只装前 N 条（排查崩溃用） |

注意：

- `enable_inf_item`、`enable_hooks` 删掉整行 = 关闭。
- `max_hooks` 不要删，删掉后只会装 1 条 hook，等于全部功能失效。

## 使用方法

1. 把 `dinput8.dll` 和 `Ronin_Cheats.ini` 放进游戏根目录
2. 启动游戏，自动生效
3. 改完配置保存，重启游戏生效

卸载：删掉游戏根目录的 `dinput8.dll`。
