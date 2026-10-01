"""Conversion checks; optional owner ROM enables the complete reference audit."""
import os
from pathlib import Path
import re
import random
import struct
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path[:0] = [str(TOOLS), str(TOOLS / "third_party/combined"), str(TOOLS / "third_party")]
import dreamland_overlay as port
import port_dreamland_overlay as reference
from v8_n64_level import V8N64Rom
from translate_v8_n64_overlay_code import translate, DREAMLAND_FUNCTIONS


def reference_object_changes(source):
    """Feed actual instruction operands through the unchanged reference rules.

    This is a conversion oracle only, not executable code or another runtime.
    Calls appear after their delay instructions, matching reference emission.
    """
    gpr = reference.GPR_NAMES
    targets = port.engine_targets()
    chunks, memory = [], {}
    for name, start, end in DREAMLAND_FUNCTIONS:
        lines = []
        pending = None
        for at in range(start, end, 4):
            word = port.read32(source, at)
            op, rs, rt, rd = word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
            imm = port.signed16(word)
            addr = f"c.{gpr[rs]}" if imm == 0 else f"(c.{gpr[rs]} + 0x{imm & port.U32:X}u)"
            statement = None
            if op in (0x23, 0x2B):
                method = "LoadWord" if op == 0x23 else "StoreWord"
                statement = f"c.{method}({rt}, m, {addr});"
            elif op in (0x20, 0x21, 0x24, 0x25):
                statement = f"c.{gpr[rt]} = m.ReadU{8 if op in (0x20,0x24) else 16}({addr});"
            elif op in (0x28, 0x29):
                statement = f"m.WriteU{8 if op == 0x28 else 16}({addr}, c.{gpr[rt]});"
            elif op in (8, 9):
                statement = (f"c.CopyRegister({rt}, {rs});" if imm == 0 else
                             f"c.{gpr[rt]} = c.{gpr[rs]} + 0x{imm & port.U32:X}u;")
            elif op == 0 and (word & 63) in (0x21, 0x25) and (rs == 0 or rt == 0):
                statement = f"c.CopyRegister({rd}, {rt if rs == 0 else rs});"
            elif op in (10, 11, 12, 13, 14, 15):
                statement = f"c.{gpr[rt]} = 0u;"
            elif op == 0 and (word & 63) not in {8, 9, 0x11, 0x13, 0x18, 0x19, 0x1A, 0x1B}:
                statement = f"c.{gpr[rd]} = 0u;"
            if statement:
                lines.append(f"        {statement} // pc:{at:X}\n")
                if op in {0x20,0x21,0x23,0x24,0x25,0x28,0x29,0x2B,8,9} and not (op in (8,9) and imm == 0):
                    memory[at] = (rs, imm & 0xFFFF)
            if pending is not None:
                lines.append(f"        Vigilante82PC.func_{pending:08X}(c, m);\n")
                pending = None
            if op == 3:
                target = 0x80000000 | ((word & 0x03FFFFFF) << 2)
                pending = targets.get(target, target)
        chunks.append(f"    public static void {name}(CpuContext c, IMemory m)\n    {{\n" + "".join(lines) +
                      "    }\n    [System.Runtime.CompilerServices.MethodImpl]\n")
    converted = reference.convert_object_layout("".join(chunks) + "\n}\n")
    changes = {}
    for line in converted.splitlines():
        marker = re.search(r"// pc:([0-9A-F]+)", line)
        if marker is None:
            continue
        at = int(marker.group(1), 16)
        if at not in memory:
            continue
        rs, original = memory[at]
        match = re.search(rf"c\.{gpr[rs]} \+ 0x([0-9A-Fa-f]+)u", line)
        value = int(match.group(1), 16) & 0xFFFF if match else 0
        if value != original:
            changes[at] = value
    return changes


class Relocation(unittest.TestCase):
    def test_native_arithmetic(self):
        image = bytearray(32)
        struct.pack_into("<5I", image, 0, 32, 0x24, 0x3C020000, 0x2442FFF8, 0x08000008)
        image.extend(struct.pack("<6I", 4, 9, 0xFFF8, 14, 19, port.U32))
        result = port.relocate(image, 0x80388000)
        self.assertEqual(port.read32(result, 4), 0x80388024)
        self.assertEqual(port.read32(result, 8), 0x3C028039)
        self.assertEqual(port.read32(result, 12), 0x24427FF8)
        self.assertEqual(port.read32(result, 16), 0x080E2008)


