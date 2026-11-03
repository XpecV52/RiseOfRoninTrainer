# Rise of the Ronin — Built-in DLL Trainer

[中文](README.md) | **English**

## What it does

Built-in DLL trainer for Rise of the Ronin. Activates automatically when the game starts.

## Features

1. Infinite items
2. Auto parry
3. Buffs never expire
4. No Ki (stamina) consumption
5. Ignore set piece requirement

## Configuration

`Ronin_Cheats.ini` has 3 keys:

| Key | Value | Description |
|---|---|---|
| `enable_inf_item` | `1` / `0` | Infinite items |
| `enable_hooks` | `1` / `0` | Master switch for the other 4 features (all or nothing) |
| `max_hooks` | `0` / `N` | `0` = install all; `N` = install only the first N hooks (crash debugging) |

Notes:

- Deleting the `enable_inf_item` or `enable_hooks` line turns that feature off.
- Do not delete `max_hooks` — without it only one hook is installed and every feature stops working.

## Usage

1. Copy `dinput8.dll` and `Ronin_Cheats.ini` into the game folder
2. Launch the game — it activates automatically
3. Restart the game after editing the config

Uninstall: delete `dinput8.dll` from the game folder.
