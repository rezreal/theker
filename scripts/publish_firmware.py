#!/usr/bin/env python3
"""Install a firmware build into the flasher site and refresh versions.json.

GitHub's CORS rules stop a Pages site from fetching a release asset with
JavaScript (esphome/esp-web-tools#521), so the binary is copied into the site
and served same-origin instead of linked from the release.

versions.json is regenerated from whatever is actually on disk, so the picker
can only ever offer builds the page can really flash.

Usage:
    publish_firmware.py --site site --tag v1.0.0 \
        --firmware .pio/build/esp32r4/firmware.factory.bin \
        --repository https://github.com/owner/repo
"""

import argparse
import json
import re
import shutil
from datetime import date, datetime, timezone
from pathlib import Path

FIRMWARE_NAME = "firmware.factory.bin"
PRODUCT_NAME = "Hismith Piupiu Emulator"


def version_key(name: str):
    """Sort v1.12.0 above v1.9.0. Unparseable names sort last."""
    match = re.match(r"^v?(\d+)\.(\d+)\.(\d+)", name)
    if not match:
        return (0, (0, 0, 0), name)
    return (1, tuple(int(part) for part in match.groups()), name)


def install_build(site: Path, tag: str, firmware: Path, chip: str) -> None:
    dest = site / "firmware" / tag
    dest.mkdir(parents=True, exist_ok=True)
    shutil.copy2(firmware, dest / FIRMWARE_NAME)

    # esp-web-tools manifest. The merged factory image is a single part at 0x0.
    manifest = {
        "name": PRODUCT_NAME,
        "version": tag,
        "new_install_prompt_erase": True,
        "builds": [
            {
                "chipFamily": chip,
                "parts": [{"path": FIRMWARE_NAME, "offset": 0}],
            }
        ],
    }
    (dest / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

    # Kept beside the manifest rather than inside it, so the manifest stays
    # exactly what esp-web-tools expects.
    meta = {"version": tag, "date": date.today().isoformat()}
    (dest / "meta.json").write_text(json.dumps(meta, indent=2) + "\n")


def regenerate_versions(site: Path, repository: str) -> int:
    firmware_dir = site / "firmware"
    releases = []

    if firmware_dir.is_dir():
        candidates = [d for d in firmware_dir.iterdir() if d.is_dir()]
        for build in sorted(candidates, key=lambda d: version_key(d.name), reverse=True):
            if not (build / "manifest.json").is_file():
                continue
            if not (build / FIRMWARE_NAME).is_file():
                continue

            meta_path = build / "meta.json"
            meta = json.loads(meta_path.read_text()) if meta_path.is_file() else {}

            releases.append(
                {
                    "version": build.name,
                    "date": meta.get("date"),
                    "manifest": f"firmware/{build.name}/manifest.json",
                    "notes": f"{repository}/releases/tag/{build.name}",
                }
            )

    payload = {
        "generated": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "repository": repository,
        "releases": releases,
    }
    (site / "versions.json").write_text(json.dumps(payload, indent=2) + "\n")
    return len(releases)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site", required=True, type=Path, help="checkout of the gh-pages branch")
    parser.add_argument("--tag", help="release tag, e.g. v1.0.0")
    parser.add_argument("--firmware", type=Path, help="merged factory image to publish")
    parser.add_argument("--repository", required=True, help="https://github.com/owner/repo")
    parser.add_argument("--chip", default="ESP32")
    args = parser.parse_args()

    args.site.mkdir(parents=True, exist_ok=True)

    if bool(args.tag) != bool(args.firmware):
        parser.error("--tag and --firmware must be given together")

    if args.tag:
        if not args.firmware.is_file():
            parser.error(f"firmware not found: {args.firmware}")
        install_build(args.site, args.tag, args.firmware, args.chip)
        print(f"installed {args.tag} -> {args.site}/firmware/{args.tag}/")

    count = regenerate_versions(args.site, args.repository.rstrip("/"))
    print(f"versions.json lists {count} release(s)")


if __name__ == "__main__":
    main()