class OwnerOverlay(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = os.environ.get("V82_DREAMLAND_ROM")
        if not path:
            raise unittest.SkipTest("V82_DREAMLAND_ROM is not set")
        cls.source = V8N64Rom(Path(path)).decoded("DREAMLND.DLL")
        cls.intermediate = translate(cls.source, 0x2C0, 0x3208)
        cls.result, cls.report = port.convert(cls.source)

    def test_object_layout_matches_reference(self):
        expected = reference_object_changes(self.intermediate)
        # Locally verified factory return contracts missing from the pinned
        # reference. Keep the remaining conversion audit byte-for-byte strict.
        expected.update({0x11A8: 0x4C, 0x11B0: 0x20,
                         0x183C: 4, 0x1844: 4, 0x1860: 0x48})
        # Both mushrooms configure an object returned by the local spawner;
        # their projectiles' +84 fields refer to the selected target vehicle.
        expected.update({0x16E8: 0x4C, 0x16EC: 0x4C,
                         0x19A8: 0x34, 0x19B8: 0x1C, 0x19BC: 4,
                         0x19C0: 0x34, 0x19CC: 4, 0x19F4: 0x22,
                         0x1A10: 0x28, 0x1A38: 0x2E, 0x1A70: 0x34,
                         0x1C28: 0x4C, 0x1C2C: 0x4C,
                         0x1ED4: 0x34, 0x1EE4: 0x34, 0x1EEC: 0x1C,
                         0x1F24: 0x22, 0x1F44: 0x28, 0x1F6C: 0x2E,
                         0x1FA4: 0x34, 0x1FB8: 9})
        actual = {}
        for patch in self.report["patches"]:
            if patch["reason"] in {"object-layout", "resource-owner", "callback-field"}:
                actual[int(patch["offset"],16)] = int(patch["after"],16) & 0xFFFF
        self.assertEqual(actual, expected)

    def test_effect_fields_preserve_native_lifecycle_header(self):
        # Replay the returned-object field accesses which caused the owner's
        # crash. Ground alignment writes a MATRIX; smoke modifies flags/roll.
        obj = bytearray(0x80)
        struct.pack_into("<I", obj, 0, 0x8004E03C)
        struct.pack_into("<I", obj, 0x10, 0x80622000)
        header = obj[:0x20]
        matrix = struct.pack("<9h", 0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000)
        matrix_at = port.signed16(port.read32(self.result, 0x11B0))
        obj[matrix_at:matrix_at+len(matrix)] = matrix
        self.assertEqual(obj[:0x20], header)
        self.assertEqual(obj[0x20:0x32], matrix)
        self.assertEqual(port.signed16(port.read32(self.result, 0x11A8)), 0x4C)

        struct.pack_into("<II", obj, 0, 0x8004DE1C, 0x34)
        load = port.signed16(port.read32(self.result, 0x183C))
        store = port.signed16(port.read32(self.result, 0x1844))
        port.put32(obj, store, port.read32(obj, load) | 0x400)
        rotation = port.signed16(port.read32(self.result, 0x1860))
        struct.pack_into("<h", obj, rotation, 0x600)
        self.assertEqual(port.read32(obj, 0), 0x8004DE1C)
        self.assertEqual(port.read32(obj, 4), 0x434)
        self.assertEqual(struct.unpack_from("<h", obj, 0x48)[0], 0x600)

    def test_mushroom_flags_preserve_callback_at_relocated_bases(self):
        # At the compilation probe base, OR 0x20 into the callback happens to
        # be inert. The owner's actual DLL base instead jumps into the middle
        # of the projectile prologue, then loads a bogus return address.
        rng = random.Random(0xD1EA)
        bases = [0x80380000, 0x8075E2B0, 0x8074EF00]
        bases += [0x80380000 + offset for offset in range(0, 0x100, 4)]
        bases += [rng.randrange(0x80300000, 0x807F0000, 4) for _ in range(4096)]
        for base in bases:
            obj = bytearray(0x98)
            callback = base + 0x15EC
            flags = rng.getrandbits(32)
            struct.pack_into("<II", obj, 0, callback, flags)
            load = port.signed16(port.read32(self.result, 0x19BC))
            store = port.signed16(port.read32(self.result, 0x19CC))
            port.put32(obj, store, port.read32(obj, load) | 0x20)
            self.assertEqual(port.read32(obj, 0), callback)
            self.assertEqual(port.read32(obj, 4), flags | 0x20)
        self.assertNotEqual((0x8075E2B0 + 0x15EC) | 0x20, 0x8075E2B0 + 0x15EC)

    def test_both_mushroom_projectiles_use_native_fields(self):
        for offsets in ((0x19B8, 0x19F4, 0x1A10, 0x1A38),
                        (0x1EEC, 0x1F24, 0x1F44, 0x1F6C)):
            self.assertEqual([port.signed16(port.read32(self.result, p)) for p in offsets],
                             [0x1C, 0x22, 0x28, 0x2E])
        for at in (0x19A8, 0x19C0, 0x1A70, 0x1ED4, 0x1EE4, 0x1FA4):
            self.assertEqual(port.signed16(port.read32(self.result, at)), 0x34)
        for at in (0x16E8, 0x16EC, 0x1C28, 0x1C2C):
            self.assertEqual(port.signed16(port.read32(self.result, at)), 0x4C)
        self.assertEqual(port.signed16(port.read32(self.result, 0x1FB8)), 9)

    def test_event_table_and_water(self):
        table = int(self.report["event_table"], 16)
        for base in (0x800A1838, port.LOAD_BASE):
            relocated = port.relocate(self.result, base)
            old = port.relocate(self.intermediate, base)
            expected = [port.read32(old, 0xA8+i*4) for i in range(18)]
            expected.insert(16, base+0x9C8)
            self.assertEqual([port.read32(relocated, table+i*4) for i in range(19)], expected)
            high = (port.read32(relocated, 0x660) & 0xFFFF) << 16
            self.assertEqual((high + port.signed16(port.read32(relocated, 0x664))) & port.U32, base+table)
            water = int(self.report["water_trampoline"],16)
            self.assertEqual(0x80000000 | ((port.read32(relocated, 0x984)&0x3FFFFFF)<<2), base+water)

    def test_callback_base_on_both_control_flow_paths(self):
        for earlier, delay, low in ((0x1AFC,0x1B08,0x1B0C),(0x2050,0x205C,0x2060)):
            for high in (earlier, delay):
                address = ((port.read32(self.result,high)&0xFFFF)<<16) + port.signed16(port.read32(self.result,low))
                self.assertEqual(address,port.HOST_MODEL_EVENT)

    def test_annulled_delay_both_paths(self):
        # The lowering always uses the inverse condition, an inert delay slot,
        # then the original delay followed by the original taken destination.
        lowered = [p for p in self.report["patches"] if p["reason"] == "branch-likely"]
        self.assertEqual(len(lowered), 36)
        for patch in lowered:
            at, old = int(patch["offset"],16), int(patch["before"],16)
            stub = (int(patch["after"],16) & 0x3FFFFFF) << 2
            branch = port.read32(self.result, stub)
            if old >> 26 == 1:
                self.assertEqual(branch >> 26, 1)
                self.assertEqual((branch >> 16) & 31, 3-((old >> 16) & 31))
                self.assertEqual(branch & 0x03E00000, old & 0x03E00000)
            else:
                self.assertEqual(branch >> 26, {0x14:5,0x15:4,0x16:7,0x17:6}[old>>26])
                self.assertEqual(branch & 0x03FF0000, old & 0x03FF0000)
            self.assertEqual(port.signed16(branch), 4)
            self.assertEqual(port.read32(self.result, stub+4), 0)
            self.assertEqual((port.read32(self.result, stub+12)&0x3FFFFFF)<<2, at+4+4*port.signed16(old))
            self.assertEqual((port.read32(self.result, stub+20)&0x3FFFFFF)<<2, at+8)
            self.assertEqual(port.read32(self.result, at+4), 0)


class OwnerMenu(unittest.TestCase):
    def test_relocated_records_and_consumers(self):
        path = os.environ.get("V82_DREAMLAND_SHELL")
        if not path:
            self.skipTest("V82_DREAMLAND_SHELL is not set")
        import dreamland_menu
        source = Path(path).read_bytes()
        result, report = dreamland_menu.convert(source)
        for base in (0x80100000,0x807D3828):
            old, new = port.relocate(source,base), port.relocate(result,base)
            table = report["table"]
            self.assertEqual(new[table:table+18*16],old[0x15BE8:0x15BE8+18*16])
            for high, low in ((0x47C0,0x47C4),(0x4F50,0x4F54),(0xFB7C,0xFB84)):
                resolved = (((port.read32(new,high)&0xFFFF)<<16)+port.signed16(port.read32(new,low)))&port.U32
                self.assertEqual(resolved,base+table)
            record = report["added_record"]
            path_at = port.read32(new,record+12)-base
            expected_path=b"Levels\\N64\\DreamLnd.exp\0"
            self.assertEqual(new[path_at:path_at+len(expected_path)],expected_path)
            for seam in report["vehicle_seams"]:
                at=seam["offset"]
                jump=lambda offset: 0x80000000|((port.read32(new,offset)&0x3FFFFFF)<<2)
                if "replacement" in seam:
                    self.assertEqual(port.read32(old,at),seam["expected"])
                    self.assertEqual(port.read32(new,at),seam["replacement"])
                elif "trampoline" in seam:
                    stub=seam["trampoline"]
                    self.assertEqual(jump(at),base+stub)
                    self.assertEqual(jump(stub+8),seam["callback"])
                    self.assertEqual(jump(stub+32),base+at+8)
                    self.assertEqual(new[stub+24:stub+32],old[at:at+8])
                    self.assertEqual(base+stub+16-port.read32(new,stub+40),base)
                else:
                    self.assertEqual(jump(at),seam["callback"])
        self.assertEqual(port.read32(result,0x45DC),0x24130013)
        self.assertEqual(port.read32(result,0x45A4),0x24130013)
        self.assertEqual(port.read32(result,0x4650),0x8EE40050)
        self.assertEqual(port.read32(result,0x10664),0x3C028020)
        self.assertEqual(port.read32(result,0x10668),0x24560000)


if __name__ == "__main__":
    unittest.main()
