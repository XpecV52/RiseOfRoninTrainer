# -*- coding: utf-8 -*-
"""package.py — 打发行版 zip
用法: python package.py [版本号]
"""
import os
import sys
import zipfile

VERSION = "1.0.0"
HERE = os.path.dirname(os.path.abspath(__file__))

# (源文件, zip 内文件名)
FILES = [
    ("release/dinput8.dll", "dinput8.dll"),
    ("release/Ronin_Cheats.ini", "Ronin_Cheats.ini"),
    ("README.md", "README.md"),
    ("README_EN.md", "README_EN.md"),
]


def main():
    ver = sys.argv[1] if len(sys.argv) > 1 else VERSION
    outdir = os.path.join(HERE, "dist")
    os.makedirs(outdir, exist_ok=True)
    name = "RiseOfRoninTrainer_v%s.zip" % ver
    path = os.path.join(outdir, name)

    print("打包 %s" % name)
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        for src, arc in FILES:
            full = os.path.join(HERE, src)
            if not os.path.isfile(full):
                print("  缺少: %s" % src)
                return 1
            z.write(full, arc)
            print("  + %-20s %8d B" % (arc, os.path.getsize(full)))

    print("\n-> %s  (%d B)" % (path, os.path.getsize(path)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
