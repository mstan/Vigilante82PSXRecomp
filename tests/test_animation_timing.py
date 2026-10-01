"""Native terminal events use cycle timestamps, including a retained tail hold."""
import struct
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools/third_party"))
from vigilante8_vehicle_tools import compiler, project, registry, xobf


class AnimationTiming(unittest.TestCase):
    def roundtrip(self, times, end, target):
        # Independent native stream, with a loop into an interior frame.
        raw = struct.pack("<II", end, 8)
        raw += b"".join(struct.pack("<hh", time, 0) for time in times)
        raw += struct.pack("<hh", end, (target - len(times)) * 4)
        decoded = registry._decode_animations(raw, 1)
        bank = SimpleNamespace(slots=(None,), animations=decoded)
        rebuilt = compiler.compile_animation(bank)
        self.assertEqual(rebuilt, raw)
        self.assertEqual(decoded[0].loop_time, end)
        self.assertEqual(decoded[0].loop_frame, target)
        return rebuilt

    def test_clyde_launch_time(self):
        self.roundtrip(range(0, 81, 2), 80, 0)

    def test_terminal_hold_and_nonzero_loop_target(self):
        self.roundtrip((0, 10, 26), 30, 1)

    def test_new_animation_uses_latest_timestamp(self):
        animation = project.SlotAnimation(0, True, tuple(
            project.AnimationFrame(time) for time in (0, 10, 20)))
        data = compiler.compile_animation(SimpleNamespace(slots=(None,), animations=(animation,)))
        self.assertEqual(xobf.AnimationBank(data, 1).frames(0)[-1].frame_delta, 20)


if __name__ == "__main__":
    unittest.main()
