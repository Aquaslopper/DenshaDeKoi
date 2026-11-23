#!/usr/bin/env python3
"""Regenerate src/EmbeddedAssets.hpp from the checked-in asset files.

Run after changing assets/fonts/*.ttf or assets/densha-logo.png:

    python3 tools/embed_assets.py [repo-root]

The generated file is committed on purpose: it keeps the build independent
of any particular build system or asset-copy step, so the pause menu always
renders with the correct fonts and wordmark.
"""
import sys
from pathlib import Path

FILES = [
    ("assets/fonts/Silkscreen-Bold.ttf", "kSilkscreenBoldTtf"),
    ("assets/fonts/VT323.ttf", "kVt323Ttf"),
    ("assets/densha-logo.png", "kDenshaLogoPng"),
]

def emit(root: Path) -> str:
    out = []
    out.append("""// Embedded pause-menu assets (fonts + wordmark) so the game renders the
// pause menu correctly regardless of the deployment folder. Generated from
// assets/fonts/*.ttf and assets/densha-logo.png -- see assets/fonts/README.md
// for regeneration and licenses (SIL OFL 1.1). Do not edit by hand.
#pragma once

namespace EmbeddedAssets {
""")
    for rel, sym in FILES:
        data = (root / rel).read_bytes()
        out.append(f"// {Path(rel).name} ({len(data)} bytes)\n")
        out.append(f"inline const unsigned char {sym}[] = {{\n")
        for i in range(0, len(data), 16):
            chunk = ", ".join(f"0x{b:02x}" for b in data[i:i + 16])
            out.append("    " + chunk + ",\n")
        out.append("};\n")
        out.append(f"inline const unsigned int {sym}_Size = {len(data)};\n\n")
    out.append("} // namespace EmbeddedAssets\n")
    return "".join(out)

def main() -> int:
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent
    target = root / "src/EmbeddedAssets.hpp"
    target.write_text(emit(root))
    print(f"wrote {target}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
