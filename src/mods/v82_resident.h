#pragma once
/* Resident Loading for SLUS-00868 (v82.enhancement.resident-loading).
 *
 * The game reads every data file through its own stream API in the main
 * executable. This adapter completes those calls from a verified resident
 * pack (mod_resident.h) instead of the emulated drive, while the game's code
 * still parses, relocates and uploads every byte. Only real-disc extents of
 * catalogued, stock-verified files are served; virtual files of other plugins
 * (Dreamland, vehicle package, presentation) and anything unrecognised take
 * the original loader. STR movies, XA speech and CD audio never pass here.
 *
 * Hooked entries (function-entry plugins; first words guarded by a hash of
 * the whole loader text 80017F50..80018708):
 *   80017F50 WholeRead(dest, lba, sectors)   blocking CdRead of whole sectors
 *   80018148 Begin(lba)                      start a ReadN sector stream
 *   800181E0 End()                           pause the drive, drop callback
 *   80018478 Read(dest, length)              copy from the stream
 *   800185F0 ReadByte()                      one byte from the stream
 *   80018644 Seek(offset, whence)            reposition the stream
 * Stream state the game owns (gp-relative): D58 current sector buffer, D5C/D60
 * consumed/filled ring slots (two 2 KiB slots at 800B6170), D64 byte position,
 * D68 next LBA to fetch (0 = idle), D6C base LBA set by Open, 83A CD state.
 *
 * A served stream is identified by a record in snapshot-backed enhancement
 * memory (Expansion 1), so a state saved anywhere around a served load
 * restores with the same loader path. */
#include <stdint.h>

namespace v82_resident {

constexpr uint32_t WholeRead = 0x80017F50u, Begin = 0x80018148u, End = 0x800181E0u;
constexpr uint32_t Read = 0x80018478u, ReadByte = 0x800185F0u, Seek = 0x80018644u;
constexpr uint32_t OpenBeginReturn = 0x8001842Cu; /* Open 800183F4 calls Begin */
constexpr uint32_t Ring = 0x800B6170u;
constexpr uint32_t GpSector = 0xD58u, GpConsumed = 0xD5Cu, GpFilled = 0xD60u;
constexpr uint32_t GpPosition = 0xD64u, GpNextLba = 0xD68u, GpBase = 0xD6Cu, GpCdState = 0x83Au;
constexpr uint32_t RecordMagic = 0x56385253u; /* "SR8V" */
constexpr uint32_t RecordBytes = 32u;
/* Record words: +0 magic, +4 pack file index, +8 base LBA, +12 gp,
 * +16 LBA held in ring slot 0 (0 = none), +20 bytes served by this stream. */

}  // namespace v82_resident
