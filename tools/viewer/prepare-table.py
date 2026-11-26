#!/usr/bin/env python3
"""Copy the game's table renderer, omitting only unsupported GPU magnification."""
import pathlib
import sys
source = pathlib.Path(sys.argv[1]).read_text()
old = '''        magnifyShader_ = LoadShaderFromMemory(nullptr, MagnifyFragmentShader);
        if (magnifyShader_.id == 0) throw std::runtime_error("Could not create magnification shader");'''
if old not in source:
    raise SystemExit('TableScene shader initialization changed; update the viewer adapter')
source = source.replace(old, '        // Headless viewer: magnification shader is unavailable.')
source = source.replace('    BeginShaderMode(magnifyShader_);', '')
source = source.replace('    EndShaderMode();', '')
pathlib.Path(sys.argv[2]).write_text(source)
