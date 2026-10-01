"""Native XA completion contract, independent of copyrighted media."""
from pathlib import Path
import sys
import unittest

TOOLS=Path(__file__).resolve().parents[1]/"tools"
sys.path[:0]=[str(TOOLS),str(TOOLS/"third_party/combined"),str(TOOLS/"third_party")]
from import_original_presentation import native_result_stream


class ResultStream(unittest.TestCase):
    def test_speech_cadence_and_native_end_callback(self):
        source=bytearray()
        for index in range(24):
            subheader=bytes((1,0 if index%8==0 else 1,0x64,0))
            source.extend(subheader*2+bytes((index,))*2328)
        result=native_result_stream(source)
        accepted=[]
        stopped=None
        for index in range(24):
            sector=result[index*2336:(index+1)*2336]
            self.assertEqual(sector[:4],sector[4:8])
            if sector[2]&0x44==0x44:
                self.assertEqual(sector,source[index*2336:(index+1)*2336])
                accepted.append(index)
            else:
                # The native callback gets the data-sector MSF, converts it
                # to LBA and pauses when LBA > file_start+sector_count-2.
                self.assertEqual(sector[:4],bytes((1,0,0,0)))
                if index>24-2:stopped=index
        self.assertEqual(accepted,[0,8,16])
        self.assertEqual(stopped,23)

    def test_invalid_cadence(self):
        for data in (b"",bytes(2336),bytes(2336*8-1)):
            with self.assertRaises(ValueError):native_result_stream(data)


if __name__=="__main__":unittest.main()
