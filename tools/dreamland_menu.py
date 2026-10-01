"""Extend the native location table using the reference arena-selector layout.

The retail menu keeps its input, drawing and launch code. Relocated input DLL
data replaces the reference's host-side table redirection; no generated C edits.
"""
import struct
from dreamland_overlay import read32, put32, relocations


def convert(source: bytes) -> tuple[bytes, dict]:
    table, count, stride = 0x15BE8, 18, 16
    records = relocations(source)
    end = read32(source, 0)
    if end != 0x16C50:
        raise ValueError("Unexpected sequel SHELL layout")
    data = bytearray(source[:end])
    guards = {0x45A4: 0x24130008, 0x45DC: 0x24130012, 0x4650: 0x8EE4004C,
              0x47C4: 0x24425BE8, 0x4F54: 0x24425BE8, 0xFB84: 0x24635BE8}
    for at, expected in guards.items():
        if read32(source, at) != expected:
            raise ValueError(f"Native arena selector identity changed at {at:X}")
    new_table = len(data)
    data.extend(source[table:table+count*stride])
    data.extend(bytes(stride))
    # Preserve all original records and relocate every pointer in the copy.
    for entry, symbol in list(records):
        if table <= (entry & ~3) < table+count*stride:
            records.append((entry-table+new_table, symbol))
    new_record = new_table+count*stride
    for field, text in ((0,"Super Dreamland 64"),(4,"Super Dreamland 64"),
                        (12,"Levels\\N64\\DreamLnd.exp")):
        put32(data,new_record+field,len(data))
        records.append((new_record+field,None))
        data.extend(text.encode("ascii")+b"\0")
    struct.pack_into("<HH",data,new_record+8,84,222)
    while len(data)&3:data.append(0)
    # Three native consumers: both preview records and the accepted launch path.
    highs={0x47C1,0x4F51,0xFB7D}
    if {e for e,s in records if s==table} != highs:
        raise ValueError("Native location-table relocation inventory changed")
    records=[(e,new_table if e in highs else s) for e,s in records]
    for at in (0x47C4,0x4F54,0xFB84):
        put32(data,at,(read32(data,at)&0xFFFF0000)|(new_table&0xFFFF))
    # The verified original disc supplies the retail disc-swap entitlement.
    # Both paths expose its ten locations plus the eight sequel locations.
    put32(data,0x45A4,0x24130013)
    put32(data,0x45DC,0x24130013)
    put32(data,0x4650,0x8EE40050) # Background follows all 19 preview entries.
    # Same centered subtitle backing expansion as V82ArenaRegistry.
    width=struct.unpack_from("<H",data,0x15DC4)[0]
    x,_,old_width=struct.unpack_from("<3H",data,0x15DC8)
    struct.pack_into("<H",data,0x15DC8,x-(width-old_width)//2)
    struct.pack_into("<H",data,0x15DCC,width)
    from vehicle_menu import patch
    vehicle_seams = patch(data, records)
    put32(data,0,len(data))
    for entry,symbol in records:
        data.extend(struct.pack("<I",entry))
        if symbol is not None:data.extend(struct.pack("<I",symbol))
    data.extend(struct.pack("<I",0xFFFFFFFF))
    return bytes(data),dict(retail_count=count,total_count=count+1,table=new_table,
                            added_record=new_record,source_table=table,vehicle_seams=vehicle_seams)
