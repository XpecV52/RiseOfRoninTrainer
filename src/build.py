# -*- coding: utf-8 -*-
"""build.py — 编译 dinput8.dll，产物输出到 ../dist/

用法: python build.py [输出dll名]    默认 dinput8.dll

目录约定:
  src/     开发目录（源码 + 本脚本）
  src/obj/ 编译中间产物
  dist/    产物目录（dll + ini + 发行包）
"""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DIST = os.path.join(ROOT, 'dist')
OBJ = os.path.join(HERE, 'obj')


def find_msvc():
    roots = ['E:/APP/Microsoft Visual Studio', 'C:/Program Files/Microsoft Visual Studio',
             'C:/Program Files (x86)/Microsoft Visual Studio']
    for root in roots:
        if not os.path.isdir(root):
            continue
        for dirpath, _, files in os.walk(root):
            d = dirpath.replace('\\', '/')
            if d.endswith('Hostx64/x64') and 'cl.exe' in files:
                return d
    return None


def find_sdk():
    for root in ('E:/Windows Kits/10', 'C:/Program Files (x86)/Windows Kits/10'):
        inc = os.path.join(root, 'Include')
        lib = os.path.join(root, 'Lib')
        if os.path.isdir(inc) and os.path.isdir(lib):
            vers = sorted(os.listdir(inc))
            if vers:
                v = vers[-1]
                return os.path.join(inc, v), os.path.join(lib, v)
    return None, None


def main():
    dll = sys.argv[1] if len(sys.argv) > 1 else 'dinput8.dll'
    bindir = find_msvc()
    if not bindir:
        print('找不到 MSVC (cl.exe)')
        return 1
    sdk_inc, sdk_lib = find_sdk()
    if not sdk_inc:
        print('找不到 Windows SDK')
        return 1
    msvc_root = bindir.split('/bin/')[0]
    print(f'MSVC: {msvc_root}')
    print(f'SDK : {sdk_inc}')

    env = dict(os.environ)
    env['INCLUDE'] = ';'.join([
        os.path.join(msvc_root, 'include'),
        os.path.join(sdk_inc, 'ucrt'),
        os.path.join(sdk_inc, 'um'),
        os.path.join(sdk_inc, 'shared'),
    ])
    env['LIB'] = ';'.join([
        os.path.join(msvc_root, 'lib', 'x64'),
        os.path.join(sdk_lib, 'ucrt', 'x64'),
        os.path.join(sdk_lib, 'um', 'x64'),
    ])
    cl = os.path.join(bindir, 'cl.exe')

    os.makedirs(DIST, exist_ok=True)
    os.makedirs(OBJ, exist_ok=True)

    out_dll = os.path.join(DIST, dll)

    def run(args):
        print('+', os.path.basename(args[0]), ' '.join(os.path.basename(a) for a in args[1:]))
        r = subprocess.run(args, cwd=HERE, env=env, capture_output=True, text=True, errors='replace')
        if r.stdout.strip():
            print(r.stdout.strip()[:4000])
        if r.returncode != 0:
            print(r.stderr[:4000])
            raise SystemExit(f'失败: {args[0]}')
        return r

    srcs = ['dllmain.cpp', 'engine.c', 'patch_table.c']
    run([cl, '/nologo', '/utf-8', '/D_CRT_SECURE_NO_WARNINGS', '/LD', '/O2', '/EHsc', '/W3'] + srcs +
        ['/Fo' + OBJ + '\\',
         '/Fd' + os.path.join(OBJ, dll.replace('.dll', '.pdb')),
         '/Fe:' + out_dll,
         '/link', '/DEF:' + dll.replace('.dll', '.def'),
         '/IMPLIB:' + os.path.join(OBJ, dll.replace('.dll', '.lib'))])

    # 配置文件一起放进产物目录
    ini_src = os.path.join(HERE, 'Ronin_Cheats.ini')
    ini_dst = os.path.join(DIST, 'Ronin_Cheats.ini')
    if os.path.isfile(ini_src):
        shutil.copy2(ini_src, ini_dst)
        print(f'  配置: {ini_dst}')

    print(f'\n完成: {out_dll}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
