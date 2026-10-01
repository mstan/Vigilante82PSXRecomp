import os
from pathlib import Path
import sys
import unittest
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from dreamland_audio import resident_sample


@unittest.skipUnless(os.environ.get('V82_PSXAVENC'),'Owner encoder path not configured')
class Audio(unittest.TestCase):
    def test_duration_pitch_and_loop(self):
        encoder=Path(os.environ['V82_PSXAVENC'])
        samples=np.sin(np.arange(8000)*2*np.pi*300/8000)*12000
        for loop in (None,560):
            data,pitch,report=resident_sample(samples,loop,744,encoder)
            self.assertEqual(pitch,372)
            self.assertEqual(report['resident_sample_count'],4000)
            self.assertLessEqual(abs((len(data)//16-1)*28/pitch-len(samples)/744),28/pitch)
            self.assertEqual(data[:16],bytes(16))
            self.assertEqual(data[-15],1 if loop is None else 3)
            if loop is not None:
                self.assertEqual(report['loop_spu_frame'],10)
                self.assertEqual(data[10*16+1],6)


if __name__=='__main__':unittest.main()
