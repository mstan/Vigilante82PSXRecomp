"""Stage V82's verified media worker, native cache and existing notices."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
from build_media_helper import preparer_revision

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--framework', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--stage', type=Path, required=True)
    args = parser.parse_args()
    framework, build, stage = args.framework, args.build_dir, args.stage
    worker = build / 'media-tools'
    if not (worker / 'preparer-revision').is_file():
        raise SystemExit('Verified media worker revision is missing')
    encoder = worker / ('psxavenc.exe' if sys.platform == 'win32' else 'psxavenc')
    expected = preparer_revision(ROOT / 'tools', encoder)
    if (worker / 'preparer-revision').read_text().strip() != expected:
        raise SystemExit('Media worker source/encoder/environment revision is stale; rebuild it')
    shutil.copytree(worker, stage / 'media-tools')

    shared = framework / 'tools/release_stage.py'
    emitter = stage / 'overlay_toolchain' / ('psxrecomp-game.exe' if sys.platform == 'win32' else 'psxrecomp-game')
    tag = subprocess.check_output([sys.executable, str(shared), 'cg-tag',
        '--runtime-include', str(framework / 'runtime/include'),
        '--recompiler', str(emitter), '--game-toml', str(stage / 'game.toml'),
        '--flavor-from-build', str(build)], text=True).strip()
    subprocess.run([sys.executable, str(shared), 'stage-cache', '--game-id',
        'SLUS-00868', '--cache-src-root', str(ROOT / 'build-release/cache'),
        '--stage', str(stage), '--cg-tag', tag], check=True)

    notices = [
        (ROOT / 'tools/third_party/NOTICE.md', 'V82-THIRD-PARTY-NOTICE.md'),
        (ROOT / 'tools/third_party/LICENSE_SDL3', 'SDL3-LICENSE.txt'),
    ]
    for directory in ('recomp-net', 'retcomm-rbengine'):
        notices.append((framework / 'lib' / directory / 'LICENSE', directory + '-LICENSE.txt'))
    for directory, filename in (('fmt', 'LICENSE.rst'), ('rabbitizer', 'LICENSE'), ('ELFIO', 'LICENSE.txt')):
        notices.append((framework / 'recompiler/lib' / directory / filename, directory + '-LICENSE.txt'))
    for source, name in notices:
        shutil.copy2(source, stage / 'licenses' / name)
    if sys.platform == 'win32':
        compiler = shutil.which('gcc')
        if not compiler:
            raise SystemExit('Cannot find compiler dependency notices')
        licenses = Path(compiler).parent.parent / 'share/licenses'
        for package in ('SDL3', 'freetype', 'harfbuzz', 'curl', 'gcc-libs', 'gcc', 'libpng', 'zlib'):
            source = licenses / package
            if source.is_dir():
                shutil.copytree(source, stage / 'licenses/mingw-dependencies' / package)


if __name__ == '__main__':
    main()
