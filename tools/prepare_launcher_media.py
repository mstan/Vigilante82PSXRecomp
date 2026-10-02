"""Bundled launcher worker. Converts owner media; never needs loose-file input."""
from pathlib import Path
import hashlib
import json
import os
import sys
import tempfile
import tomllib

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS), str(TOOLS / 'third_party'), str(TOOLS / 'third_party/combined')]
import import_original_cars
import import_original_presentation
import import_original_music
import import_dreamland
import vehicle_menu
from media_inputs import external_output, verified_volume, read_file

OUTPUTS = {
    'menus': {'menu': 'menu/SHELL.DLL'},
    'cars': {'archive': 'cars/package/CUSTOM.EXP', 'registry': 'cars/package/VEHICLES.V8R',
             'presentation': 'presentation/V8IDENTITY.DAT', 'outcomes': 'presentation/V8RESULTS.DAT'},
    'music': {'music': 'music/V8MUSIC.DAT'},
    'arena': {'arena': 'dreamland/package/LEVELS/N64/DREAMLND.EXP',
              'overlay': 'dreamland/package/LEVELS/N64/DREAMLND.DLL',
              'sounds': 'dreamland/package/SHARED/SOUNDS.SND',
              'menu': 'dreamland/package/SHELL/SHELL.DLL',
              'previews': 'dreamland/package/SHELL/LEVELSEL.TBL'},
}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def disc_image(path):
    """A BIN selected by the user should retain its sibling CUE's audio TOC."""
    path = Path(path).resolve()
    if path.suffix.lower() in ('.bin', '.img', '.iso'):
        for cue in sorted(path.parent.glob('*')):
            if cue.suffix.lower() != '.cue':
                continue
            try:
                if any(t['path'] == path for t in import_original_music.cue_tracks(cue)):
                    return cue
            except (OSError, ValueError):
                continue
    return path


def source_identity(path, include_audio=False):
    if path.suffix.lower() == '.cue':
        tracks = import_original_music.cue_tracks(path)
        chosen = tracks if include_audio else tracks[:1]
        return [(t['audio'], t['start'], t['count'], digest(t['path'])) for t in chosen]
    return digest(path)


def validate_outputs(folder, expected, mode):
    if set(expected) != set(OUTPUTS[mode]):
        raise ValueError('The launcher and media worker have different resource versions')
    for name, relative in OUTPUTS[mode].items():
        path = folder / relative
        identity = expected[name]
        if not path.is_file() or path.stat().st_size != identity['size'] or digest(path) != identity['sha256']:
            return False
    return True


def prepare(request, progress=lambda message: None):
    mode = request['feature']
    if mode not in OUTPUTS:
        raise ValueError('Unknown content package')
    cache = external_output(Path(request['cache'])) / 'v82-media-v1'
    cache.mkdir(parents=True, exist_ok=True)
    sequel = disc_image(request['disc'])
    progress('Checking the 2nd Offense disc…')
    verified_volume(sequel, 'v82')
    inputs = {k: Path(v) for k, v in request.get('inputs', {}).items()}
    if 'original' in inputs:
        inputs['original'] = disc_image(inputs['original'])
        if mode == 'music' and inputs['original'].suffix.lower() != '.cue':
            raise ValueError('Original soundtrack needs the full disc with audio tracks. Select its CUE or CHD; a data-only ISO has no music.')
        verified_volume(inputs['original'], 'v8')
    if mode == 'arena':
        rom = import_dreamland.V8N64Rom(inputs['n64'])
        if len(rom.data) != import_dreamland.ROM_SIZE or hashlib.sha256(rom.data).hexdigest() != import_dreamland.ROM_SHA256:
            raise ValueError('Expected Vigilante 8 USA N64 ROM')
    progress('Checking the content cache…')
    expected = request['outputs']
    identity = dict(mode=mode, outputs=expected,
        sequel=source_identity(sequel),
        inputs={k: source_identity(v, mode == 'music') for k, v in inputs.items()})
    key = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
    destination = cache / key
    if not validate_outputs(destination, expected, mode):
        with tempfile.TemporaryDirectory(prefix='preparing-', dir=cache) as temp:
            work = Path(temp)
            if mode == 'menus':
                volume, entries = verified_volume(sequel, 'v82')
                target = work / 'menu/SHELL.DLL'
                target.parent.mkdir(parents=True)
                target.write_bytes(vehicle_menu.convert(read_file(volume, entries, 'SHELL/SHELL.DLL')))
            elif mode == 'cars':
                progress('Preparing original vehicles and specials…')
                import_original_cars.prepare(inputs['original'], sequel, work / 'cars')
                progress('Preparing portraits and voices…')
                import_original_presentation.prepare(inputs['original'], sequel, work / 'presentation')
            elif mode == 'music':
                progress('Preparing original soundtrack…')
                import_original_music.prepare(inputs['original'], work / 'music')
            else:
                progress('Preparing Super Dreamland 64…')
                encoder = Path(request['encoder'])
                import_dreamland.prepare(inputs['n64'], None, sequel, encoder, work / 'dreamland')
            if not validate_outputs(work, expected, mode):
                raise ValueError('Converted content failed verification; no assets were activated')
            for relative in OUTPUTS[mode].values():
                target = destination / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                os.replace(work / relative, target)
    return {name: str(destination / relative) for name, relative in OUTPUTS[mode].items()}


def main():
    request_path, response_path = map(Path, sys.argv[1:3])
    try:
        request = tomllib.loads(request_path.read_text(encoding='utf-8'))
        result = prepare(request)
        response = '[outputs]\n' + ''.join(f'{json.dumps(k)} = {json.dumps(v)}\n' for k, v in result.items())
    except Exception as error:
        response = 'error = ' + json.dumps(str(error)) + '\n'
    temporary = response_path.with_suffix('.tmp')
    temporary.write_text(response, encoding='utf-8', newline='\n')
    os.replace(temporary, response_path)


if __name__ == '__main__':
    main()
