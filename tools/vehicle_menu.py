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
        # Callback RA points to the restore sequence. Its trailing constant
        # lets the callback recover this DLL's actual relocation base.
        code = [0x27BDFFE8, 0xAFBF0010,
                0x0C000000 | ((HOST+kind*4)&0xFFFFFFF)//4, 0,
                0x8FBF0010, 0x27BD0018, *words,
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

    calls={0x8001A3B0:16, 0x8001EF34:17, 0x8003C464:18,
           0x80036BE0:19, 0x800149AC:20, 0x8010536C:21,
           0x801109FC:22, 0x8001E28C:23, 0x8001E188:24}
    # Identify internal JALs through their relocation records, external ones
    # through their encoded address. No matching arbitrary immediates.
    relative={e&~3 for e,s in records if e&3==3}
    for lo,hi in ((0x5440,0x62C0),(0x669C,0x7AD4)):
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
