"""Synthetic CUE fixtures; no donor music is required or stored in Git."""
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from import_original_music import cue_tracks, prepare


class MusicImport(unittest.TestCase):
    def test_track_boundaries_and_lossless_pack(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root/'data.bin').write_bytes(bytes(2352))
            cue = ['FILE "data.bin" BINARY', 'TRACK 01 MODE2/2352', 'INDEX 01 00:00:00']
            for i in range(12):
                (root/f'{i}.bin').write_bytes(bytes(2352)+bytes([i+1])*2352)
                cue += [f'FILE "{i}.bin" BINARY', f'TRACK {i+2:02d} AUDIO',
                        'INDEX 00 00:00:00', 'INDEX 01 00:00:01']
            path = root/'disc.cue';path.write_text('\n'.join(cue))
            with patch('import_original_music.verified_volume') as verify:
                first = prepare(path, root/'first');second = prepare(path, root/'second')
                self.assertEqual(verify.call_count, 2)
                verify.assert_called_with(path, 'v8')
            self.assertEqual(first, second)
            packed = (root/'first/V8MUSIC.DAT').read_bytes()
            self.assertEqual(struct.unpack_from('<4sI', packed), (b'V8CD', 12))
            for i in range(12):
                self.assertEqual(struct.unpack_from('<II', packed, 8+8*i), (i, 1))
                self.assertEqual(packed[104+i*2352:104+(i+1)*2352], bytes([i+1])*2352)

    def test_shared_file_and_invalid_indices(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'all.bin').write_bytes(bytes(8*2352))
            path=root/'disc.cue'
            text='FILE "all.bin" BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:00:00\nTRACK 02 AUDIO\nINDEX 00 00:00:03\nINDEX 01 00:00:05\n'
            path.write_text(text)
            tracks=cue_tracks(path)
            self.assertEqual([(t['start'],t['count']) for t in tracks], [(0,3),(5,3)])
            for broken in (text.replace('00:00:03','00:00:09'),text.replace('00:00:05','00:00:09')):
                path.write_text(broken)
                with self.assertRaises(ValueError):cue_tracks(path)


if __name__ == '__main__':unittest.main()
