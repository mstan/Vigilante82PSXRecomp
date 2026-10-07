"""Stage the same verified player payload for Windows ZIP and Linux AppImage."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

from check_release_config import check

ROOT = Path(__file__).resolve().parent.parent


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--build-dir', required=True, type=Path)
    ap.add_argument('--stage', required=True, type=Path)
    ap.add_argument('--recompiler-build', required=True, type=Path)
    ap.add_argument('--cache-dir', required=True, type=Path)
    ap.add_argument('--download-cache', required=True, type=Path)
    ap.add_argument('--platform', required=True, choices=('win', 'linux'))
    ap.add_argument('--framework', type=Path, default=ROOT / 'psxrecomp-v4')
    ap.add_argument('--mingw-bin', type=Path)
    ap.add_argument('--zip', type=Path)
    args = ap.parse_args()
    check()
    version = (ROOT / 'packaging/release/VERSION').read_text().strip()
    build, stage = args.build_dir.resolve(), args.stage.resolve()
    fw = args.framework.resolve()
    if args.platform == 'win' and (not args.mingw_bin or not args.zip):
        raise SystemExit('Windows packaging requires --mingw-bin and --zip')
    executable = 'Vigilante82PSXRecomp' + ('.exe' if args.platform == 'win' else '')
    with (build / executable).open('rb') as stream:
        magic = stream.read(4)
    if (args.platform == 'win' and not magic.startswith(b'MZ')) or (
            args.platform == 'linux' and magic != b'\x7fELF'):
        raise SystemExit('Runtime binary does not match the requested package platform')
    if (build / 'psx_game_version.txt').read_text().strip() != version:
        raise SystemExit('Build version does not match release VERSION')
    if stage.exists() and any(stage.iterdir()):
        raise SystemExit(f'Staging directory must be empty: {stage}')
    stage.mkdir(parents=True, exist_ok=True)

    def copy(source, destination):
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)

    copy(build / executable, stage / executable)
    for tree in ('assets', 'media-tools'):
        shutil.copytree(build / tree, stage / tree, symlinks=True)
    for file in ('openbios.bin', 'OpenBIOS.LICENSE'):
        copy(build / 'bios' / file, stage / 'bios' / file)
    for file in ('README.md', 'LICENSE', 'LICENSING.md', 'RELEASE_NOTES.md'):
        copy(ROOT / file, stage / file)
    for file in ('game.toml', 'START_HERE.txt', 'input.ini'):
        copy(ROOT / 'packaging/release' / file, stage / file)
    copy(ROOT / 'tools/third_party/NOTICE.md', stage / 'licenses/V82-THIRD-PARTY-NOTICE.md')
    copy(fw / 'LICENSE', stage / 'licenses/psxrecomp-LICENSE.txt')
    copy(ROOT / 'tools/third_party/LICENSE_SDL3', stage / 'licenses/SDL3-LICENSE.txt')
    for directory in ('recomp-net', 'retcomm-rbengine'):
        copy(fw / 'lib' / directory / 'LICENSE', stage / 'licenses' / (directory + '-LICENSE.txt'))
    for directory, filename in (('fmt', 'LICENSE.rst'), ('rabbitizer', 'LICENSE'), ('ELFIO', 'LICENSE.txt')):
        copy(fw / 'recompiler/lib' / directory / filename, stage / 'licenses' / (directory + '-LICENSE.txt'))
    ui = ROOT / 'recomp-ui'
    for source, name in ((ui / 'LICENSE', 'recomp-ui-LICENSE.txt'),
                         (ui / 'src/third_party/imgui/LICENSE.txt', 'imgui-LICENSE.txt'),
                         (ui / 'assets/common/fonts/NOTICE.md', 'FONT-NOTICE.md'),
                         (ui / 'assets/common/img/NOTICE.md', 'UI-IMAGE-NOTICE.md')):
        copy(source, stage / 'licenses' / name)
    if args.platform == 'win':
        license_root = args.mingw_bin.parent / 'share/licenses'
        for package in ('SDL3', 'freetype', 'harfbuzz', 'curl', 'gcc-libs', 'gcc', 'libpng', 'zlib'):
            source = license_root / package
            if source.is_dir():
                shutil.copytree(source, stage / 'licenses/mingw-dependencies' / package)
    for file in (fw / 'runtime/licenses').iterdir():
        if file.is_file():
            copy(file, stage / 'licenses' / file.name)
    (stage / 'saves').mkdir()
    (stage / 'RELEASE.txt').write_text(f'Vigilante82PSXRecomp {version}\n', encoding='utf-8')

    def shared(*arguments):
        return subprocess.check_output([sys.executable, str(fw / 'tools/release_stage.py'),
                                        *map(str, arguments)], text=True).strip()

    print(shared('stage-mods', '--build-path', build, '--stage', stage,
                 '--runtime-target', 'psx-runtime'))
    print(shared('stage-toolchain', '--stage', stage,
                 '--recomp-dir', args.recompiler_build.resolve(),
                 '--recomp-tools', fw / 'tools', '--recomp-include', fw / 'runtime/include',
                 '--dl-cache', args.download_cache.resolve(), '--platform', args.platform,
                 *(['--mingw-bin', args.mingw_bin.resolve()] if args.mingw_bin else [])))
    tag = shared('cg-tag', '--runtime-include', fw / 'runtime/include',
                 '--recompiler', args.recompiler_build.resolve() / ('psxrecomp-game.exe' if args.platform == 'win' else 'psxrecomp-game'),
                 '--game-toml', stage / 'game.toml', '--flavor-from-build', build)
    print(shared('stage-cache', '--game-id', 'SLUS-00868',
                 '--cache-src-root', args.cache_dir.resolve(), '--stage', stage, '--cg-tag', tag))
    forbidden = ('SCPH1001.BIN', 'overlay_captures.json', 'state.toml', 'disc.cfg', 'bios.cfg', 'user_settings.toml')
    for path in stage.rglob('*'):
        if path.name in forbidden or path.suffix.lower() in ('.cue', '.chd', '.iso', '.v8r', '.exp'):
            raise SystemExit(f'Personal state or donor media must not ship: {path}')
    if args.platform == 'win':
        imports = subprocess.check_output([str(args.mingw_bin / 'objdump.exe'), '-p', str(stage / executable)], text=True)
        allowed = set('kernel32 user32 gdi32 shell32 msvcrt advapi32 ws2_32 comdlg32 dbghelp ole32 oleaut32 winmm imm32 version setupapi dinput8 rpcrt4 hid cfgmgr32 opengl32 bcrypt secur32 ucrtbase ntdll d2d1 dwrite dxgi iphlpapi'.split())
        for line in imports.splitlines():
            if 'DLL Name:' in line:
                name = line.split('DLL Name:')[1].strip().lower()
                if name.removesuffix('.dll') not in allowed and not name.startswith('api-ms-win-'):
                    raise SystemExit(f'Runtime requires an unbundled non-system DLL: {name}')
        # Bind after all executable mutations, including any future signing.
        print(shared('stage-execution', '--binary', stage / executable,
                     '--manifest', build / (executable.removesuffix('.exe') + '.execution.json')))
        subprocess.run([sys.executable, str(fw / 'tools/create_release_zip.py'),
                        '--source', str(stage), '--output', str(args.zip.resolve())], check=True)
    else:
        # AppImage deployment can rewrite RPATH; its wrapper binds again to
        # the final ELF using this validated contract as the input manifest.
        print(shared('stage-execution', '--binary', stage / executable,
                     '--manifest', build / (executable + '.execution.json')))
    print(f'Verified {args.platform} player payload: {stage}')


if __name__ == '__main__':
    main()
