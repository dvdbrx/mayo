#!/usr/bin/env python3
"""Bootstrap official Qt6 and OpenCASCADE dependencies locally for Ubuntu without sudo.

Downloads deb packages directly from official Ubuntu package mirrors and extracts
them into .deps/sysroot/ inside the repository.
"""
import concurrent.futures
import hashlib
import json
import os
import pathlib
import shlex
import subprocess
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEPS = ROOT / '.deps'
ARCHIVES = DEPS / 'archives'
SYSROOT = DEPS / 'sysroot'

PACKAGES = [
    'qt6-base-dev',
    'qt6-svg-dev',
    'libqt6svg6',
    'libqt6svgwidgets6',
    'libocct-data-exchange-dev',
    'libocct-visualization-dev',
    'libocct-ocaf-dev',
    'libocct-draw-dev',
    'occt-misc',
    'libtbb-dev',
    'libxi-dev',
    'libfontconfig-dev',
    'libfreetype-dev',
    'libfreeimage-dev',
    'libgl1-mesa-dev',
    'xvfb',
    'xauth',
]


def download_spec(spec):
    url, name, size, checksum = spec
    url = url.replace('http://', 'https://', 1)
    archive = ARCHIVES / name
    if not archive.exists() or archive.stat().st_size != int(size):
        with urllib.request.urlopen(url, timeout=60) as response:
            archive.write_bytes(response.read())
    data = archive.read_bytes()
    if len(data) != int(size):
        raise RuntimeError(f'Incorrect size: {name}')
    if checksum.startswith('MD5Sum:'):
        if hashlib.md5(data).hexdigest() != checksum.split(':', 1)[1]:
            raise RuntimeError(f'APT checksum mismatch: {name}')
    return {'file': name, 'url': url}


def main():
    ARCHIVES.mkdir(parents=True, exist_ok=True)
    SYSROOT.mkdir(parents=True, exist_ok=True)

    print('Querying official apt repositories for package URLs...')
    result = subprocess.run([
        'apt-get', '--print-uris', '--yes', '--download-only',
        '--no-install-recommends', '-o', 'Debug::NoLocking=1', 'install',
        *PACKAGES,
    ], check=True, capture_output=True, text=True)

    specs = [shlex.split(line) for line in result.stdout.splitlines()
             if line.startswith("'http")]

    print(f'Downloading {len(specs)} packages in parallel...')
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        manifest = list(pool.map(download_spec, specs))

    print(f'Extracting {len(manifest)} packages into {SYSROOT}...')
    for item in manifest:
        deb_path = ARCHIVES / item['file']
        subprocess.run(['dpkg-deb', '-x', str(deb_path), str(SYSROOT)], check=True)

    # Ensure lib symlinks for x86_64-linux-gnu
    lib_dir = SYSROOT / 'usr' / 'lib' / 'x86_64-linux-gnu'
    if lib_dir.exists():
        fc_so = lib_dir / 'libfontconfig.so'
        if not fc_so.exists() or (fc_so.is_symlink() and not fc_so.resolve().exists()):
            fc_so.unlink(missing_ok=True)
            for target in ['/usr/lib/x86_64-linux-gnu/libfontconfig.so.1', str(lib_dir / 'libfontconfig.so.1')]:
                if os.path.exists(target):
                    fc_so.symlink_to(target)
                    break

        ft_so = lib_dir / 'libfreetype.so'
        if ft_so.is_symlink() and not ft_so.resolve().exists():
            system_ft = pathlib.Path('/usr/lib/x86_64-linux-gnu/libfreetype.so.6')
            if system_ft.exists():
                ft_so.unlink(missing_ok=True)
                ft_so.symlink_to(system_ft)

    # Patch OpenCASCADE cmake configs for relocatable installation
    occt_cmake_dir = lib_dir / 'cmake' / 'opencascade'
    if occt_cmake_dir.exists():
        for cmake_file in occt_cmake_dir.glob('*.cmake'):
            content = cmake_file.read_text()
            modified = False
            if '/usr/include/opencascade' in content:
                content = content.replace('/usr/include/opencascade', str(SYSROOT / 'usr' / 'include' / 'opencascade'))
                modified = True
            if '/usr/lib/x86_64-linux-gnu/libfreeimage.so' in content:
                content = content.replace('/usr/lib/x86_64-linux-gnu/libfreeimage.so', str(lib_dir / 'libfreeimage.so'))
                modified = True
            if '/usr/lib/x86_64-linux-gnu/libfreetype.so' in content:
                content = content.replace('/usr/lib/x86_64-linux-gnu/libfreetype.so', str(lib_dir / 'libfreetype.so'))
                modified = True
            if modified:
                cmake_file.write_text(content)

    print('Bootstrap completed successfully!')


if __name__ == '__main__':
    main()
