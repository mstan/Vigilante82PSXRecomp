"""Prepare lossless original CD audio from an owner-supplied USA v1.0 CUE.

Output stays outside Git. Track order/names follow the authorized reference's
V8LooseManifest; samples come exclusively from the user's disc.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
from media_inputs import external_output, verified_volume

NAMES = ('Boogie', 'Beezwax', 'Convoy', 'Dave', 'Houston', 'John Torque',
         'Loki', 'Molo', 'Sheila', 'Sid', 'Slick Clyde', 'Chassey')


def cue_tracks(cue):
    tracks, source = [], None
    for line in cue.read_text(encoding='utf-8-sig').splitlines():
        line = line.strip()
        match = re.fullmatch(r'FILE "([^"]+)" BINARY', line, re.I)
        if match:
            source = (cue.parent / match[1]).resolve()
            continue
        match = re.fullmatch(r'TRACK (\d+) (AUDIO|MODE2/2352)', line, re.I)
        if match:
            if source is None or int(match[1]) != len(tracks)+1:
                raise ValueError('Expected consecutive raw BIN/CUE tracks')
            tracks.append(dict(path=source, audio=match[2].upper() == 'AUDIO', indices={}))
            continue
        match = re.fullmatch(r'INDEX (00|01) (\d+):(\d+):(\d+)', line, re.I)
        if match and tracks:
            minutes, seconds, frames = map(int, match.groups()[1:])
            if seconds >= 60 or frames >= 75:
                raise ValueError('Invalid CUE time')
            tracks[-1]['indices'][int(match[1])] = (minutes*60+seconds)*75+frames
            continue
        if line and not line.upper().startswith('REM '):
            raise ValueError(f'Unsupported CUE directive: {line}')
    for i, track in enumerate(tracks):
        size = track['path'].stat().st_size
        if size % 2352 or 1 not in track['indices']:
            raise ValueError('Truncated track or missing INDEX 01')
        start, end = track['indices'][1], size//2352
        if i+1 < len(tracks) and tracks[i+1]['path'] == track['path']:
            end = tracks[i+1]['indices'].get(0, tracks[i+1]['indices'].get(1, -1))
        if not 0 <= track['indices'].get(0, start) <= start < end <= size//2352:
            raise ValueError('Overlapping or empty CUE track')
        track.update(start=start, count=end-start)
    return tracks


def prepare(cue, output):
    output = external_output(output)
    tracks = cue_tracks(cue.resolve())
    if len(tracks) != 13 or tracks[0]['audio'] or not all(t['audio'] for t in tracks[1:]):
        raise ValueError('Expected original V8 USA data track and twelve audio tracks')
    verified_volume(cue, 'v8')
    header = bytearray(struct.pack('<4sI', b'V8CD', 12)+bytes(12*8))
    count = 0
    for i, track in enumerate(tracks[1:]):
        struct.pack_into('<II', header, 8+i*8, count, track['count'])
        count += track['count']
    output.mkdir(parents=True, exist_ok=True)
    destination = output/'V8MUSIC.DAT'
    digest = hashlib.sha256(header)
    report = []
    with destination.open('wb') as target:
        target.write(header)
        for name, track in zip(NAMES, tracks[1:]):
            song_hash = hashlib.sha256()
            with track['path'].open('rb') as source:
                source.seek(track['start']*2352)
                remaining = track['count']*2352
                while remaining:
                    block = source.read(min(remaining, 1024*1024))
                    if not block:
                        raise ValueError('Truncated audio source')
                    target.write(block); digest.update(block); song_hash.update(block)
                    remaining -= len(block)
            report.append(dict(name=name, sectors=track['count'], sha256=song_hash.hexdigest()))
    result = dict(size=destination.stat().st_size, sha256=digest.hexdigest(), tracks=report)
    (output/'music.json').write_text(json.dumps(result, indent=2)+'\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cue', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.cue, args.output), indent=2))
