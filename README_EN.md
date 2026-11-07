# Rise of the Ronin — Built-in DLL Trainer (Lite)

[中文](README.md) | **English**

## What it does

Built-in DLL trainer for Rise of the Ronin. Activates automatically when the game starts. This branch keeps only 3 features.

## Features

1. Auto parry
2. Ignore set piece requirement
3. Infinite items

## Configuration

`Ronin_Cheats.ini` has 3 keys, each switchable on its own:

| Key | Value | Description |
|---|---|---|
| `enable_auto_parry` | `1` / `0` | Auto parry |
| `enable_no_set_req` | `1` / `0` | Ignore set piece requirement |
| `enable_inf_item` | `1` / `0` | Infinite items |

## Usage

1. Copy `dinput8.dll` and `Ronin_Cheats.ini` into the game folder
2. Launch the game — it activates automatically
3. Restart the game after editing the config

Uninstall: delete `dinput8.dll` from the game folder.
