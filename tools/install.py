#!/usr/bin/env python3
"""Build and install XPWm on Linux, using explicit dependency installation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MODULES = ('wlroots-0.18', 'wayland-server', 'wayland-client', 'xkbcommon', 'libinput',
           'cairo', 'pangocairo', 'fontconfig', 'libdrm', 'gtk+-3.0', 'gio-unix-2.0', 'gtk-layer-shell-0', 'pam', 'wayland-cursor')
BINARIES = ('xpwm-lock', 'xpwm-displayctl', 'xpwm-taskbar', 'xpwm-desktop', 'xpwm-logon',
            'xpwm-logoff', 'xpwm-theme-preview', 'xpwm-luna-module.so', 'xpwm-sound', 'xpwm-run', 'xpwm-screensaver', 'xpwm-screenshot')
MANIFEST = Path('share/xpwm/install-manifest.json')


def manager_for(os_release):
    values = {}
    for line in os_release.splitlines():
        if '=' in line:
            key, value = line.split('=', 1)
            values[key] = shlex.split(value)[0] if value else ''
    names = (values.get('ID', '') + ' ' + values.get('ID_LIKE', '')).split()
    for name in names:
        if name in ('arch', 'manjaro', 'endeavouros', 'cachyos'): return 'pacman'
        if name in ('debian', 'ubuntu', 'linuxmint', 'pop'): return 'apt'
        if name in ('fedora', 'rhel', 'centos', 'rocky', 'almalinux'): return 'dnf'
        if name in ('opensuse', 'opensuse-tumbleweed', 'opensuse-leap', 'suse'): return 'zypper'
    return None


def dependency_command(manager):
    # Versioned wlroots is essential: newer releases have incompatible APIs.
    packages = {
        'pacman': ['gcc', 'make', 'pkgconf', 'wlroots0.18', 'wayland', 'wayland-protocols',
                   'libinput', 'libxkbcommon', 'cairo', 'pango', 'fontconfig', 'libdrm',
                   'gtk3', 'gtk-layer-shell', 'dbus', 'python', 'meson', 'ninja', 'pam'],
        'apt': ['build-essential', 'pkg-config', 'libwlroots-0.18-dev', 'libwayland-dev',
                'wayland-protocols', 'libinput-dev', 'libxkbcommon-dev', 'libcairo2-dev',
                'libpango1.0-dev', 'libfontconfig1-dev', 'libdrm-dev', 'libgtk-3-dev',
                'libgtk-layer-shell-dev', 'dbus', 'dbus-daemon', 'python3', 'meson', 'ninja-build', 'libpam0g-dev'],
        'dnf': ['gcc', 'make', 'pkgconf-pkg-config', 'wlroots0.18-devel', 'wayland-devel',
                'wayland-protocols-devel', 'libinput-devel', 'libxkbcommon-devel',
                'cairo-devel', 'pango-devel', 'fontconfig-devel', 'libdrm-devel',
                'gtk3-devel', 'gtk-layer-shell-devel', 'dbus-daemon', 'python3', 'meson', 'ninja-build', 'pam-devel'],
        'zypper': ['gcc', 'make', 'pkg-config', 'pkgconfig(wlroots-0.18)',
                   'wayland-devel', 'wayland-protocols-devel', 'libinput-devel',
                   'libxkbcommon-devel', 'cairo-devel', 'pango-devel', 'fontconfig-devel',
                   'libdrm-devel', 'gtk3-devel', 'gtk-layer-shell-devel', 'dbus-1', 'python3', 'meson', 'ninja', 'pam-devel'],
    }
    commands = {'pacman': ['pacman', '-S', '--needed'], 'apt': ['apt-get', 'install'],
                'dnf': ['dnf', 'install'], 'zypper': ['zypper', 'install']}
    if manager not in commands:
        raise ValueError('No dependency recipe for this distribution; install the README build requirements manually.')
    return commands[manager] + packages[manager]


def run(command, dry=False, privileged=False):
    if privileged and os.geteuid() != 0:
        if not dry and not shutil.which('sudo'):
            raise ValueError('This step needs root access; install sudo or run the shown command as root.')
        command = ['sudo', *command]
    print('+ ' + shlex.join([str(arg) for arg in command]), flush=True)
    if not dry:
        subprocess.run(command, check=True)


def check_dependencies():
    missing = [tool for tool in ('make', 'cc', 'pkg-config', 'wayland-scanner', 'dbus-run-session', 'meson', 'ninja')
               if not shutil.which(tool)]
    if missing:
        raise ValueError('Missing tools: ' + ', '.join(missing) + '. Try --deps or install the README requirements.')
    missing = [name for name in MODULES if subprocess.run(
        ['pkg-config', '--exists', name], check=False).returncode]
    if missing:
        raise ValueError('Missing development packages: ' + ', '.join(missing)
                         + '. XPWm requires wlroots 0.18.x; do not substitute another wlroots version.')
    version = subprocess.check_output(['pkg-config', '--modversion', 'wlroots-0.18'], text=True).strip()
    if not version.startswith('0.18.'):
        raise ValueError(f'Expected wlroots 0.18.x, found {version}.')


def installed_files(prefix):
    """An explicit runtime allowlist: never copy private assets or the checkout."""
    files = {Path('compositor/dwl'): ROOT / 'compositor/dwl'}
    files.update({Path('shell') / name: ROOT / 'shell' / name for name in BINARIES})
    files[Path('shell/session.sh')] = ROOT / 'shell/session.sh'
    for directory, pattern in (('themes', '*'), ('tools', '*.py'), ('tools', '*.sh'),
                               ('platforms', 'session.sh')):
        for source in (ROOT / directory).rglob(pattern):
            if source.is_file() and '__pycache__' not in source.parts:
                if directory == 'tools' and source.name == 'install.py': continue
                files[source.relative_to(ROOT)] = source
    files[Path('licenses/swaylock-MIT.txt')] = ROOT / 'third_party/swaylock/LICENSE'
    result = {Path('lib/xpwm') / name: source.read_bytes() for name, source in files.items()}
    bundle = prefix / 'lib/xpwm'
    bundle_quote = shlex.quote(str(bundle))
    asset_setup = '''asset_root=${XPWM_ASSET_ROOT:-${XDG_DATA_HOME:-$HOME/.local/share}/xpwm-private}
if [ -d "$asset_root/assets/local" ]; then export XPWM_ASSET_ROOT="$asset_root"; fi
'''
    result[Path('bin/xpwm')] = (f'#!/bin/sh\nset -eu\n{asset_setup}exec sh {bundle_quote}/tools/start-session.sh "$@"\n').encode()
    result[Path('bin/xpwm-import-assets')] = (
        f'#!/bin/sh\nexec python3 {bundle_quote}/tools/import-xp-iso.py "$@"\n').encode()
    result[Path('bin/xpwm-theme-preview')] = (
        f'#!/bin/sh\nset -eu\n{asset_setup}export XPWM_PROJECT_ROOT={bundle_quote}\n'
        f'. {bundle_quote}/tools/session-theme.sh\nexec {bundle_quote}/shell/xpwm-theme-preview "$@"\n').encode()
    result[Path('bin/xpwm-screenshot')] = (
        f'#!/bin/sh\nset -eu\n{asset_setup}export XPWM_PROJECT_ROOT={bundle_quote}\n'
        f'. {bundle_quote}/tools/session-theme.sh\nexec {bundle_quote}/shell/xpwm-screenshot "$@"\n').encode()
    def desktop_exec(path):
        return '"' + str(path).replace('\\', '\\\\').replace('"', '\\"').replace('`', '\\`').replace('$', '\\$').replace('%', '%%') + '"'
    result[Path('share/wayland-sessions/xpwm.desktop')] = (
        '[Desktop Entry]\nName=XPWm\nComment=Windows XP-inspired Wayland desktop built on dwl\n'
        f'Exec={desktop_exec(prefix / "bin/xpwm")} tty\nType=Application\n').encode()
    result[Path('share/applications/xpwm-theme-preview.desktop')] = (
        '[Desktop Entry]\nType=Application\nName=Luna Theme Preview\n'
        f'Exec={desktop_exec(prefix / "bin/xpwm-theme-preview")}\nCategories=Settings;\nTerminal=false\n').encode()
    result[Path('share/doc/xpwm/README.md')] = (ROOT / 'README.md').read_bytes()
    for name in ('LICENSE', 'compositor/LICENSE', 'compositor/LICENSE.dwm',
                 'compositor/LICENSE.sway', 'compositor/LICENSE.tinywl', 'compositor/UPSTREAM.md',
                 'compositor/protocols/wlr-screencopy-unstable-v1.xml',
                 'compositor/protocols/wlr-data-control-unstable-v1.xml'):
        result[Path('share/licenses/xpwm') / name] = (ROOT / name).read_bytes()
    return result


def destination(prefix, destdir):
    return (destdir / prefix.relative_to('/')) if destdir else prefix


def safe_target(root, relative):
    if relative.is_absolute() or '..' in relative.parts:
        raise ValueError(f'Unsafe install manifest path: {relative}')
    target = root / relative
    # Refuse symlink traversal, including stale links from unrelated software.
    for path in (target, *target.parents):
        if path == root.parent: break
        if path.is_symlink(): raise ValueError(f'Refusing symlink install path: {path}')
    return target


def deploy(prefix, destdir=None):
    root = destination(prefix, destdir)
    files = installed_files(prefix)
    manifest_path = safe_target(root, MANIFEST)
    old = json.loads(manifest_path.read_text()) if manifest_path.is_file() else None
    old_files = old.get('files', {}) if old else {}
    # Validate every destination before writing the first file.
    for name in files:
        target = safe_target(root, name)
        if target.exists() and str(name) not in old_files:
            raise ValueError(f'Unmanaged file already exists: {target}')
        if target.exists() and hashlib.sha256(target.read_bytes()).hexdigest() != old_files.get(str(name)):
            raise ValueError(f'Installed file was modified; preserve or remove it before updating: {target}')
    hashes = {}
    for name, data in files.items():
        target = root / name
        target.parent.mkdir(parents=True, exist_ok=True)
        # Write a new inode, avoiding accidental writes through old hard links.
        temporary = target.with_name(target.name + '.xpwm-new')
        with temporary.open('xb') as stream: stream.write(data)
        executable = name.parts[0] == 'bin' or name == Path('lib/xpwm/compositor/dwl') or (
            name.parent == Path('lib/xpwm/shell') and name.name in BINARIES)
        temporary.chmod(0o755 if executable else 0o644)
        temporary.replace(target)
        hashes[str(name)] = hashlib.sha256(data).hexdigest()
    # Preserve removed/obsolete files in the ledger so uninstall can clean them.
    old_files.update(hashes)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(dict(schema_version=1, prefix=str(prefix), files=old_files), indent=2) + '\n')
    print(f'Installed {len(files)} files under {root}')


def uninstall(prefix, destdir=None):
    root = destination(prefix, destdir)
    manifest = safe_target(root, MANIFEST)
    if not manifest.is_file(): raise ValueError(f'No XPWm install manifest at {manifest}')
    report = json.loads(manifest.read_text())
    remaining = {}
    paths = []
    for name, digest in report['files'].items():
        relative = Path(name)
        allowed = relative.parts[:2] in (('lib', 'xpwm'),) or str(relative) in (
            'bin/xpwm', 'bin/xpwm-import-assets', 'bin/xpwm-theme-preview', 'bin/xpwm-screenshot',
            'share/wayland-sessions/xpwm.desktop', 'share/applications/xpwm-theme-preview.desktop') or relative.parts[:3] in (
            ('share', 'doc', 'xpwm'), ('share', 'licenses', 'xpwm'))
        if not allowed: raise ValueError(f'Unexpected manifest path: {name}')
        paths.append((name, digest, safe_target(root, relative)))
    for name, digest, path in paths:
        if not path.exists(): continue
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            print(f'Preserving modified file: {path}')
            remaining[name] = digest
        else:
            path.unlink()
    if remaining:
        report['files'] = remaining
        manifest.write_text(json.dumps(report, indent=2) + '\n')
    else:
        manifest.unlink()
    # Only remove empty directories belonging to these installed paths.
    directories = {parent for _, _, path in paths for parent in path.parents
                   if parent != root and root in parent.parents}
    directories.add(manifest.parent)
    for directory in sorted(directories, key=lambda p: len(p.parts), reverse=True):
        try: directory.rmdir()
        except OSError: pass
    print('XPWm uninstall complete. User settings and private assets are preserved.')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system', action='store_true', help='Install for all users (default prefix: /usr/local)')
    parser.add_argument('--prefix', type=Path, help='Install prefix (default: ~/.local, or /usr/local with --system)')
    parser.add_argument('--destdir', type=Path, help='Stage installation for packaging without changing embedded prefix')
    parser.add_argument('--deps', action='store_true', help='Install build/runtime dependencies using the distro package manager')
    parser.add_argument('--package-manager', choices=('pacman', 'apt', 'dnf', 'zypper'), help='Override distro detection for --deps')
    parser.add_argument('--no-build', action='store_true', help='Install previously built binaries')
    parser.add_argument('--uninstall', action='store_true', help='Remove files recorded by this installer')
    parser.add_argument('--dry-run', action='store_true', help='Print the plan without building, installing packages, or writing files')
    args = parser.parse_args(argv)
    try:
        if sys.version_info < (3, 11): raise ValueError('Python 3.11 or newer is required for XPWm asset tools.')
        if sys.platform != 'linux': raise ValueError('XPWm requires Linux and a Wayland-capable graphics/seat stack.')
        prefix = (args.prefix or (Path('/usr/local') if args.system else Path.home() / '.local')).expanduser()
        if not prefix.is_absolute() or prefix == Path('/') or any(c in str(prefix) for c in '\n\r\0'):
            raise ValueError('Prefix must be an absolute directory other than /.')
        prefix = Path(os.path.abspath(prefix))
        destdir = args.destdir.expanduser().absolute() if args.destdir else None
        if args.uninstall and args.deps: raise ValueError('--uninstall and --deps cannot be combined.')
        if args.deps:
            manager = args.package_manager or manager_for(Path('/etc/os-release').read_text())
            run(dependency_command(manager), dry=args.dry_run, privileged=True)
        print(f'{"Uninstall" if args.uninstall else "Install"} prefix: {prefix}')
        if destdir: print(f'Packaging destination: {destination(prefix, destdir)}')
        if not args.uninstall:
            if not args.dry_run and not args.no_build: check_dependencies()
            if not args.no_build:
                # Building C code must not run the private-media asset generators.
                run(['make', '-C', str(ROOT / 'compositor')], dry=args.dry_run)
                run(['make', '-C', str(ROOT / 'shell')], dry=args.dry_run)
        if args.dry_run:
            print('Would remove manifest-owned files.' if args.uninstall else
                  'Would install the compositor, shell, source-only theme, import tools, launchers and session entry.')
            return 0
        target = destination(prefix, destdir)
        parent = target
        while not parent.exists(): parent = parent.parent
        if args.system and not destdir and os.geteuid() != 0 and not os.access(parent, os.W_OK):
            command = [sys.executable, str(Path(__file__).resolve()), '--system', '--prefix', str(prefix), '--no-build']
            if args.uninstall: command.append('--uninstall')
            run(command, privileged=True)
            return 0
        if args.uninstall:
            uninstall(prefix, destdir)
        else:
            deploy(prefix, destdir)
            print(f'Launch from a TTY: {shlex.quote(str(prefix / "bin/xpwm"))} tty')
            print('Launch nested: xpwm nested (inside an existing Wayland session).')
            print('Import optional XP media: xpwm-import-assets /path/to/windows-xp-sp3-x86.iso')
            print('Ensure the prefix bin directory is on PATH. Some display managers only read /usr/share/wayland-sessions; use --system --prefix /usr for those.')
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError, KeyError) as error:
        parser.exit(1, f'XPWm installation failed: {error}\n')


if __name__ == '__main__':
    sys.exit(main())
