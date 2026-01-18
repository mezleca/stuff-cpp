#!/usr/bin/env python3

import os
import sys
import shutil
import subprocess
import platform
import argparse
import time
from pathlib import Path

# paths
CWD = Path.cwd()
BUILD_DIR = CWD / "build"
EXTERNAL_DIR = CWD / "external"
SOURCE_DIR = CWD
BINARY_NAME = "shitty_ass_wm"

SYSTEM = platform.system()

def run(
    cmd: str,
    check: bool = True,
    env: dict[str, str] | None = None,
    capture: bool = False,
    wait: bool = True,
) -> tuple[int, str]:
    print(f"exec: {cmd}")

    if wait:
        result = subprocess.run(
            cmd,
            shell=True,
            env=env,
            capture_output=capture,
            text=capture,
        )

        if check and result.returncode != 0:
            print(f"command failed with exit code {result.returncode}")
            sys.exit(result.returncode)

        return result.returncode, result.stdout

    _ = subprocess.Popen(
        cmd,
        shell=True,
        env=env,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
        text=capture,
    )

    return 0, ""

def is_process_active(name: str, check: bool = False) -> bool:
    print(f"check process: {name}")

    result = subprocess.run(
        ["pgrep", "-f", name],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    active = result.returncode == 0

    if check and not active:
        print(f"process not running: {name}")
        sys.exit(1)

    return active

def kill_process(name: str, signal: str = "TERM", check: bool = True) -> bool:
    print(f"kill process: {name} ({signal})")

    result = subprocess.run(
        ["pkill", f"-{signal}", "-f", name],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    killed = result.returncode == 0

    if check and not killed:
        print(f"failed to kill process: {name}")
        sys.exit(1)

    return killed

def tool_exists(tool: str):
    if shutil.which(tool) is None:
        raise Exception(f"{tool} not found in PATH")

def init() -> int:
    print("initializing submodules...")

    if Path("./.gitmodules").exists() and run("git submodule update --init --recursive", check=False)[0] != 0:
        print("failed to initialize submodules")
        return 1

    tool_exists("cmake")
    tool_exists("ninja")
    tool_exists("clang")

    return 0

def clean() -> int:
    if not BUILD_DIR.exists():
        print("build directory doesnt exist")
        return 0

    print("removing build directory...")
    try:
        shutil.rmtree(BUILD_DIR)
        print("build directory removed")
        return 0
    except PermissionError:
        print("permission denied - try running as admin/sudo")
        return 1

def get_cpu_count() -> int:
    if SYSTEM == "Linux":
        result = subprocess.run("nproc", shell=True, capture_output=True, text=True)
        cores = result.stdout.strip()
        return int(cores) if cores.isdigit() else os.cpu_count() or 4

    return os.cpu_count() or 4

def configure_linux(debug: bool) -> int:
    cache = BUILD_DIR / "CMakeCache.txt"

    if cache.exists():
        cache.unlink()

    BUILD_DIR.mkdir(exist_ok=True)

    cmd = (
        f"cmake -G Ninja "
        f"-B {BUILD_DIR} "
        f"-S {SOURCE_DIR} "
        f"-DCMAKE_BUILD_TYPE={"Debug" if debug else "Release"} "
        f"-DOUTPUT_NAME={BINARY_NAME}"
    )

    return run(cmd)[0]

def configure(debug: bool) -> int:
    return configure_linux(debug)

def build(debug: bool) -> int:
    if not BUILD_DIR.exists():
        print("build directory doesnt exist, configuring...")
        if configure(debug) != 0:
            return 1

    cores = get_cpu_count()
    return run(f"ninja -C {BUILD_DIR} -j{cores}")[0]

def run_x11_test_apps():
    _ = run("DISPLAY=:1 kitty")[0]

def run_binary(test: bool) -> int:
    binary = BUILD_DIR / BINARY_NAME

    if not binary.exists():
        print(f"binary not found at {binary}")
        print("build the project first")
        return 1

    if not test:
        return run(str(binary))[0]
    else:
        if not is_process_active("Xephyr"):
            print("Xephyr is not running...")
            sys.exit(1)

        # awalys attempt to kill the old wm process
        _ = kill_process(BINARY_NAME, check=False)

        # wait a bit then run the wm once again
        time.sleep(0.5)
        _ = run(f"DISPLAY=:1 {str(binary)}", wait=False)
        time.sleep(1.5)

        if not is_process_active(BINARY_NAME):
            print(f"failed to run {BINARY_NAME}...")
            sys.exit(1)

        _ = run_x11_test_apps()
        return 0

def main():
    parser = argparse.ArgumentParser(
        description="build script for c++ projects",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    _ = parser.add_argument(
        "command",
        choices=["init", "configure", "build", "clean", "run"],
        help="command to execute"
    )

    _ = parser.add_argument(
        "--debug",
        action="store_true",
        help="use debug build type"
    )

    _ = parser.add_argument(
        "--test",
        action="store_true",
        help="run test apps on wm"
    )

    _ = parser.add_argument(
        "--run",
        action="store_true",
        help="run binary after building"
    )

    args = parser.parse_args()

    commands = {
        "init": init,
        "configure": lambda: configure(args.debug),
        "build": lambda: build(args.debug),
        "clean": clean,
        "run": lambda: run_binary(args.test)
    }

    exit_code = commands[args.command]()

    if args.command == "build" and exit_code == 0 and args.run:
        exit_code = run_binary(args.test)

    sys.exit(exit_code)

if __name__ == "__main__":
    main()
