#!/usr/bin/env python3
"""Regenerate src/EmbeddedAssets.cpp/.hpp from the checked-in asset files.

Run after changing assets/fonts/*.ttf or assets/densha-logo.png:

    python3 tools/embed_assets.py [repo-root]

The fonts and the wordmark are compiled into the game as base64 blobs (loaded
with Raylib's LoadFontFromMemory / LoadImageFromMemory), so the pause menu
renders correctly regardless of how the executable was deployed. The output
is checked in on purpose.
"""
import base64
import sys
from pathlib import Path

FILES = [
    ("assets/fonts/Silkscreen-Bold.ttf", "kSilkscreenBoldTtf"),
    ("assets/fonts/VT323.ttf", "kVt323Ttf"),
    ("assets/densha-logo.png", "kDenshaLogoPng"),
]

HEADER_PRE = """// Embedded pause-menu assets (fonts + wordmark) as base64 blobs.
// Generated from assets/fonts/*.ttf and assets/densha-logo.png by
// tools/embed_assets.py -- do not edit by hand; see assets/fonts/README.md
// for licenses (SIL OFL 1.1).
#pragma once

namespace EmbeddedAssets {
"""

def build_decl(sym: str) -> str:
    return f"extern const char {sym}[];\nextern const unsigned int {sym}Size;\n"

def build_unit(root: Path):
    header = [HEADER_PRE]
    unit = [HEADER_PRE.replace("#pragma once\n\n", "").rstrip("\n") + "\n\n"]
    for rel, sym in FILES:
        data = (root / rel).read_bytes()
        b64 = base64.b64encode(data).decode("ascii")
        header.append(build_decl(sym))
        unit.append(f"// {Path(rel).name} ({len(data)} bytes)\n")
        unit.append(f"extern const char {sym}[] =\n")
        for i in range(0, len(b64), 96):
            unit.append(f'    "{b64[i:i + 96]}"\n')
        unit.append(";\n")
        unit.append(f"extern const unsigned int {sym}Size = {len(data)};\n\n")
    header.append("} // namespace EmbeddedAssets\n")
    unit.append("} // namespace EmbeddedAssets\n")
    return "".join(header), "".join(unit)

def main() -> int:
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent
    decl, unit = build_unit(root)
    (root / "src/EmbeddedAssets.hpp").write_text(decl)
    (root / "src/EmbeddedAssets.cpp").write_text(unit)
    # sanity check: decoded blobs must match the source files byte-for-byte
    import re
    for rel, sym in FILES:
        raw = (root / rel).read_bytes()
        blob = re.search(rf'const char {sym}\[\] =\n((?:    "[^"]*"\n)+);', unit).group(1)
        decoded = base64.b64decode("".join(re.findall(r'"([^"]*)"', blob)))
        assert decoded == raw, f"roundtrip failed for {rel}"
    print("wrote src/EmbeddedAssets.{hpp,cpp} (base64, roundtrip verified)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
