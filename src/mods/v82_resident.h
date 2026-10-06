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
 * restores with the same loader path.
 *
 * Load floor. With the disc out of the way, an arena load is dominated by
 * work the game does in guest time (beads-eio.12.17 profile, 301 frames):
 *   A  80021C24 waits for the loading-screen wipe (a VSync callback that
 *      advances 2 per frame to 256) after the loader is done. At the arena
 *      load's call (ra 80013798) a running wipe is marked finished first.
 *   B  8001E914 (per-object setup, called by the EXP chunk loader 80032F40
 *      at 8003305C) calls VSync(0) five times and ignores the result. Those
 *      five calls (ra 8001E954..8001E974, caller ra 80033064 at sp+28) return
 *      at once.
 *   C  The VRAM rectangle allocator 80020A80 walks its free tree. Called by
 *      the EXP texture loader 8001F3C0 (ra 8001F490 / 8001F5C8), it runs
 *      unchanged through psx_mod_call_guest_uncharged: same RAM result, no
 *      guest time.
 *   D  The texture LZSS 80021064, called by 8001F3C0 (ra 8001F5F4), decodes
 *      2 KiB windows to VRAM. Every EXP texture is pre-decoded into the pack
 *      (v82_lzss.h, an exact model of the guest decoder); when the stream in
 *      guest RAM equals the pack's bytes, the game's GPU queue is drained
 *      (DrawSync) and the rows go to VRAM with psx_mod_psyq_load_image.
 *   E  The sound-bank loader 8001DEA8 uploads 1 KiB chunks with SpuWrite
 *      (ra 8001DFE8) and polls SpuIsTransferCompleted. A DMA-mode chunk is
 *      completed synchronously (psx_mod_spu_upload of the DMA's whole
 *      16-word blocks) with libspu's own bookkeeping, so the poll succeeds.
 * Each shim runs only from those load-only call sites, only while the
 * resident pack is active and the code contract (loader plus every function
 * above) matches, and otherwise leaves the original path. Every decision
 * depends on guest state and the pack alone, so netplay peers with the same
 * plan agree. Debug builds: PSX_V82_LOAD_FLOOR=0 keeps resident serving and
 * turns the five shims off (A/B comparison). */
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

/* Load floor (see above). */
constexpr uint32_t WipeWait = 0x80021C24u, ArenaWipeWaitReturn = 0x80013798u;
constexpr uint32_t GpWipeState = 0xE98u;  /* 0 none, 1 running, 2 finished */
constexpr uint32_t VSync = 0x80054C4Cu, ObjectSetupFrameRa = 28u;
constexpr uint32_t ObjectSetupVSyncReturns[] = {0x8001E954u, 0x8001E95Cu, 0x8001E964u,
                                                0x8001E96Cu, 0x8001E974u};
constexpr uint32_t ExpChunkObjectSetupReturn = 0x80033064u;
constexpr uint32_t RectAlloc = 0x80020A80u;
constexpr uint32_t TextureClutAllocReturn = 0x8001F490u, TextureImageAllocReturn = 0x8001F5C8u;
constexpr uint32_t AllocBudgetCycles = 32u * 1024u * 1024u;
constexpr uint32_t Lzss = 0x80021064u, TextureDecodeReturn = 0x8001F5F4u;
constexpr uint32_t DrawSync = 0x8005BF88u;
constexpr uint32_t LzssFrame = 0xC70u;     /* the decoder's frame, below sp */
constexpr uint32_t TextureTag = 0x5A4C3854u; /* "T8LZ": meta w|h<<16, offset, consumed */
constexpr uint32_t SpuWrite = 0x800535ECu, BankChunkWriteReturn = 0x8001DFE8u;
/* libspu state (SpuWrite 800535EC, _spu_t 80052970, SpuIsTransferCompleted
 * 800536DC): */
constexpr uint32_t SpuIoMode = 0x8006419Cu;      /* 0 = DMA transfers */
constexpr uint32_t SpuTransferAddr = 0x80064198u; /* u16, address >> shift */
constexpr uint32_t SpuAddrShift = 0x800641A8u;
constexpr uint32_t SpuDone = 0x800641B4u, SpuCallback = 0x800641B8u;
constexpr uint32_t SpuTMode = 0x800641D0u, SpuDmaSource = 0x800641D4u, SpuDmaBlocks = 0x800641D8u;
constexpr uint32_t SpuDelayPtr = 0x80064194u;     /* -> 1F801014 */
constexpr uint32_t SpuWriteMax = 0x7EFF0u;

}  // namespace v82_resident
