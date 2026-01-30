#!/usr/bin/env python3
"""Bootstrap a Zephyr workspace from requirements.

Creates a `.venv`, installs `west` and Zephyr Python requirements, runs
`west init`/`west update` to fetch Zephyr sources, and optionally downloads
and installs the Zephyr SDK into `.zephyr-sdk`.

This script is intended to let users who only have Python installed run a
single command to prepare the project for `west build`.

Usage:
  python3 bootstrap_zephyr_project.py [--install-sdk] [--sdk-version 0.16.8]

Notes:
 - The script attempts a non-interactive SDK install. If the SDK installer
   does not support unattended install on the platform, instructions are
   printed for manual installation.
 - The script must be run from the project root (it will detect its own
   location automatically when invoked from the scripts directory).
"""

import argparse
import os
import shutil
import stat
import subprocess
import sys
import tarfile
from pathlib import Path
from urllib.request import urlopen, Request


ROOT = Path(__file__).resolve().parents[3]
VENV_DIR = ROOT / '.venv'
SDK_DIR = ROOT / '.zephyr-sdk'
SCRIPTS_DIR = ROOT / 'modules' / 'layered-queue-driver' / 'scripts'
BUILD_DIR = ROOT / 'build'
# Default: write generated project files into build/ for predictable builds
WRITE_TO_BUILD = True


def run(cmd, **kwargs):
    print(f">>> {' '.join(cmd)}")
    subprocess.check_call(cmd, **kwargs)


def ensure_venv(python_exe=None):
    if VENV_DIR.exists():
        print(f"Using existing venv: {VENV_DIR}")
        return

    py = python_exe or shutil.which('python3') or shutil.which('python')
    if not py:
        raise SystemExit('No Python interpreter found (python3 or python).')

    print(f"Creating virtualenv at {VENV_DIR} using {py}")
    run([py, '-m', 'venv', str(VENV_DIR)])


def pip_install(packages):
    pip = VENV_DIR / 'bin' / 'pip'
    run([str(pip), 'install', '--upgrade', 'pip', 'setuptools', 'wheel'])
    run([str(pip), 'install'] + packages)


def ensure_west_and_deps():
    pip_install(['west'])


def west_init_update():
    west = VENV_DIR / 'bin' / 'west'
    # If ZEPHYR_BASE is set, assume user has a Zephyr checkout and skip init/update
    if os.environ.get('ZEPHYR_BASE'):
        print(f"ZEPHYR_BASE is set to {os.environ.get('ZEPHYR_BASE')}; skipping west init/update")
        return

    if not (ROOT / '.west').exists():
        print('Initializing west workspace (manifest will be fetched from upstream)')
        try:
            run([str(west), 'init', '-m', 'https://github.com/zephyrproject-rtos/manifest.git', str(ROOT)])
        except subprocess.CalledProcessError as e:
            print(f'Warning: west init failed: {e}; continuing (workspace may be managed elsewhere)')
    else:
        print('west workspace already initialized')
    print('Updating west workspace (this may download Zephyr and modules)')
    try:
        run([str(west), 'update'], cwd=str(ROOT))
    except subprocess.CalledProcessError as e:
        print(f'Warning: west update failed: {e}; continuing')


def install_zephyr_python_reqs():
    # Install zephyr's python requirements into the venv
    req_file = ROOT / 'zephyr' / 'scripts' / 'requirements.txt'
    if not req_file.exists():
        print('Zephyr sources not found; skipping Zephyr Python requirements install')
        return
    pip = VENV_DIR / 'bin' / 'pip'
    print(f'Installing Zephyr Python requirements from {req_file}')
    run([str(pip), 'install', '-r', str(req_file)])


def download_file(url: str, dest: Path):
    dest.parent.mkdir(parents=True, exist_ok=True)
    print(f'Downloading {url} → {dest}')
    req = Request(url, headers={'User-Agent': 'bootstrap-script/1.0'})
    with urlopen(req) as resp, open(dest, 'wb') as out:
        shutil.copyfileobj(resp, out)


