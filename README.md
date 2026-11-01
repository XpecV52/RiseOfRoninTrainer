# 浪人崛起 内置修改 DLL

游戏启动自动加载，不需要额外开修改器。

## 使用

1. 把 `release/dinput8.dll` 和 `release/Ronin_Cheats.ini` 复制到游戏根目录
2. 启动游戏

## 配置

`Ronin_Cheats.ini`：

```ini
[Ronin]
enable_inf_item=1     ; 无限物品
enable_hooks=1        ; 全部 hook 功能
max_hooks=0           ; 0=全开
log_verbose=0         ; 0=不写日志
```

## 卸载

删掉游戏目录的 `dinput8.dll` 即可，存档不受影响。

## 编译

```bash
cd dll
python build.py
```

需要 Visual Studio + Windows SDK。
