# Resident Loading

`v82.enhancement.resident-loading` is enabled by default for SLUS-00868.
Disable **Resident Loading** in Mods to restore the original loading path.
Guest CD timing, movies (STR), speech (XA) and CD music are unchanged.

## Preparation

On first launch the shared psxrecomp kit (`mod_resident.h`) reads the 82
data files the game's own loader uses from the mounted, mod-effective disc:
`LEVELS/*.EXP|DLL`, `LEVELS/V8/*`, `SHARED/*`, `SHELL/*` and `QUEST.BIN`
(20,267,008 sector bytes). Each file is hashed over its complete sectors and
compared with the generated catalog (`src/mods/v82_resident_catalog.inc`,
metadata only). The pack is cached under `LOCALAPPDATA/Vigilante82PSXRecomp/
seamless`, keyed by the resolved mod-plan fingerprint; an unwritable cache
keeps the pack in memory, so the loader path never depends on the host file
system. Changed or unknown files stay on the original loader per request.

Regenerate the catalog from an owned disc:

```sh
py -3 tools/v82_resident_catalog.py --disc "<SLUS-00868>.cue" --output src/mods/v82_resident_catalog.inc
```

## Guest contract

The adapter hooks the game's stream API in the main executable and guards it
with a SHA-256 of the loader text 80017F50..80018708 plus each entry word.

| Entry | Served behaviour | Kept original |
| --- | --- | --- |
| `80018148` Begin, called by Open only | stream state set, no drive commands | Seek restarts, directory walker |
| `80018478` Read / `800185F0` ReadByte | bytes from the resident file; current sector placed in ring slot 800B6170 (D58) exactly as Wait leaves it | reads past the file: drive restarted at the next sector, original body runs |
| `80018644` Seek | position and sector buffer updated | as above |
| `800181E0` End | stream closed | CD streams |
| `80017F50` WholeRead | completed CD DMA of whole stock sectors | other extents |

Virtual files of Dreamland, the vehicle package and the presentation plugin
use LBAs outside the disc and are never matched, so those plugins' hooks win
whatever the plan order. The served-stream record lives in snapshot-backed
Expansion 1 memory; every hook is atomic, so a state saved at any point
restores on the same path. In netplay the feature is part of the published
mod plan.

## Observability

`{"cmd":"resident_status"}` reports the pack; `{"cmd":"resident_events"}`
returns the always-on service ring (every Begin/End/WholeRead, served or not,
with guest frame and cycle). `mod_counters` carries `v82.resident.*`.
Debug builds accept `PSX_V82_RESIDENT_OBSERVE=1`: record loader calls, never
serve (a timing-identical "off" arm with the same event windows).

## Validation

`v82-resident` compares the adapter with a transliteration of the original
loader over 400 randomized Read/ReadByte/Seek sequences (bytes, position,
sector buffer), snapshot restore, fallback, identity and code-contract
declines, and WholeRead bounds. `v82-resident-catalog` checks the catalog
against `V82_DISC` when set.
