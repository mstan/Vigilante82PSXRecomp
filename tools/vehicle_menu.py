"""Native original-roster seams, adapted from V82VehicleRegistry.

Modify the verified input DLL, never generated code. The appended MIPS
bridges preserve the native stack/return address and use trusted title callbacks.
Copyright (c) 2026 flaffy. MIT; third_party/LICENSE_RECOMPONE.
"""
from dreamland_overlay import read32, put32

HOST = 0x8FFF0300


def patch(data, records):
    audit = []

    # Original Y expands to 119,936 bytes of MDEC run-length data, beyond
    # SHELL's fixed scratch at 8009CA68 and into live scene data. Keep the
    # native VLC/MDEC decoder and give its output a dedicated 1 MiB region
    # below the combined-content metadata (80300000) and heap (80400000).
    for at,expected,replacement in ((0x10664,0x3C02800A,0x3C028020),
                                    (0x10668,0x2456CA68,0x24560000)):
        if read32(data,at)!=expected:raise ValueError('Portrait scratch seam changed')
        put32(data,at,replacement)
        audit.append(dict(offset=at,expected=expected,replacement=replacement))

    def inline(at, kind, expected):
        words = [read32(data, at), read32(data, at+4)]
        if tuple(words) != expected:
            raise ValueError(f"Vehicle selector seam changed at {at:X}")
        start = len(data)
        moved = list(words)
        # Preserve a native conditional branch when moving its delay slot.
        # The duplicate-car comparison in the two-player selector uses one.
        for i, word in enumerate(moved):
            if word >> 26 in (4, 5):
                displacement = (word & 0x7FFF) - (word & 0x8000)
                target = at + i*4 + 4 + displacement*4
                displacement = (target - (start + 24 + i*4 + 4)) // 4
                if not -32768 <= displacement <= 32767:
                    raise ValueError('Selector trampoline branch out of range')
                moved[i] = (word & 0xFFFF0000) | (displacement & 0xFFFF)
        # Callback RA points to the restore sequence. Its trailing constant
        # lets the callback recover this DLL's actual relocation base.
        code = [0x27BDFFE8, 0xAFBF0010,
                0x0C000000 | ((HOST+kind*4)&0xFFFFFFF)//4, 0,
                0x8FBF0010, 0x27BD0018, *moved,
                0x08000000 | ((at+8)//4), 0, start+16]
        data.extend(b''.join(w.to_bytes(4,'little') for w in code))
        for i, (entry, symbol) in enumerate(records):
            if at <= (entry & ~3) < at+8:
                records[i] = (entry-at+start+24, symbol)
        records.extend(((at|3, None), ((start+32)|3, None)))
        put32(data, at, 0x08000000 | (start//4))
        put32(data, at+4, 0)
        audit.append(dict(offset=at, callback=HOST+kind*4, trampoline=start))

    inline(0x669C,0,(0x27BDFDC8,0xAFBE0230)) # begin player
    inline(0x6808,1,(0x3C028007,0x2442B08C)) # safe retail slot
    inline(0x6818,2,(0x8FAB01E8,0))          # variant load has settled
    inline(0x7928,3,(0x3C0308D0,0x00431024)) # accepted navigation
    inline(0x7AA4,4,(0x8FBF0234,0x8FBE0230)) # final native return value
    inline(0x6E94,5,(0x27A30080,0x00712821)) # four stat bars
    inline(0x7544,7,(0x3C0A0000,0xA0550002)) # independent color writes
    inline(0x75C0,7,(0xAFA001E0,0x90460003)) # independent color reads
    # Player's initial color-edit record, uniquely identified in retail input.
    matches=[p for p in range(0x669C,0x7AD0,4)
             if read32(data,p)==0x00822021 and read32(data,p+4)==0x90830002]
    if len(matches)!=1:raise ValueError('Vehicle color seam inventory changed')
    # Run after the address addition, before its first consumer.
    at=matches[0]+4
    inline(at,6,(read32(data,at),read32(data,at+4)))

    # The native two-player selector is a separate simultaneous carousel.
    inline(0x7AD4,8,(0x27BDFEE0,0xAFBF011C)) # begin both players
    inline(0x7F84,9,(0x8FAD00E8,0x00000000)) # current player after input poll
    inline(0x87C8,10,(0x0C016FBC,0x24040001)) # settled navigation/allowed mask
    inline(0x8668,11,(0x1462000E,0x3C020000)) # compare actual identities
    inline(0x8914,12,(0x8FAC0128,0x00000000)) # after both native slot stores
    inline(0x8B08,13,(0x8FA2005C,0x8FA30058)) # return packed player identities
    inline(0x845C,14,(0x9043002C,0x8CA20000)) # authored performance bars
    inline(0x8A24,15,(0x24050190,0x24070003)) # optional two-player enemy editor

    calls={0x8001A3B0:16, 0x8001EF34:17, 0x8003C464:18,
           0x80036BE0:19, 0x800149AC:20, 0x8010536C:21,
           0x801109FC:22, 0x8001E28C:23, 0x8001E188:24,
           0x8001ECCC:25}
    # Identify internal JALs through their relocation records, external ones
    # through their encoded address. No matching arbitrary immediates.
    relative={e&~3 for e,s in records if e&3==3}
    for lo,hi in ((0x5440,0x62C0),(0x669C,0x8B48)):
        for at in range(lo,hi,4):
            word=read32(data,at)
            if word>>26!=3:continue
            target=0x80000000|((word&0x3FFFFFF)<<2)
            if at in relative:target+=0x100000
            if target not in calls:continue
            kind=calls[target]
            put32(data,at,0x0C000000|((HOST+kind*4)&0xFFFFFFF)//4)
            records[:]=[(e,s) for e,s in records if (e&~3)!=at]
            audit.append(dict(offset=at,callback=HOST+kind*4,target=target))
    return audit


def convert(source: bytes) -> bytes:
    """Cars-only native menu: retain all eighteen retail arena entries."""
    import struct
    from dreamland_overlay import relocations
    records = relocations(source)
    data = bytearray(source[:read32(source, 0)])
    patch(data, records)
    put32(data, 0, len(data))
    for entry, symbol in records:
        data.extend(struct.pack('<I', entry))
        if symbol is not None:
            data.extend(struct.pack('<I', symbol))
    data.extend(struct.pack('<I', 0xFFFFFFFF))
    return bytes(data)
