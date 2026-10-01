"""Apply the reference's V8:2 conversion to input MIPS, then use psxrecomp.

The ABI rules are imported from the attributed port_dreamland_overlay module.
No generated C is edited, and no second runtime or recompiler is required.
The seven reference native helper routines retain their explicit host call IDs.
"""
from __future__ import annotations
from collections import Counter
import struct

from port_dreamland_overlay import (
    ENGINE_CALLS, OBJECT_ROOTS, GPR_NAMES, native_object_offset,
)
from translate_v8_n64_overlay_code import DREAMLAND_FUNCTIONS, translate


U32 = 0xFFFFFFFF
HOST_WATER_INIT = 0x8FFF011C
HOST_MODEL_EVENT = 0x8FFF0120
LOAD_BASE = 0x80380000


def read32(data, at):
    return struct.unpack_from("<I", data, at)[0]


def put32(data, at, value):
    struct.pack_into("<I", data, at, value & U32)


def signed16(value):
    return (value & 0x7FFF) - (value & 0x8000)


def relocations(data):
    at = read32(data, 0)
    result = []
    while at + 4 <= len(data):
        entry = read32(data, at)
        at += 4
        if entry == U32:
            return result
        if (entry & ~3) + 4 > read32(data, 0):
            raise ValueError("Relocation lies outside overlay image")
        symbol = None
        if entry & 3 == 1:
            symbol = read32(data, at)
            at += 4
        result.append((entry, symbol))
    raise ValueError("Missing relocation terminator")


def relocate(data, base):
    """Native DLL relocation arithmetic, including 32-bit J overflow."""
    output = bytearray(data[:read32(data, 0)])
    for entry, symbol in relocations(data):
        at, kind = entry & ~3, entry & 3
        word = read32(output, at)
        if kind == 0:
            word += base
        elif kind == 1:
            word = (word & 0xFFFF0000) | (((base + symbol + 0x8000) >> 16) & 0xFFFF)
        elif kind == 2:
            word = (word & 0xFFFF0000) | ((word + base) & 0xFFFF)
        else:
            word += ((base << 4) & U32) >> 6
        put32(output, at, word)
    return bytes(output)


def engine_targets():
    sdk = dict(RotMatrixYXZ_gte=0x8004DAB4, SquareRoot0=0x8004C6E4,
               VectorNormalS=0x8004C800, VectorNormalSS=0x8004C844)
    return {(int(old[4:], 16) if old.startswith("FUN_") else sdk[old]):
            int(new[5:], 16) for old, new in ENGINE_CALLS.items()}


