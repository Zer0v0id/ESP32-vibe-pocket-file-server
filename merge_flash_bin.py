#!/usr/bin/env python3
"""
Merge ESP-IDF build outputs into a single flashable .bin.
Run from project root after idf.py build.

  python merge_flash_bin.py           # default build/
  python merge_flash_bin.py build     # same
"""
import os
import subprocess
import sys

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    build_name = sys.argv[1] if len(sys.argv) > 1 else "build"
    build_dir = os.path.join(script_dir, build_name)
    flash_args_path = os.path.join(build_dir, "flash_args")
    out_name = "vibe_pocket_file_server_flash.bin"

    if not os.path.isfile(flash_args_path):
        print(f"{build_name}/flash_args not found. Run idf.py build first.", file=sys.stderr)
        sys.exit(1)

    with open(flash_args_path, "r", encoding="utf-8") as f:
        lines = [line.strip() for line in f if line.strip()]
    args = []
    for line in lines:
        args.extend(line.split())

    os.chdir(build_dir)
    cmd = [
        sys.executable, "-m", "esptool",
        "--chip", "esp32s3",
        "merge_bin", "-o", out_name,
        *args,
    ]
    print("Merging binaries ->", out_name)
    subprocess.run(cmd, check=True)
    print("Single flash image: " + build_name + "\\" + out_name)
    print("Flash with: esptool.py -p COMx write_flash 0x0 " + build_name + "\\" + out_name)

if __name__ == "__main__":
    main()