def download_and_install_sdk(version: str = '0.16.8'):
    # Attempt to download SDK installer for linux x86_64
    # URL pattern (GitHub releases): sdk-ng/releases/download/v{version}/zephyr-sdk-{version}-setup.run
    runfile = ROOT / 'build' / f'zephyr-sdk-{version}-setup.run'
    url = f'https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v{version}/zephyr-sdk-{version}-setup.run'
    try:
        download_file(url, runfile)
    except Exception as e:
        print(f'Failed to download SDK installer: {e}')
        return False

    # Make executable
    runfile.chmod(runfile.stat().st_mode | stat.S_IXUSR)

    # Attempt non-interactive install into SDK_DIR
    SDK_DIR.mkdir(parents=True, exist_ok=True)
    installer = str(runfile)
    print(f'Attempting to run SDK installer into {SDK_DIR} (may require sudo)')
    try:
        # Many Zephyr SDK installers accept a "-- -d <dir> -y" style to pass
        # options to the inner installer. Try a couple of common forms.
        cmd = [installer, '--', '-d', str(SDK_DIR), '-y']
        run(cmd)
    except subprocess.CalledProcessError:
        try:
            cmd = [installer, '-d', str(SDK_DIR), '-y']
            run(cmd)
        except subprocess.CalledProcessError as e:
            print('Automatic SDK installation failed; please run the installer manually:')
            print(f'  {installer} -d {SDK_DIR} -y')
            return False

    print(f'Zephyr SDK installed into {SDK_DIR}')
    return True


def write_env_file():
    
    if WRITE_TO_BUILD:
        BUILD_ENV = BUILD_DIR / '.zephyr_env'
        BUILD_DIR.mkdir(parents=True, exist_ok=True)
        with open(BUILD_ENV, 'w') as f:
            f.write(f'export ZEPHYR_SDK_INSTALL_DIR={SDK_DIR}\n')
            f.write(f'export ZEPHYR_BASE={ROOT}/zephyr\n')
        print(f'Also wrote environment snippet to {BUILD_ENV}')


def generate_cmakelists():
    content = f'''# Auto-generated CMakeLists.txt by bootstrap_zephyr_project.py
cmake_minimum_required(VERSION 3.20.0)
project(zephy_app)

option(ENABLE_HIL_TESTS "Build native HIL tests with Google Test" ON)

# Register layered-queue-driver as a Zephyr module BEFORE find_package(Zephyr)
list(APPEND ZEPHYR_EXTRA_MODULES ${{CMAKE_CURRENT_SOURCE_DIR}}/modules/layered-queue-driver)

# Use generated prj.conf from prereq directory when present
set(CONF_FILE ${{CMAKE_CURRENT_BINARY_DIR}}/prereq/prj.conf)

find_package(Zephyr REQUIRED HINTS $ENV{{ZEPHYR_BASE}})
project(zephy_app)

include(modules/layered-queue-driver/cmake/LayeredQueueApp.cmake)
include(modules/layered-queue-driver/cmake/RequirementsDriven.cmake)

add_lq_application_from_requirements(app
    REQUIREMENTS requirements/
    PLATFORM zephyr
    RTOS zephyr
)
'''
    # Optionally write to build directory
    if WRITE_TO_BUILD:
        BUILD_DIR.mkdir(parents=True, exist_ok=True)
        build_cmake = BUILD_DIR / 'CMakeLists.txt'
        build_cmake.write_text(content)
        print(f'Also wrote CMakeLists.txt to {build_cmake}')


def generate_west_yml():
    content = '''manifest:
  remotes:
    - name: zephyr
      url-base: https://github.com/zephyrproject-rtos
  projects:
    - name: zephyr
      remote: zephyr
      revision: main
'''
    # Optionally write to build directory
    if WRITE_TO_BUILD:
        BUILD_DIR.mkdir(parents=True, exist_ok=True)
        build_west = BUILD_DIR / 'west.yml'
        build_west.write_text(content)
        print(f'Also wrote west.yml to {build_west}')


def generate_project_files():
    # Generate minimal project scaffolding if missing
    generate_cmakelists()
    generate_west_yml()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--install-sdk', action='store_true', help='Download and attempt to install Zephyr SDK')
    parser.add_argument('--sdk-version', default='0.16.8', help='Zephyr SDK version to download')
    parser.add_argument('--no-write-to-build', action='store_true', help="Don't write generated project files into build/ directory")
    args = parser.parse_args()

    ensure_venv()
    ensure_west_and_deps()
    west_init_update()
    install_zephyr_python_reqs()

    # Generate minimal top-level project files if they don't exist
    global WRITE_TO_BUILD
    if getattr(args, 'no_write_to_build', False):
        WRITE_TO_BUILD = False
    generate_project_files()

    if args.install_sdk:
        ok = download_and_install_sdk(args.sdk_version)
        if not ok:
            print('SDK automatic install failed or incomplete. Please follow installer output.')

    write_env_file()
if __name__ == '__main__':
    main()
