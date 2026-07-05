# DirectComposition Text Editor

A Notepad-style text editor built entirely on DirectComposition, D2D, and DWrite — no GDI, no Win32 controls. Renders all UI (menu bar, status bar, line numbers, text, caret, syntax highlighting) via DirectComposition swap chains. Uses reverse-engineered private `dcomp.dll` APIs for frame statistics and presentation control.

## Overview

The editor demonstrates a complete rendering pipeline built on top of DirectComposition private APIs:

- **DCompositionCreateDevice3** with v1→v2→v3 device chain
- **IDCompositionDesktopDevice** for HWND binding
- Private frame statistics APIs (`DCompositionGetFrameId`, `DCompositionGetStatistics`)
- **CreatePresentationFactory** undocumented API
- **D2D1** + **DWrite** for all text rendering
- **Direct Manipulation** for smooth scrolling
- **Tree-sitter** for syntax highlighting

## Architecture

```
DirectCompositionApp/
├── main.cpp                    # Window, message loop, rendering, input (~1700 lines)
├── DCompHelper.h/cpp           # DComp device chain, D2D/DWrite init, swap chain, resize
├── DirectManipHelper.h/cpp     # Direct Manipulation viewport, smooth scroll, VSync paint loop
├── TreeSitterHighlighter.h/cpp # Tree-sitter-cpp integration, UTF-8→UTF-16 offset mapping
├── PrivateApiDemo.h/cpp        # Frame statistics wrapper (DCompositionGetStatistics etc.)
├── PresentationFactoryDemo.h/cpp # CreatePresentationFactory demo (unused)
└── CMakeLists.txt              # Build configuration (CMake, MSVC 2022)
```

## Features

### Text Editing
- Character input, cursor navigation (arrows, Home/End, Ctrl+arrows)
- Selection (Shift+arrows, Ctrl+A, Shift+click, click-drag)
- Clipboard (Ctrl+C/X/V)
- Undo/Redo (Ctrl+Z/Ctrl+Y)
- Tab insertion, Enter, Backspace, Delete
- Open/Save files with UTF-8 support via Win32 file dialogs

### UI Chrome (All D2D-rendered)
- **Menu bar**: File, Edit, Format, View, Help — classic Notepad style with shortcuts
- **Status bar**: Character count, line/column, FPS via DComp frame statistics
- **Line numbers**: Dynamic gutter width based on digit count, 12pt font
- **Hint bar**: Context-sensitive help text at the bottom
- **Dropdown menus**: Hover tracking, keyboard shortcuts displayed, separators
- **Right-click context menu**: Cut/Copy/Paste/Select All, grayed by state

### Syntax Highlighting
- Tree-sitter-cpp with incremental parsing
- 10 capture types: keywords, types, functions, strings, numbers, comments, operators, preprocessor, parameters, field names
- UTF-8→UTF-16 offset mapping for DWrite text layout

### Smooth Scrolling
- Direct Manipulation viewport with manual update mode
- VSync-chained paint loop (InvalidateRect at end of WM_PAINT)
- Mouse wheel with delta-based scrolling
- Cursor following during text input

### Cursor Management
- Class cursor set to nullptr to avoid flickering
- I-beam for text area, hand for menu bar/dropdowns, arrow for gutter/status/hint bar
- Cursor cached to avoid redundant SetCursor calls
- HTCLIENT check for proper non-client area handling (border resize cursors)
- Drag-selection preserves I-beam outside window bounds

## Building

### Prerequisites
- Windows 11 SDK (10.0.26100.0)
- Visual Studio 2022 (MSVC 19.44)
- CMake 3.20+

### Quick Build
```bash
build.bat
```

### Manual Build
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Debug
```

### Output
`build\bin\Release\DirectCompositionApp.exe`

## Notes

- Requires Intel/AMD GPU with D3D11 and D2D support
- Private APIs may change between Windows versions
- Tree-sitter grammar libraries built as static libs via CMake
- No GDI or Win32 common controls used — everything is custom-rendered
