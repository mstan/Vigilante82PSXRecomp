"""Package the launcher worker and its runtime; no Python install needed to play."""
import argparse
import importlib.metadata
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys


def preparer_revision(tools, encoder):
    """Small build-owned identity; warm launches never hash importer binaries."""
    digest = hashlib.sha256(b'v82-media-receipt-1\0')
    for source in sorted(tools.rglob('*.py')):
        if {'__pycache__', 'generated'} & set(source.relative_to(tools).parts[:-1]):
            continue
        digest.update(source.relative_to(tools).as_posix().encode('utf-8') + b'\0')
        with source.open('rb') as stream:
            digest.update(hashlib.file_digest(stream, 'sha256').digest())
    with encoder.open('rb') as stream:
        digest.update(hashlib.file_digest(stream, 'sha256').digest())
    digest.update(sys.version.encode('utf-8'))
    for name in ('numpy', 'pillow', 'pyinstaller'):
        digest.update((name + '=' + importlib.metadata.version(name)).encode('utf-8') + b'\0')
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--encoder', type=Path, required=True)
    args = parser.parse_args()
    tools = Path(__file__).resolve().parent
    if not args.encoder.is_file():
        raise SystemExit('Set V82_PSXAVENC to psxavenc 0.3.1 before building')
    revision = preparer_revision(tools, args.encoder)
    output = args.output.resolve()
    build = output.parent / 'media-worker-build'
    command = [sys.executable, '-m', 'PyInstaller', '--noconfirm', '--onedir',
               '--windowed', '--name', 'v82-media', '--distpath', str(build / 'dist'),
               '--workpath', str(build / 'work'), '--specpath', str(build),
               '--paths', str(tools), '--paths', str(tools / 'third_party'),
               '--paths', str(tools / 'third_party/combined'),
               str(tools / 'prepare_launcher_media.py')]
    subprocess.run(command, check=True)
    shutil.copytree(build / 'dist/v82-media', output, dirs_exist_ok=True)
    shutil.copy2(args.encoder, output / args.encoder.name)
    for name in ('NOTICE.md', 'LICENSE_RECOMPONE', 'LICENSE_PSXAVENC'):
        shutil.copy2(tools / 'third_party' / name, output / name)
    licenses = output / 'licenses'
    licenses.mkdir(exist_ok=True)
    python_license = Path(sys.base_prefix) / 'LICENSE.txt'
    if not python_license.is_file():
        python_license = tools / 'third_party/LICENSE_PYTHON'
    shutil.copy2(python_license, licenses / 'Python-LICENSE.txt')
    for name in ('numpy', 'pillow', 'pyinstaller'):
        distribution = importlib.metadata.distribution(name)
        for entry in distribution.files or ():
            if entry.name.lower().startswith(('license', 'copying')):
                destination = licenses / name / entry
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(distribution.locate_file(entry), destination)
    if preparer_revision(tools, args.encoder) != revision:
        raise SystemExit('Media importer inputs changed while packaging; rebuild')
    marker = output / 'preparer-revision.tmp'
    marker.write_text(revision + '\n', encoding='ascii', newline='\n')
    marker.replace(output / 'preparer-revision')
    (output / '.ready').write_text(revision + '\n', encoding='ascii', newline='\n')


if __name__ == '__main__':
    main()