def convert(source):
    data = bytearray(translate(source, 0x2C0, 0x3208))
    original = bytes(data)
    records = relocations(data)
    image_end = read32(data, 0)
    audit = []

    def patch(at, word, reason):
        before = read32(data, at)
        if before != word:
            audit.append(dict(offset=hex(at), before=hex(before), after=hex(word), reason=reason))
            put32(data, at, word)

    targets = engine_targets()
    counts = Counter()
    for at in range(0x2C0, 0x3208, 4):
        word = read32(data, at)
        if word >> 26 not in (2, 3):
            continue
        target = 0x80000000 | ((word & 0x03FFFFFF) << 2)
        if target in targets:
            patch(at, (word & 0xFC000000) | ((targets[target] & 0x0FFFFFFF) >> 2), "engine-call")
            counts[target] += 1
    if set(counts) != set(targets):
        raise ValueError(f"Reference engine-call inventory changed: {set(targets) - set(counts)}")

    # Match the reference's stable object roots and transient object aliases.
    # A JAL's delay instruction executes before caller-saved registers change.
    caller_saved = set(range(2, 16)) | {24, 25}
    # The native effect factories return the allocated object too. Their callers
    # immediately edit its matrix/position or flags/rotation. Missing these
    # aliases leaves V8 offsets in V8:2 objects: castle's matrix overwrites the
    # child link, and projectile smoke edits its callback instead of its flags.
    # Verified against the owner's SLUS_008.68 factory return paths.
    returns_object = {0x8002C17C, 0x80052188, 0x8004DE54, 0x8004E128,
                      0x8FFF0108, 0x8FFF010C}  # contact effect, nearest vehicle
    # Local JAL operands are still unrelocated here. The shared mushroom
    # spawner returns the cloned projectile; both callers then configure it.
    returns_object.update(0x80000000 | start for name, start, _ in DREAMLAND_FUNCTIONS
                          if name == "DreamProjectileSpawn")
    homing_functions = {"DreamGoodProjectile", "DreamBadProjectile",
                        "mushroom_good", "mushroom_bad"}
    load_ops = {0x20, 0x21, 0x23, 0x24, 0x25}
    memory_ops = load_ops | {0x28, 0x29, 0x2B}
    for name, start, end in DREAMLAND_FUNCTIONS:
        root = GPR_NAMES.index(OBJECT_ROOTS[name]) if name in OBJECT_ROOTS else -1
        aliases = {root} if root >= 0 else set()
        pending_call = None
        for at in range(start, end, 4):
            word = read32(data, at)
            op, rs, rt, rd = word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
            offset = signed16(word)
            native_offset = offset
            if rs in aliases and op in memory_ops | {8, 9} and 0 <= offset < 0xE8:
                # addiu dst,root,0 passes the pointer itself, unlike a load/store.
                if offset or op in memory_ops:
                    native_offset = native_object_offset(offset)
                    patch(at, (word & 0xFFFF0000) | native_offset, "object-layout")
            # Reference also converts these two unambiguous secondary fields.
            if op in memory_ops | {8, 9} and native_offset == 0x58:
                native_offset = 0x5C
                patch(at, (word & 0xFFFF0000) | native_offset, "resource-owner")
            if op == 0x2B and native_offset == 0x64:
                native_offset = 0
                patch(at, word & 0xFFFF0000, "callback-field")

            if op in load_ops:
                # These four routines store/use a vehicle pointer at +84.
                # Other actors reuse that extension offset for scalar state.
                object_fields = {0x0C, 0x10, 0x14}
                if name in homing_functions:
                    object_fields.add(0x84)
                is_object = op == 0x23 and rs in aliases and native_offset in object_fields
                if rt != root:
                    aliases.discard(rt)
                if is_object:
                    aliases.add(rt)
            elif op == 0 and (word & 63) in (0x21, 0x25) and (rs == 0 or rt == 0):
                source_reg = rt if rs == 0 else rs
                is_object = source_reg in aliases
                if rd != root:
                    aliases.discard(rd)
                if is_object:
                    aliases.add(rd)
            elif op in {8, 9, 10, 11, 12, 13, 14, 15}:
                if rt != root:
                    aliases.discard(rt)
            elif op == 0 and (word & 63) not in {8, 9, 0x11, 0x13, 0x18, 0x19, 0x1A, 0x1B}:
                if rd != root:
                    aliases.discard(rd)
            if pending_call is not None:
                aliases.difference_update(caller_saved - {root})
                if pending_call in returns_object:
                    aliases.add(2)
                pending_call = None
            if op == 3:
                pending_call = 0x80000000 | ((read32(data, at) & 0x03FFFFFF) << 2)

    # Track LUI definitions so unrelated immediates cannot match global fields.
    globals_map = {0x800659FC: 0x8006B814, 0x800737E8: 0x8006B41C,
                   0x800605F8: 0x8006B41C, 0x80065A18: 0x8006B830,
                   0x80065A50: 0x8006B868, 0x80065A10: 0x8006B828,
                   0x800607B4: 0x80065C90, 0x8003E7B4: HOST_MODEL_EVENT}
    seen_globals = Counter()
    changed_high = {}
    for _, start, end in DREAMLAND_FUNCTIONS:
        lui = {}
        for at in range(start, end, 4):
            word = read32(original, at)
            op, rs, rt = word >> 26, (word >> 21) & 31, (word >> 16) & 31
            if op == 15:
                lui[rt] = (at, (word & 0xFFFF) << 16)
                continue
            if rs in lui and op in memory_ops | {8, 9}:
                hi_at, high = lui[rs]
                address = (high + signed16(word)) & U32
                if address in globals_map:
                    target = globals_map[address]
                    target_hi = ((target + 0x8000) >> 16) & 0xFFFF
                    if hi_at in changed_high and changed_high[hi_at] != target_hi:
                        raise ValueError("Shared LUI needs incompatible global conversion")
                    changed_high[hi_at] = target_hi
                    patch(hi_at, (read32(data, hi_at) & 0xFFFF0000) | target_hi, "global-high")
                    patch(at, (read32(data, at) & 0xFFFF0000) | (target & 0xFFFF), "global-low")
                    seen_globals[address] += 1
            if op in load_ops | {8, 9, 10, 11, 12, 13, 14}:
                lui.pop(rt, None)
            elif op == 0:
                lui.pop((word >> 11) & 31, None)
    if set(seen_globals) != set(globals_map):
        raise ValueError(f"Reference global inventory changed: {set(globals_map) - set(seen_globals)}")
    # The reference replaces every 0x80040000 callback base. Two conditional
    # paths reach the same low-half add through an earlier LUI, bypassing the
    # nearest lexical LUI. Both definitions must form the host callback ID.
    for at in range(0x2C0, 0x3208, 4):
        word = read32(original, at)
        if word >> 26 == 15 and word & 0xFFFF == 0x8004:
            patch(at, (word & 0xFFFF0000) | (HOST_MODEL_EVENT >> 16), "model-callback-base")

    # Append the native event-16 slot; relocate table pointers as usual.
    if read32(data, 0x648) != 0x2CA20012 or read32(data, 0x660) != 0x3C020000 or read32(data, 0x664) != 0x244200A8:
        raise ValueError("Primary event table instruction identity changed")
    data = data[:image_end]
    table = len(data)
    events = [read32(original, 0xA8 + i * 4) for i in range(18)]
    events.insert(16, 0x9C8)
    for event in events:
        records.append((len(data), None))
        data.extend(struct.pack("<I", event))
    patch(0x648, 0x2CA20013, "native-event-bound")
    patch(0x664, 0x24420000 | table, "native-event-table")
    records = [(entry, table if entry == 0x661 else symbol) for entry, symbol in records]

    # Water setup precedes exactly the one-time event-1 path, as in the reference.
    water = len(data)
    displaced = [read32(data, 0x984), read32(data, 0x988)]
    if displaced != [0x3C048007, 0x2484B868]:
        raise ValueError("Native event-1 water initialization anchor moved")
    data.extend(struct.pack("<6I", 0x0C000000 | ((HOST_WATER_INIT & 0x0FFFFFFF) >> 2), 0,
                            *displaced, 0x08000000 | (0x98C >> 2), 0))
    records.append(((water + 16) | 3, None))
    patch(0x984, 0x08000000 | (water >> 2), "water-initializer")
    patch(0x988, 0, "water-initializer-delay")
    records.append((0x987, None))
    # Export and event tables contain relocated pointers into the known code
    # interval. Retain every one as explicit dispatch evidence; the overlay
    # compiler deliberately does not sweep unobserved bytes to guess entries.
    pointer_entries = sorted({read32(data, entry) for entry, _ in records
                              if entry & 3 == 0 and 0x2C0 <= read32(data, entry) < 0x3208})

    # N64 branch-likely annuls its delay instruction when the branch is false.
    # R3000 has no such opcode. Lower it into ordinary branches in input MIPS;
    # every original PC remains stable for callback/relocation identities.
    trampolines = [dict(name="NativeWaterInit", offset=water, size=24)]
    continuations = {0x98C}
    for at in range(0x2C0, 0x3208, 4):
        word = read32(data, at)
        op = word >> 26
        rt = (word >> 16) & 31
        regimm_likely = op == 1 and rt in {2, 3}
        if op not in {0x14, 0x15, 0x16, 0x17} and not regimm_likely:
            continue
        target = at + 4 + 4 * signed16(word)
        delay = read32(data, at + 4)
        # A branch in a delay slot has no defined lowering here.
        if delay >> 26 in {1, 2, 3, 4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17}:
            raise ValueError("Branch-likely contains a control transfer in its delay slot")
        stub = len(data)
        inverse_branch = ((1 << 26) | (word & 0x03E00000) | ((3-rt) << 16) if regimm_likely else
                          ({0x14: 5, 0x15: 4, 0x16: 7, 0x17: 6}[op] << 26) | (word & 0x03FF0000))
        words = [inverse_branch | 4, 0, delay,
                 0x08000000 | (target >> 2), 0, 0x08000000 | ((at + 8) >> 2), 0]
        data.extend(struct.pack("<7I", *words))
        records = [(((stub + 8) | (entry & 3)) if (entry & ~3) == at + 4 else entry, symbol)
                   for entry, symbol in records]
        records.extend([(at | 3, None), ((stub + 12) | 3, None), ((stub + 20) | 3, None)])
        patch(at, 0x08000000 | (stub >> 2), "branch-likely")
        patch(at + 4, 0, "annulled-delay-relocated")
        trampolines.append(dict(name=f"Annul_{at:04X}", offset=stub, size=28))
        continuations.update((target, at + 8))
    # Lowering splits an original routine across appended trampolines. Keep
    # every original conditional edge explicit, including paths whose nearest
    # lexical root now exits through a trampoline before reaching that edge.
    for at in range(0x2C0, 0x3208, 4):
        word = read32(data, at)
        op = word >> 26
        if op in {1, 4, 5, 6, 7}:
            continuations.update((at + 8, at + 4 + 4*signed16(word)))
        elif op == 2 and 0x2C0 <= (target := (word & 0x3FFFFFF) << 2) < 0x3208:
            continuations.add(target)
    put32(data, 0, len(data))
    for entry, symbol in records:
        data.extend(struct.pack("<I", entry))
        if symbol is not None:
            data.extend(struct.pack("<I", symbol))
    data.extend(struct.pack("<I", U32))
    return bytes(data), dict(patches=audit, engine_calls=sum(counts.values()),
                            global_uses=sum(seen_globals.values()),
                            water_trampoline=hex(water), event_table=hex(table),
                            functions=[dict(name=n, offset=s, size=e-s) for n,s,e in DREAMLAND_FUNCTIONS],
                            trampolines=trampolines, continuations=sorted(continuations),
                            pointer_entries=pointer_entries)


def seeds(report, base=LOAD_BASE):
    lines = [f"0x{base+f['offset']:08X}" for f in report['functions']]
    # These are direct J targets emitted above, not heuristic function guesses.
    lines.extend(f"call_root 0x{base+f['offset']:08X}" for f in report['trampolines'])
    entries = {f['offset'] for f in report['functions']}
    additional = set(report['continuations']) | set(report['pointer_entries'])
    lines.extend(f"call_root 0x{base+pc:08X}" for pc in sorted(additional - entries))
    return ("\n".join(lines) + "\n").encode()


def psx_executable(data, base=LOAD_BASE):
    image = relocate(data, base)
    padded = image + bytes((-len(image)) % 2048)
    header = bytearray(2048)
    header[:8] = b"PS-X EXE"
    for offset, value in ((0x10, base + 0x63C), (0x18, base), (0x1C, len(padded)), (0x30, 0x801FFFF0)):
        put32(header, offset, value)
    return bytes(header) + padded
