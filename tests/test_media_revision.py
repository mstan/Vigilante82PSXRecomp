"""Importer build identity uses actual tools and encoder bytes."""
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from build_media_helper import preparer_revision


class MediaRevision(unittest.TestCase):
    def test_conversion_sources_encoder_and_versions_invalidate(self):
        with tempfile.TemporaryDirectory() as folder, patch('build_media_helper.importlib.metadata.version', return_value='1'):
            root = Path(folder)
            tools = root / 'tools'; tools.mkdir()
            (tools / 'prepare.py').write_text('first')
            encoder = root / 'encoder'; encoder.write_bytes(b'encoder')
            first = preparer_revision(tools, encoder)
            self.assertEqual(first, preparer_revision(tools, encoder))
            (tools / 'prepare.py').write_text('other')
            self.assertNotEqual(first, preparer_revision(tools, encoder))
            (tools / 'prepare.py').write_text('first')
            dependency = tools / 'third_party/module.py'; dependency.parent.mkdir(); dependency.write_text('converter')
            self.assertNotEqual(first, preparer_revision(tools, encoder))
            nested = preparer_revision(tools, encoder)
            dependency.write_text('modified converter')
            self.assertNotEqual(nested, preparer_revision(tools, encoder))
            ignored = tools / '__pycache__/unused.py'; ignored.parent.mkdir(); ignored.write_text('generated cache')
            unchanged = preparer_revision(tools, encoder)
            ignored.write_text('different generated cache')
            self.assertEqual(unchanged, preparer_revision(tools, encoder))
            dependency.unlink(); encoder.write_bytes(b'changed')
            self.assertNotEqual(first, preparer_revision(tools, encoder))
            encoder.write_bytes(b'encoder')
            with patch('build_media_helper.importlib.metadata.version', return_value='2'):
                self.assertNotEqual(first, preparer_revision(tools, encoder))


if __name__ == '__main__':
    unittest.main()
