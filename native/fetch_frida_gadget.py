#!/usr/bin/env python3
"""Fetch and verify the pinned official Frida Gadget runtime for the ARM64 patch."""
from __future__ import annotations
import hashlib, json, lzma, os, pathlib, shutil, struct, tempfile, urllib.request

VERSION = "17.24.2"
ASSET_NAME = f"frida-gadget-{VERSION}-android-arm64.so.xz"
ASSET_URL = f"https://github.com/frida/frida/releases/download/{VERSION}/{ASSET_NAME}"
ROOT = pathlib.Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "patches/src/main/resources/pgo/arm64-v8a/libfrida-gadget.so"
BUILD = ROOT / "build/frida-gadget"
USER_AGENT = "belalsalah74-rpc-patch-build"

def request(url: str, *, api: bool = False):
    headers = {"User-Agent": USER_AGENT}
    token = os.environ.get("GITHUB_TOKEN")
    if token and api:
        headers["Authorization"] = f"Bearer {token}"
        headers["Accept"] = "application/vnd.github+json"
    return urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=90)

def main() -> None:
    BUILD.mkdir(parents=True, exist_ok=True)
    with request(f"https://api.github.com/repos/frida/frida/releases/tags/{VERSION}", api=True) as response:
        release = json.load(response)
    asset = next((x for x in release.get("assets", []) if x.get("name") == ASSET_NAME), None)
    if not asset or asset.get("browser_download_url") != ASSET_URL:
        raise SystemExit(f"Official release asset not found: {ASSET_NAME}")
    digest = asset.get("digest", "")
    if not isinstance(digest, str) or not digest.startswith("sha256:"):
        raise SystemExit("GitHub release metadata did not provide a SHA-256 asset digest")
    expected = digest.split(":", 1)[1].lower()

    fd, archive_name = tempfile.mkstemp(prefix="gadget-", suffix=".xz.partial", dir=BUILD); os.close(fd)
    fd, output_name = tempfile.mkstemp(prefix="gadget-", suffix=".so.partial", dir=BUILD); os.close(fd)
    archive, unpacked = pathlib.Path(archive_name), pathlib.Path(output_name)
    try:
        hasher = hashlib.sha256()
        with request(ASSET_URL) as source, archive.open("wb") as target:
            while True:
                chunk = source.read(1024 * 1024)
                if not chunk: break
                hasher.update(chunk); target.write(chunk)
        if hasher.hexdigest().lower() != expected:
            raise SystemExit("Frida Gadget compressed-asset SHA-256 mismatch")
        with lzma.open(archive, "rb") as source, unpacked.open("wb") as target:
            shutil.copyfileobj(source, target, length=1024 * 1024)
        if unpacked.stat().st_size < 1024 * 1024:
            raise SystemExit("Frida Gadget payload is unexpectedly small")
        with unpacked.open("rb") as stream: header = stream.read(20)
        if len(header) < 20 or header[:4] != b"\x7fELF" or header[4] != 2 or header[5] != 1:
            raise SystemExit("Frida Gadget is not a little-endian ELF64 shared library")
        machine = struct.unpack("<H", header[18:20])[0]
        if machine != 183: raise SystemExit(f"Expected AArch64 ELF machine 183; received {machine}")
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        os.replace(unpacked, OUTPUT); OUTPUT.chmod(0o644)
        print(f"Verified Frida Gadget {VERSION} ARM64: {OUTPUT} ({OUTPUT.stat().st_size} bytes)")
    finally:
        archive.unlink(missing_ok=True); unpacked.unlink(missing_ok=True)

if __name__ == "__main__":
    main()
