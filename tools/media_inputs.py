"""Owner-media identities and bounded extraction shared by preparation tools."""
from pathlib import Path
from v8_media import PsxVolume

IDENTITIES = {
    "v8": (223959040, "1133d14a882bf65c061aa24d7e81f875368dbb8f5cd33abdc5cbd5b26f38ffbf"),
    "v82": (301703168, "12777aecb48f08d5f0a20b71ae8df4d64a8b5d5d4832664009586eaf99e58201"),
}


def external_output(path: Path) -> Path:
    path = path.resolve()
    if any((p / ".git").exists() for p in (path, *path.parents)):
        raise ValueError("Game media and converted assets must stay outside Git")
    return path


def verified_volume(path: Path, game: str):
    volume = PsxVolume(path)
    size, digest = IDENTITIES[game]
    if volume.sectors * 2048 != size or volume.digest() != digest:
        raise ValueError(f"Wrong {game} disc revision")
    return volume, {entry["name"]: entry for entry in volume.files()}


def read_file(volume, entries, name: str) -> bytes:
    entry = entries[name]
    return volume.read(entry["lba"], entry["size"])
