# Licensing and media

Original project source is licensed under the accompanying PolyForm
Noncommercial 1.0.0 license, copyright 2026 Matthew Stanley. Framework and UI
dependencies retain their own licenses.

Adapted and copied upstream portions retain their original notices and terms.
The detailed attribution and permission record is in
[tools/third_party/NOTICE.md](tools/third_party/NOTICE.md). RecompOne-derived
portions retain its MIT notice. The copied reference Python converters have
no blanket upstream license; the owner's reported author permission allows
their reuse here. The project license does not replace those upstream terms.

Release packages include the redistributable PCSX-Redux OpenBIOS and its MIT
notice. Players supply all original game discs and optional donor ROMs. No
retail BIOS, disc image, converted car/music/arena assets or personal saves are
included. Bundled native overlay caches contain compiled code, not captures
or media archives.

The launcher uses the USA PlayStation cover archived by
[Libretro Thumbnails](https://github.com/libretro-thumbnails/Sony_-_PlayStation/blob/25c6d021383b6dd4f7c74bad24845b15f6fb7d04/Named_Boxarts/Vigilante%208%20-%202nd%20Offense%20%28USA%29.png).
The build downloads a pinned, SHA-256-verified image into its build directory;
the source repository does not contain the artwork. Cover artwork and game
trademarks retain their owners' rights and are not covered by the source license.
Offline builds can set `V82_LAUNCHER_BOXART` to an existing cover image.
