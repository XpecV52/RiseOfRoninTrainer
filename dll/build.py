# -*- coding: utf-8 -*-
"""build.py — Ronin DLL 编译脚本（不依赖 vcvars，自动探测 VS / Windows SDK）
用法: python build.py [输出dll名]   默认 dinput8.dll
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


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
        ['/Fe:' + dll, '/link', '/DEF:' + dll.replace('.dll', '.def')])
    print(f'\n完成: {os.path.join(HERE, dll)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
