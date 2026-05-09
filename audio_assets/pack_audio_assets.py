#!/usr/bin/env python3
"""Create an ordered WT2003 asset folder from manifest.json."""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path


def load_manifest(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def validate_entry(entry: dict) -> None:
    name = entry["name"]
    filename = entry["filename"]
    if len(name) > 8:
        raise ValueError(f"{filename}: WT2003 filename stem must be <= 8 characters")
    if Path(filename).stem != name:
        raise ValueError(f"{filename}: manifest name must match filename stem")


def pack(manifest_path: Path, source_dir: Path, output_dir: Path) -> None:
    manifest = load_manifest(manifest_path)
    files = sorted(manifest["files"], key=lambda item: int(item["index"]))

    output_dir.mkdir(parents=True, exist_ok=True)
    for entry in files:
        validate_entry(entry)
        src = source_dir / entry["filename"]
        dst = output_dir / entry["filename"]
        if not src.exists():
            raise FileNotFoundError(src)
        shutil.copy2(src, dst)
        print(f"{entry['index']:04d} {entry['filename']} -> {dst}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=Path(__file__).with_name("manifest.json"))
    parser.add_argument("--source", type=Path, default=Path(__file__).with_name("source"))
    parser.add_argument("--output", type=Path, default=Path(__file__).with_name("build") / "wt2003_copy")
    args = parser.parse_args()

    pack(args.manifest, args.source, args.output)
    print("Copy the output folder contents to the WT2003 USB drive, then disconnect USB before UART playback.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
