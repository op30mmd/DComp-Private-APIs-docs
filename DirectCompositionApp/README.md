# DirectComposition Private APIs Demo

A demonstration application showcasing Windows DirectComposition private APIs based on reverse engineering analysis of `dcomp.dll`.

## Overview

This application demonstrates the use of DirectComposition APIs documented in `DirectComposition_Private_APIs.md`, including:

- **DCompositionCreateDevice3** - Creates an IDCompositionDevice3 interface
- **IDCompositionDesktopDevice** - Private interface for HWND-based targets (GUID: `5F4633FE-1E08-4CB8-8C75-CE24333F5602`)
- **DCompositionGetFrameId** - Queries the current composition frame ID
- **DCompositionGetStatistics** - Retrieves per-frame composition statistics
- **DCompositionBoostCompositorClock** - Boosts compositor clock frequency
- **DCompositionWaitForCompositorClock** - Waits for compositor clock tick
- **CreatePresentationFactory** - Private undocumented presentation factory creation

## Architecture

```
DirectCompositionApp/
├── main.cpp              # Window creation and message loop
├── DCompHelper.h/cpp     # Core DirectComposition device management
├── PrivateApiDemo.h/cpp  # Private API wrapper functions
└── CMakeLists.txt        # Build configuration
```

## Building

### Prerequisites
- Windows 10/11 SDK
- Visual Studio 2019+ or CMake 3.20+
- C++17 compatible compiler

### Build with CMake
```bash
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

### Build with Visual Studio
1. Open `CMakeLists.txt` in Visual Studio
2. Select configuration (Debug/Release)
3. Build the solution

## Features

### Visual Composition
- Creates a visual tree with 5 colored rectangles
- Uses `IDCompositionVisual` for each element
- Applies `IDCompositionRectangleClip` for visual clipping

### Frame Statistics
- Real-time frame ID display
- Composition rate monitoring
- Frame timing statistics

### Private API Usage
- Demonstrates undocumented API functions
- Shows proper error handling patterns
- Uses NT kernel syscall wrappers

## API Reference

### DCompositionCreateDevice3
```cpp
HRESULT DCompositionCreateDevice3(
    IDXGIDevice* dxgiDevice,
    REFIID riid,
    void** ppvDevice
);
```

### IDCompositionDesktopDevice (Private Interface)
```cpp
// GUID: {5F4633FE-1E08-4CB8-8C75-CE24333F5602}
// Extends IDCompositionDevice2
STDMETHOD(CreateTargetForHwnd)(
    HWND hwnd,
    BOOL topmost,
    IDCompositionTarget** target
) PURE;
```

### DCompositionGetStatistics
```cpp
HRESULT DCompositionGetStatistics(
    COMPOSITION_FRAME_ID frameId,
    COMPOSITION_FRAME_STATS* frameStats,
    UINT targetIdCount,
    COMPOSITION_TARGET_ID* targetIds,
    UINT* actualTargetIdCount
);
```

### DCompositionBoostCompositorClock
```cpp
HRESULT DCompositionBoostCompositorClock(BOOL boost);
```

### CreatePresentationFactory (Undocumented)
```cpp
HRESULT CreatePresentationFactory(
    IUnknown* punkDevice,
    REFIID riid,  // {8FB37B58-1D74-4F64-A49C-1F97A80A2EC0}
    void** ppvFactory
);
```

## Notes

- The application requires a compatible GPU with D3D11 support
- Private APIs may change between Windows versions
- Use responsibly - these are undocumented interfaces

## References

- [DirectComposition_Private_APIs.md](../DirectComposition_Private_APIs.md) - Complete API documentation
- [Microsoft DirectComposition Docs](https://learn.microsoft.com/en-us/windows/win32/directcomposition/directcomposition-portal)
- [IDCompositionDevice3 Interface](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nn-dcomp-idcompositiondevice3)
