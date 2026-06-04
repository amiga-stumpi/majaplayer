#!/usr/bin/env python3
"""Generate Amiga-friendly static API files for the MajaRadio MOD server."""

from __future__ import annotations

import argparse
import os
import random
import re
from pathlib import Path
from urllib.parse import quote

DOMAIN = "mods.c64.social"
DEFAULT_MODS_DIR = Path("mods")
API_DIR = Path("api")
LIST_FILE = API_DIR / "list.txt"
RANDOM_FILE = API_DIR / "random.txt"

SAFE_ID_RE = re.compile(r"[^a-zA-Z0-9_]+")
MOD_EXT_RE = re.compile(r"\.mod$", re.IGNORECASE)
MOD_SIGNATURES = {
    b"M.K.", b"M!K!", b"M&K!", b"N.T.", b"FLT4", b"4CHN", b"4FLT",
}


def sanitize_id(relative_path: Path) -> str:
    parts = [part for part in relative_path.with_suffix("").parts if part not in (".", "")]
    stem = "_".join(parts).strip().replace(" ", "_")
    stem = SAFE_ID_RE.sub("_", stem)
    stem = stem.strip("_").lower()
    return stem or "module"


def title_from_name(path: Path) -> str:
    return path.stem.replace("_", " ").replace("-", " ").strip() or path.stem


def url_path_for(relative_path: Path) -> str:
    encoded_parts = [quote(part, safe="") for part in relative_path.parts]
    return "/mods/" + "/".join(encoded_parts)


def is_probably_mod(path: Path) -> bool:
    try:
        with path.open("rb") as fh:
            data = fh.read(1084)
    except OSError:
        return False
    if len(data) < 1084:
        return False
    signature = data[1080:1084]
    return signature in MOD_SIGNATURES


def collect_modules(mods_dir: Path, max_size: int | None, validate: bool) -> list[dict[str, object]]:
    modules: list[dict[str, object]] = []
    seen_ids: dict[str, int] = {}
    if not mods_dir.exists():
        return modules
    for path in sorted(mods_dir.rglob("*")):
        if not path.is_file() or not MOD_EXT_RE.search(path.name):
            continue
        size = path.stat().st_size
        if max_size is not None and size > max_size:
            continue
        if validate and not is_probably_mod(path):
            continue
        relative_path = path.relative_to(mods_dir)
        mod_id = sanitize_id(relative_path)
        count = seen_ids.get(mod_id, 0)
        seen_ids[mod_id] = count + 1
        if count:
            mod_id = f"{mod_id}_{count + 1}"
        rel_path = url_path_for(relative_path)
        modules.append({
            "id": mod_id,
            "title": title_from_name(path),
            "size": size,
            "path": rel_path,
            "url": f"http://{DOMAIN}{rel_path}",
            "source": str(relative_path),
        })
    return modules


def write_list(modules: list[dict[str, object]]) -> None:
    API_DIR.mkdir(parents=True, exist_ok=True)
    with LIST_FILE.open("w", encoding="ascii", errors="replace", newline="\n") as fh:
        fh.write("# ID|SIZE|PATH|TITLE\n")
        for mod in modules:
            title = str(mod["title"]).replace("|", " ")
            fh.write(f"{mod['id']}|{mod['size']}|{mod['path']}|{title}\n")


def write_random(modules: list[dict[str, object]]) -> None:
    API_DIR.mkdir(parents=True, exist_ok=True)
    with RANDOM_FILE.open("w", encoding="ascii", errors="replace", newline="\n") as fh:
        if not modules:
            fh.write("ERROR\nMESSAGE=No MOD files indexed yet\n")
            return
        mod = random.choice(modules)
        fh.write("OK\n")
        fh.write(f"ID={mod['id']}\n")
        fh.write(f"TITLE={mod['title']}\n")
        fh.write(f"SIZE={mod['size']}\n")
        fh.write(f"PATH={mod['path']}\n")
        fh.write(f"URL={mod['url']}\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate static MOD server API files")
    parser.add_argument("--mods-dir", default=str(DEFAULT_MODS_DIR),
                        help="directory containing MOD files; scanned recursively")
    parser.add_argument("--max-size", type=int, default=None,
                        help="skip MOD files larger than this many bytes")
    parser.add_argument("--no-validate", action="store_true",
                        help="do not check ProTracker-style MOD signatures")
    args = parser.parse_args()

    os.chdir(Path(__file__).resolve().parents[1])
    modules = collect_modules(Path(args.mods_dir), args.max_size, not args.no_validate)
    write_list(modules)
    write_random(modules)
    print(f"Indexed {len(modules)} MOD file(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
