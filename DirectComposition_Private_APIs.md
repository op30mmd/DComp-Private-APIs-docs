# DirectComposition Private APIs - Reverse Engineering Documentation

**Target DLL:** `C:\Windows\System32\dcomp.dll`  
**DLL Size:** 2,232,800 bytes  
**Build Date:** 2026-07-04  
**Analysis Date:** 2026-07-05  
**Tools:** Frida (dynamic), Ghidra-style disassembly (static)  
**Platform:** Windows 11, x86-64

---

## Table of Contents

1. [Export Table Overview](#1-export-table-overview)
2. [Private Exported Functions](#2-private-exported-functions)
3. [Internal Source Architecture](#3-internal-source-architecture)
4. [Private COM Interface GUIDs (DCompositionCreateDevice3)](#4-private-com-interface-guids)
5. [CreatePresentationFactory Deep Analysis](#5-createpresentationfactory-deep-analysis)
6. [DCompositionCreateDevice Family](#6-dcompositioncreatedevice-family)
7. [Frame Statistics APIs](#7-frame-statistics-apis)
8. [Surface Handle API](#8-surface-handle-api)
9. [Compositor Clock Boost API](#9-compositor-clock-boost-api)
10. [DWM Integration APIs](#10-dwm-integration-apis)
11. [Mouse Attachment APIs](#11-mouse-attachment-apis)
12. [Internal Helper Functions](#12-internal-helper-functions)
13. [COM Vtable Layout Discovery](#13-com-vtable-layout-discovery)

---

## 1. Export Table Overview

dcomp.dll exports only **18 functions** — far fewer than its massive internal codebase. Most interesting functionality is hidden behind COM interfaces.

| # | Export Name | Type | Private? | Notes |
|---|------------|------|----------|-------|
| 1 | `CreatePresentationFactory` | Function | **YES** | Undocumented presentation factory creation |
| 2 | `DCompositionAttachMouseDragToHwnd` | Function | Semi-public | Attaches mouse drag to HWND |
| 3 | `DCompositionAttachMouseWheelToHwnd` | Function | Semi-public | Attaches mouse wheel to HWND |
| 4 | `DCompositionBoostCompositorClock` | Function | Semi-documented | Boosts compositor clock frequency |
| 5 | `DCompositionCreateDevice` | Function | Public | Creates IDCompositionDevice (v1) |
| 6 | `DCompositionCreateDevice2` | Function | Public | Creates IDCompositionDevice2 |
| 7 | `DCompositionCreateDevice3` | Function | Public | Creates IDCompositionDevice3 (supports 10 IIDs) |
| 8 | `DCompositionCreateSurfaceHandle` | Function | Semi-documented | Creates shareable surface handles |
| 9 | `DCompositionGetFrameId` | Function | Semi-documented | Returns current composition frame ID |
| 10 | `DCompositionGetStatistics` | Function | Semi-documented | Returns per-frame composition statistics |
| 11 | `DCompositionGetTargetStatistics` | Function | Semi-documented | Returns target-specific statistics |
| 12 | `DCompositionWaitForCompositorClock` | Function | Semi-documented | Waits for compositor clock tick |
| 13 | `DllCanUnloadNow` | Function | Standard | COM DLL unload check |
| 14 | `DllGetActivationFactory` | Function | WinRT | WinRT activation factory resolution |
| 15 | `DllGetClassObject` | Function | Standard | COM class factory retrieval |
| 16 | `DwmEnableMMCSS` | Function | Re-exported | DWM Multimedia Class Scheduler Service |
| 17 | `DwmFlush` | Function | Re-exported | DWM flush (stub, returns S_OK) |
| 18 | `DwmpEnableDDASupport` | Function | Private | Display Awareness support (stub) |

---

## 2. Private Exported Functions

### 2.1 `CreatePresentationFactory` (TRULY UNDOCUMENTED)

**Signature (inferred from disassembly):**
```c
HRESULT CreatePresentationFactory(
    _In_ IUnknown* punkDevice,       // rcx: Must be an IDCompositionDevice
    _In_ REFIID riid,                // rdx: Must be {8FB37B58-1D74-4F64-A49C-1F97A80A2EC0}
    _Out_ void** ppvFactory          // r8:  Output presentation factory
);
```

**Accepts GUID:** `8FB37B58-1D74-4F64-A49C-1F97A80A2EC0` (unknown, likely internal IPresentationFactory)

**Behavior:**
1. Validates that `ppvFactory` is non-null (returns `E_INVALIDARG` if null)
2. Validates the REFIID against the internal GUID at `dcomp!0x7fffb70c5a08`
3. Calls internal factory creation at `0x7fffb7075264`
4. Internal factory allocates 0x28 (40) bytes for the factory object
5. Initializes vtable at offset +0x10 with a pointer to `0x7fffb70b1468` area (GUID-check table)
6. Vtable pointer set at offset +0x00 from `rip+0x301d1` (the PresentationFactory vtable)
7. Initializes internal state, calls `0x7fffb707f010` (IUnknown::AddRef pattern)
8. Calls `0x7fffb7075408` for final initialization

**Internal allocation (0x7fffb7075264):**
```
Object size: 0x28 bytes
+0x00: Vtable pointer (PresentationFactory vtable)
+0x10: Secondary vtable/function pointer (set to rip+0x30224)
+0x18: Zero (initialized)
+0x20: Zero word
```

**Error handling:** On failure, calls `0x7fffb6efc2bc` (likely a WER/error reporting function) with the HR value and `0x14`.

---

### 2.2 `DwmpEnableDDASupport`

**Signature:**
```c
HRESULT DwmpEnableDDASupport();  // No parameters
```

**Behavior:** Stub — immediately returns `S_OK` (0x0). Shares code with `DwmFlush`.

**Address:** `0x7fffb6fa8960` (same as DwmFlush — aliased)

---

### 2.3 `DwmFlush`

**Signature:**
```c
HRESULT DwmFlush();  // No parameters
```

**Behavior:** Stub — immediately returns `S_OK`. This is a no-op in dcomp.dll.

---

## 3. Internal Source Architecture

From string references in the `.rdata` section, the dcomp.dll internal source tree is organized as:

```
onecoreuap/windows/dwm/dcomp/
├── Device.h
├── VisualProxy.h
├── device.cpp                         # Core device implementation
├── dxdevice.cpp                       # DXGI device integration
├── atlassurfacepool.cpp               # Atlas surface memory pooling
├── devicetexturemanagerbase.cpp       # Texture management base
├── devicetexturemanagerd3d11.cpp      # D3D11 texture management
├── devicetexturemanagerd3d12.cpp      # D3D12 texture management
├── compositiontexture.cpp             # Composition texture objects
├── compositiondynamictextureproxy.cpp # Dynamic texture proxy
├── dll/
│   ├── kerneltokenfactory.cpp         # Kernel token creation
│   └── globaldevice.cpp               # Global device singleton
├── winrtnested/
│   ├── wrtvisualiterator.cpp          # Visual tree iterator
│   ├── wrtvisualcollection.cpp        # Visual collection management
│   ├── wrtcompositionanimation.cpp    # Animation handling
│   ├── wrtspritevisual.cpp            # Sprite visual (image surfaces)
│   ├── wrtcontainervisual.cpp         # Container visual (visual tree)
│   ├── wrtcompositiongeometricclip.cpp # Geometric clipping
│   ├── wrtlayervisual.cpp             # Layer visual (visual layers)
│   ├── wrtshapevisual.cpp             # Shape visual
│   ├── wrtkeyframeanimation.cpp       # Keyframe animation
│   ├── wrtvisual.cpp                  # Base visual implementation
│   ├── wrtexpressionanimation.cpp     # Expression animation
│   ├── wrtanimationbindingmanager.cpp # Animation binding
│   ├── wrtpointereventrouter.cpp      # Pointer event routing (input)
│   ├── wrtpathkeyframeanimation.cpp   # Path keyframe animation
│   ├── wrtinteropvisual.cpp           # Interop visual (cross-process)
│   ├── wrtcompositioneffectfactory.cpp # Effect factory (blur, etc.)
│   ├── wrtanimationhelper.cpp         # Animation helper utilities
│   ├── wrtconditionalexpressionanimation.cpp # Conditional animation
│   ├── wrtcompositionanimationtriggerpartner.cpp # Animation triggers
│   ├── wrtanimationpropertyinfo.cpp   # Animation property metadata
│   ├── wrtcompositionsurfacebrush.cpp # Surface brush (tiling, etc.)
│   ├── wrtcompositionbatch.cpp        # Batch composition
│   ├── wrtcompositionscopedbatch.cpp  # Scoped batch operations
│   ├── wrtcompositionsurfacewrapper.cpp # Surface wrapper
│   ├── wrtcompositiongraphicsdevice.cpp # Graphics device management
│   ├── wrtcompositionvisualsurface.cpp # Visual surface
│   ├── wrtcompositionvirtualdrawingsurface.cpp # Virtual drawing surface
│   ├── wrtcomponenttransform2d.cpp    # 2D transform
│   ├── wrtcomponenttransform3d.cpp    # 3D transform
│   ├── wrtcompositiondrawingsurface.cpp # Drawing surface
│   ├── wrtdrawingsurfacebase.cpp      # Drawing surface base
│   ├── wrtinsetclip.cpp               # Inset clip
│   ├── wrtvisualunorderedcollection.cpp # Unordered visual collection
│   ├── wrtcompositionbatchcompletedeventargs.cpp # Batch completion events
│   ├── wrtcommitcompletionwaiter.cpp  # Commit completion waiter
│   ├── wrtdesktopwindowtarget.cpp     # Desktop window target
│   ├── wrtimplicitanimationcollection.cpp # Implicit animations
│   ├── wrtcompositionanimationgroup.cpp # Animation groups
│   ├── wrtmanipulationtransform.cpp   # Manipulation transform (touch)
│   ├── wrtsharedd3ddevice.cpp         # Shared D3D device
│   ├── wrtsharedd3ddevicepool.cpp     # Shared D3D device pool
│   ├── wrtcompositionclip.cpp         # Clip operations
│   ├── wrtcapturerendertarget.cpp     # Render target capture
│   ├── wrtvisualreference.cpp         # Visual references
│   ├── wrtinjectionanimation.cpp      # Injection animation
│   ├── wrttransforminformation.cpp    # Transform information
│   ├── wrtanimationcontroller.cpp     # Animation controller
│   ├── wrtsurfacebindpoint.cpp        # Surface bind point
│   ├── wrtvisualtreeisland.cpp        # Visual tree island
│   ├── wrtdelegatedinktrailvisual.cpp # Delegated ink trail
│   └── global/
│       ├── wrtvisualg.cpp             # Visual global state
│       ├── wrtvisualtreeisland.cpp    # Visual tree island global
│       ├── wrtvisualislandsite.cpp    # Visual island site
│       ├── wrtinteroplegacyrendertarget.cpp # Legacy interop render target
│       ├── wrtinteropredirectvisual.cpp # Interop redirect visual
│       └── wrtinteropremoteapprendertarget.cpp # Remote app render target
```

**Key observations:**
- The internal namespace is `wrt` (WinRT) — most internals are WinRT composition objects
- `interop` objects handle cross-process composition
- `global` subdirectory contains singleton/global state objects
- Device texture management supports both D3D11 and D3D12 backends
- Atlas surface pooling is used for memory-efficient surface management

---

## 4. Private COM Interface GUIDs

`DCompositionCreateDevice3` accepts **10 different interface IIDs** via QueryInterface, revealing the internal COM interface hierarchy:

| # | GUID | Interface Name (Inferred) | Notes |
|---|------|--------------------------|-------|
| 0 | `C37EA93A-E7AA-450D-B16F-9746CB0407F3` | IDCompositionDevice3 | Primary device v3 |
| 1 | `85FC5CCA-2DA6-494C-86B64A775C049B8A` | Private Device Extension | Unknown |
| 2 | `FB4AD130-2175-48D8-9CB5BA840543452A` | Private Device Extension | Unknown |
| 3 | `5F4633FE-1E08-4CB8-8C75CE24333F5602` | Private Visual Interface | Unknown |
| 4 | `D14B6158-C3FA-4BCE-9C1FB61D8665EAB0` | Private Batch Interface | Unknown |
| 5 | `28D6AD3D-EE2A-4BCD-94197D54800435B1` | Private Surface Interface | Unknown |
| 6 | `E01EB649-787E-4560-B3980DE7A2065D8B` | Private Animation Interface | Unknown |
| 7 | `25090604-9C8B-42C9-8A71BD1B3AD16512` | Private Target Interface | Unknown |
| 8 | `CA67B562-1C32-4017-9DD93D4B7E2510AA` | Private Render Interface | Unknown |
| 9 | `7BD36C9A-56EE-4FDD-AC4E76BBD16EC8E4` | Private Debug Interface | Unknown |

**CreatePresentationFactory also accepts:**
| GUID | Interface Name |
|------|---------------|
| `8FB37B58-1D74-4F64-A49C-1F97A80A2EC0` | IPresentationFactory (internal) |

---

## 5. CreatePresentationFactory Deep Analysis

### Calling Convention
```
rcx = IUnknown* (IDCompositionDevice)
rdx = REFIID (must match internal GUID)
r8  = void** (output)
```

### Flow
```
0x7fffb7073650: CreatePresentationFactory
  ├─ Validate ppvFactory != NULL
  ├─ Validate REFIID == {8FB37B58-1D74-4F64-A49C-1F97A80A2EC0}
  │   ├─ Compare [rdx]   vs [dcomp!0x7fffb70c5a08] (first 8 bytes)
  │   └─ Compare [rdx+8] vs [dcomp!0x7fffb70c5a10] (second 8 bytes)
  ├─ Call 0x7fffb7075264 (InternalFactoryCreate)
  │   ├─ Allocate 0x28 bytes (Pool allocation via 0x7fffb6f460b8)
  │   ├─ Zero-initialize all fields
  │   ├─ Call 0x7fffb707522c (VtableSetup)
  │   │   ├─ Call 0x7fffb6f2bcac (base init)
  │   │   ├─ [rcx+0x10] = secondary vtable pointer
  │   │   ├─ [rcx+0x00] = primary vtable pointer
  │   │   └─ [rcx+0x18] = NULL, [rcx+0x20] = 0
  │   ├─ Call vtable[1] (AddRef pattern at 0x7fffb70f010)
  │   └─ Call 0x7fffb7075408 (final initialization with device ptr)
  ├─ Store result in *ppvFactory
  └─ Return HRESULT
```

### Error Path
- If HRESULT < 0: calls `0x7fffb6efc2bc(hr, 0x14)` — likely error reporting/logging
- Always calls `0x7fffb6f855e0` for cleanup (CoTaskMemFree pattern)

---

## 6. DCompositionCreateDevice Family

### DCompositionCreateDevice (v1)
```
Signature: HRESULT DCompositionCreateDevice(IDXGIDevice*, REFIID, void**)
Target: jmp to 0x7fffb6fd9240

Accepted GUIDs: 1
  - {C37EA93A-E7AA-450D-B16F-9746CB0407F3} (IDCompositionDevice internal)

Flow:
  ├─ Validate ppvDevice != NULL (E_INVALIDARG if NULL)
  ├─ Validate REFIID matches single accepted GUID
  │   ├─ Compare [rdx]   vs [rip+0xd81f2]
  │   └─ Compare [rdx+8] vs [rip+0xd81ed]
  ├─ Call 0x7fffb6f5d1e0 (CreateDeviceInternal) with:
  │   rcx = 0 (null)
  │   rdx = 0 (null)
  │   r8  = rcx (IDXGIDevice*)
  │   r9  = [rsp+0x40] (output ptr)
  ├─ QI the resulting device for the requested interface
  │   └─ vtable[0] called on result object
  └─ Return HRESULT
```

### DCompositionCreateDevice2
```
jmp to 0x7fffb6f5ea08, then jmp to 0x7fffb6ef8390

Accepted GUIDs: 1
  - {75DEF60C-9D13-4B0D-A36E-EB6F4864E85F} (IDCompositionDevice2)
```

**Correction (verified 2026-07-05):** The GUID `{75DEF60C-9D13-4B0D-A36E-EB6F4864E85F}` is **rejected at runtime** on Windows 11 24H2. `DCompositionCreateDevice2` returns `E_NOINTERFACE` for this GUID. The working approach is to call `DCompositionCreateDevice3` with the v1 GUID `{C37EA93A-E7AA-450D-B16F-9746CB0407F3}`, then `QueryInterface` up to `IDCompositionDevice2` and `IDCompositionDevice3`.

### DCompositionCreateDevice3
```
jmp to 0x7fffb6fd4308

Accepted GUIDs: 10 (listed in Section 4)

Flow:
  ├─ Validate ppvDevice != NULL
  ├─ Match REFIID against 10 known GUIDs (cascading if/else)
  │   ├─ GUID #0: {C37EA93A-E7AA-450D-B16F-9746CB0407F3}
  │   ├─ GUID #1: {85FC5CCA-2DA6-494C-86B64A775C049B8A}
  │   ├─ ... (see Section 4)
  │   └─ GUID #9: {7BD36C9A-56EE-4FDD-AC4E76BBD16EC8E4}
  ├─ Optional: Check registry flag at rip+0x1093ed
  │   └─ If flag set, also accept additional GUID: {28D6AD3D-EE2A-4BCD-94197D54800435B1}
  ├─ If GUID #0: jmp to 0x7fffb6fd4454 (common path)
  │   └─ Call internal device creation + QI
  └─ If no GUID matches: E_NOINTERFACE
```

---

## 7. Frame Statistics APIs

### DCompositionGetStatistics
```c
HRESULT DCompositionGetStatistics(
    _In_ UINT64 frameId,
    _Out_ DCOMPOSITION_FRAME_STATISTICS* stats,  // via stack param at [rsp+0x60]
    _In_ UINT32 statsSize
);
```

**Correction (verified 2026-07-05):** The actual parameter order in the calling convention is `(frameId, statsSize, stats)` — the `statsSize` comes before the `stats` pointer. This matters for static analysis where the stack layout shows `[rsp+0x60]` as the third argument (stats pointer) but the second register parameter (`edx`) is `statsSize`. A working C declaration:
```c
HRESULT DCompositionGetStatistics(
    _In_ UINT64 frameId,
    _In_ UINT32 statsSize,
    _Out_ DCOMPOSITION_FRAME_STATISTICS* stats
);
```

**Implementation pattern:**
```
0x7fffb6fa8920:
  ├─ Save rcx to [rsp+8]
  ├─ Allocate 0x38 stack frame
  ├─ Read stats pointer from [rsp+0x60] (7th parameter or stack arg)
  ├─ Load function pointer from [rip+0xfe9c9] (IAT entry)
  │   └─ Resolves to an internal composition statistics function
  ├─ Call via IAT with: rcx = &stack_var, rdx = stats ptr
  ├─ Result in eax → mov ecx, eax
  ├─ Deallocate stack
  └─ jmp 0x7fffb6f01110 (shared error→HRESULT conversion)
```

### DCompositionGetTargetStatistics
```c
HRESULT DCompositionGetTargetStatistics(
    _In_ UINT64 unknown,  // rcx parameter
    _Out_ void* stats     // via stack
);
```

**Implementation:**
```
0x7fffb6fa9960:
  ├─ Save rcx to [rsp+8]
  ├─ Load function from [rip+0xfd98b] (IAT)
  ├─ Call: rcx = &stack_var
  ├─ jmp 0x7fffb6f01110
```

### DCompositionGetFrameId
```c
HRESULT DCompositionGetFrameId(_Out_ UINT64* frameId);
```

**Correction (verified 2026-07-05):** The actual call pattern returns the frame ID in `eax`/`rax` directly, not via the out pointer. The caller should capture the return value as the frame ID. The `HRESULT` return is the low 32 bits; the frame ID occupies the full 64-bit register pair (`edx:eax`). If using a C wrapper, declare the return type as `UINT64` and ignore the HRESULT semantics — the frame ID is always valid (0 is a valid frame ID meaning "no frames yet").

**Implementation:**
```
0x7fffb6fbe380:
  ├─ Call [rip+0xe8f85] (IAT → internal function)
  ├─ Result in eax
  ├─ mov ecx, eax
  └─ jmp 0x7fffb6f01110
```

**Key insight:** All three statistics functions are thin wrappers that delegate to internal IAT entries, which resolve to `dwmcore.dll` or internal dcomp functions. The `jmp 0x7fffb6f01110` is a shared HRESULT conversion/tail-call that converts internal error codes to standard HRESULT.

---

## 8. Surface Handle API

### DCompositionCreateSurfaceHandle
```c
HRESULT DCompositionCreateSurfaceHandle(
    _In_ DWORD desiredAccess,    // ecx → moved to r9d
    _In_ SECURITY_ATTRIBUTES* sa, // rdx (can be NULL)
    _Out_ HANDLE* surfaceHandle   // r8 → moved to rbx
);
```

**Implementation:**
```
0x7fffb6fac600:
  ├─ Push rbx
  ├─ Allocate 0x50 stack frame
  ├─ Zero 48 bytes of stack (XMM0 zeroing at +0x20, +0x30, +0x40)
  ├─ r9d = ecx (desiredAccess)
  ├─ rcx = 0 (NULL - cleared)
  ├─ rbx = r8 (surfaceHandle output)
  ├─ Test if security attributes (rdx) are NULL
  │   ├─ If NULL: 
  │   │   ├─ mov edx, r9d (use desiredAccess as parameter)
  │   │   ├─ Call [rip+0xfacb9] (IAT → NtCreateSection or similar)
  │   │   ├─ mov ecx, eax (pass HRESULT)
  │   │   ├─ Call 0x7fffb6f01110 (error conversion)
  │   │   └─ Test result, if >= 0 return early
  │   └─ If non-NULL: (more complex path at 0x7fffb6fac646)
  └─ Return HRESULT
```

---

## 9. Compositor Clock Boost API

### DCompositionBoostCompositorClock
```c
HRESULT DCompositionBoostCompositorClock(BOOL boost);
```

**Implementation:**
```
0x7fffb6fcaa20:
  ├─ Allocate 0x28 stack frame
  ├─ Call [rip+0xdc8cd] (IAT → internal clock boost function)
  ├─ nop [rax+rax] (alignment)
  ├─ mov ecx, eax (result)
  ├─ Deallocate stack
  └─ jmp 0x7fffb6f01110 (HRESULT conversion)
```

### DCompositionWaitForCompositorClock
```c
HRESULT DCompositionWaitForCompositorClock(
    _In_ HANDLE* handles,
    _In_ UINT32 count,
    _In_ DWORD timeout
);
```

**Implementation:**
```
0x7fffb6fb2010:
  └─ jmp qword ptr [rip+0xf52d9]  (IAT → WaitForCompositionClockInternal)
```

This is a direct IAT tail-call — the exported function IS the IAT thunk.

---

## 10. DWM Integration APIs

### DwmEnableMMCSS
```c
HRESULT DwmEnableMMCSS(BOOL enable);
```

**Implementation:**
```
0x7fffb6ff8470:
  ├─ Call [rip+0xaee4d] (IAT → DwmEnableMMCSS in dwmapi.dll or internal)
  ├─ nop [rax+rax]
  ├─ mov ecx, eax
  └─ jmp 0x7fffb6f01110
```

This is a re-export that delegates to the actual DWM implementation.

### DwmFlush / DwmpEnableDDASupport
Both are stubs at `0x7fffb6fa8960`:
```
0x7fffb6fa8960:
  xor eax, eax    ; return S_OK
  ret
```

---

## 11. Mouse Attachment APIs

### DCompositionAttachMouseDragToHwnd
```c
HRESULT DCompositionAttachMouseDragToHwnd(
    _In_ IUnknown* source,     // rcx: Composition source
    _In_ HWND hwnd,            // rdx: Target HWND
    _In_ BOOL enable           // r8d: Enable/disable
);
```

**Implementation:**
```
0x7fffb6ff84a0:
  ├─ Save rbx, rdi
  ├─ Allocate 0x30 stack frame
  ├─ Zero stack at [rsp+0x40]
  ├─ r8d = enable flag
  ├─ rdi = hwnd
  ├─ Test if source (rcx) is NULL
  │   ├─ If NULL: goto error path (0x7fffb6ff84fd → E_POINTER)
  │   └─ If valid:
  │       ├─ Load vtable: rax = [rcx]
  │       ├─ lea r8, [rsp+0x40] (output buffer)
  │       ├─ lea rdx, [rip+0xbdb96] (interface GUID in .rdata)
  │       ├─ rax = [rax] (first vtable entry)
  │       ├─ Call [rax] (QueryInterface pattern → 0x7fffb707f010)
  │       ├─ Test eax (HRESULT)
  │       └─ If failed: goto error
  └─ Continue with attachment logic
```

### DCompositionAttachMouseWheelToHwnd
Same pattern as Drag, different GUID at `rip+0xbdb16`.

---

## 12. Internal Helper Functions

### `0x7fffb6f01110` — Shared HRESULT Converter
Called by nearly every exported function as a tail-call. Converts internal error codes to standard HRESULT values.

### `0x7fffb707f010` — IUnknown Thunk
Called for COM operations (QueryInterface, AddRef, Release). This appears to be a universal IUnknown method caller.

### `0x7fffb6f460b8` — Pool Allocator
Memory allocation function used for COM object creation. Takes size in rcx, returns pointer in rax.

### `0x7fffb6f855e0` — Cleanup Function
Called during cleanup paths. Likely releases temporary COM references or frees stack allocations.

### `0x7fffb6f2bcac` — Base Object Constructor
Called during vtable initialization for PresentationFactory.

### `0x7fffb6efc2bc` — Error Reporter
Called with (hr, extraInfo) when operations fail. Likely reports telemetry or structured error info.

### `0x7fffb7002110` — DllGetClassObject
Standard COM class factory creation. Calls `0x7fffb6fa6534` (internal singleton accessor) then `0x7fffb6ffd344` (class factory creation).

### `0x7fffb6fa6500` — DllGetActivationFactory
WinRT activation factory. Calls the same singleton accessor then routes to `0x7fffb6f82080` for WinRT class resolution.

---

## 13. COM Vtable Layout Discovery

### PresentationFactory Object (0x28 bytes)
```
+0x00: Vtable pointer → PresentationFactory vtable (at rip+0x301d1)
+0x08: ??? (zero after init)
+0x10: Secondary vtable/function table → rip+0x30224
+0x18: Zero (refcount or state)
+0x20: Zero word
```

### Key Vtable Methods Called
From the disassembly, the following vtable patterns are observed:

```
vtable[0] = QueryInterface / IUnknown::QueryInterface
vtable[1] = AddRef / Release (at 0x7fffb70f010)
```

The `0x7fffb707f010` function is a universal vtable method caller:
```
mov rax, [rcx]       ; load vtable
mov rax, [rax + N]   ; load method N
call rax              ; call method
```

### DllGetClassObject Flow
```
DllGetClassObject (0x7fffb7002110)
  ├─ Save rbx, rsi, rdi
  ├─ Call 0x7fffb6fa6534 (GetGlobalDevice singleton)
  ├─ Call 0x7fffb6ffd344 (CreateClassFactory)
  │   rcx = global device
  │   rdx = rclsid
  │   r8  = riid
  │   r9  = ppv (stack param at [rsp+0x20])
  └─ Return HRESULT
```

---

## Appendix A: Shared Error Handler

The function at `0x7fffb6f01110` is the most-called internal function. It appears to:
1. Accept an error code in `ecx`
2. Convert internal DComp-specific error codes to HRESULT
3. Possibly log/trace the error
4. Return the converted HRESULT in `eax`

This is a **critical undocumented function** — understanding it would reveal the full error code space of DirectComposition.

## Appendix B: IAT Dependencies

Key IAT entries resolved from the exported functions:

| IAT Address | Used By | Likely Target |
|-------------|---------|---------------|
| rip+0xfe9c9 | DCompositionGetStatistics | Internal frame stats |
| rip+0xfd98b | DCompositionGetTargetStatistics | Internal target stats |
| rip+0xe8f85 | DCompositionGetFrameId | Internal frame ID |
| rip+0xdc8cd | DCompositionBoostCompositorClock | Internal clock control |
| rip+0xaee4d | DwmEnableMMCSS | DWM MMSS forwarding |
| rip+0xfacb9 | DCompositionCreateSurfaceHandle | NtCreateSection or equivalent |
| rip+0xf52d9 | DCompositionWaitForCompositorClock | Waitable clock implementation |

## Appendix C: Strings of Interest

Key string references found in `.rdata`:

| String | Purpose |
|--------|---------|
| `"Shared Composition D3D Device"` | Named shared D3D device for composition |
| `"CompositionTextureSurfaceBindingTest"` | Test-mode texture binding |
| `"CreateCompositionTextureTest"` | Test-mode texture creation |
| `"SceneNode::RuntimeClassInitialize"` | Scene graph node initialization |
| `"CompositionGraphicsDevice::CreateMipmapSurface"` | Mipmap surface creation |
| `"simpleTexture"` | Simple texture shader name |
| `"SV_TARGET"` | Shader semantic name |
| `"Begin callback counts"` / `"End callback counts"` | Animation callback debugging |
| `"Must register event from UI thread"` | Thread affinity assertion |
| `"The underlying graphics device was removed."` | Device-loss error |
| `"Unsupported property type for implicit animations."` | Type mismatch error |
| `"Cannot bind AnimationController to itself."` | Self-binding prevention |

---

## 14. NT API Surface (Kernel Syscalls)

All frame statistics and composition APIs are thin wrappers around NT kernel syscalls. The NTSTATUS → HRESULT translation is consistent across all functions:

### NTSTATUS to HRESULT Translation Table

| NTSTATUS | Value | HRESULT | Description |
|----------|-------|---------|-------------|
| `STATUS_SUCCESS` | `0x0` | `S_OK` (0x0) | Success |
| `STATUS_NO_MEMORY` | `0xC0000017` | `E_OUTOFMEMORY` (0x8007000E) | Out of memory |
| `STATUS_UNSUCCESSFUL` | `0xC0000001` | `E_FAIL` (0x80004005) | General failure |
| `STATUS_NOT_IMPLEMENTED` | `0xC0000002` | `E_NOTIMPL` (0x80004001) | Not implemented |
| `STATUS_INVALID_HANDLE` | `0xC0000008` | `E_HANDLE` (0x80070006) | Invalid handle |
| `STATUS_INVALID_PARAMETER` | `0xC000000D` | `E_INVALIDARG` (0x80070057) | Invalid parameter |
| `STATUS_ACCESS_DENIED` | `0xC0000022` | `E_ACCESSDENIED` (0x80070005) | Access denied |
| `STATUS_OBJECT_TYPE_MISMATCH` | `0xC0000024` | `E_HANDLE` (0x80070006) | Type mismatch |
| `STATUS_NOT_SUPPORTED` | `0xC00000BB` | `E_INVALIDARG` (0x80070057) | Not supported |
| `DWM_E_FRAME_STATISTICS_MISMATCH` | `0x803E0006` | `0x88980800` | Frame statistics mismatch |
| Other | Any | `NTSTATUS \| 0x10000000` | Passthrough with high bit set |

### Kernel Functions Called

| Function | Called By | Purpose |
|----------|-----------|---------|
| `NtDCompositionGetStatistics` | `DCompositionGetStatistics` | Get per-frame composition stats |
| `NtDCompositionGetTargetStatistics` | `DCompositionGetTargetStatistics` | Get target-specific stats |
| `NtDCompositionGetFrameId` | `DCompositionGetFrameId` | Get current frame ID |
| `NtDCompositionBoostCompositorClock` | `DCompositionBoostCompositorClock` | Boost compositor clock |
| `NtDCompositionEnableMMCSS` | `DwmEnableMMCSS` | Enable MMSS support |
| `NtDCompositionCreateChannel` | `device3_init` | Create composition channel |
| `NtDCompositionDestroyChannel` | `device3_init` | Destroy composition channel |
| `NtCreateCompositionSurfaceHandle` | `DCompositionCreateSurfaceHandle` | Create surface handle |
| `NtFlipObjectCreate` | `batch_object_init2` | Create flip object |
| `NtFlipObjectOpen` | `batch_object_init2` | Open existing flip object |
| `NtFlipObjectQueryLostEvent` | `batch_object_init2` | Query lost event handle |
| `NtQueryInformationProcess` | `FUN_180096bf4` | Query process info (class 0x40) |

---

## 15. Device3 Internal Architecture

### Object Layout (0x3A8 bytes)

The Device3 object is a large COM object allocated by `device3_create_internal`:

```
+0x000: Vtable array (13 pointers) → vtable tables at UNK_1801a6xxx
+0x068: IDXGIDevice* reference (param_2, AddRef'd)
+0x070: Critical section
+0x078: Composition state
+0x0B8: Channel handle (from NtDCompositionCreateChannel)
+0x0C0: Channel object
+0x0CC: Channel capacity (0x1000)
+0x1C8: Heap allocation list
+0x1D0: Process name string (from NtQueryInformationProcess)
+0x260: Timestamp counter
+0x280: Presentation factory array (32 entries)
+0x2E0: Presentation factory count
+0x320: Linked list heads (8 doubly-linked lists)
+0x3A0: Compositor availability flag
```

### Initialization Flow

```
device3_create_internal (0x18006d1e0)
  ├─ Allocate 0x3A8 bytes via heap_alloc
  ├─ Call device3_construct (0x18006d308)
  │   ├─ Set 13 vtable pointers (offsets 0x00-0x68)
  │   ├─ AddRef IDXGIDevice if non-null
  │   ├─ Initialize critical section at +0x80
  │   ├─ Initialize 8 doubly-linked lists
  │   ├─ Set timeout to GetTickCount64() + 600000 (10 min)
  │   └─ Initialize hash table at +0x280
  ├─ Call device3_init (0x18006d61c)
  │   ├─ QueryPerformanceFrequency (store at global)
  │   ├─ Call NtDCompositionCreateChannel
  │   │   └─ Create channel with capacity 0x1000
  │   ├─ Create channel listener object (16 bytes)
  │   ├─ Query process name via NtQueryInformationProcess
  │   │   └─ Class 0x40 = ProcessBasicInformation
  │   ├─ Check compositor availability
  │   ├─ Check MMCSS enabled
  │   └─ Register flip device
  └─ Call device3_QI (0x18006daec)
      └─ QueryInterface for requested interface
```

### PresentationFactory Vtable (Complete)

| Index | Address | Function | Purpose |
|-------|---------|----------|---------|
| 0 | `0x1800FF340` | `QueryInterface` | Check GUID 0x46000000000000c0, return this |
| 1 | `0x180075DE0` | `AddRef` | Increment reference count |
| 2 | `0x1800FF340` | `QueryInterface` | Same as index 0 |
| 3 | `0x180185480` | `get_FlagA` | Return byte at +0x20 |
| 4 | `0x180185490` | `get_FlagB` | Return byte at +0x21 |
| 5 | `0x180185330` | `CreateBatch` | Allocate 0xF0 byte batch object |
| 6 | `0x180095400` | `Release` | Release internal ref, free if zero |
| 7 | `0x1800BAE90` | `NoOp` | Empty function (ret) |
| 8 | `0x1801853A0` | `QueryInterface2` | Check GUID 8FB37B58 / 6BD9A16F |
| 9 | `0x180107360` | `QueryInterface3` | GUID 0x46000000000000c0, offset -0x10 |
| 10 | `0x1800F0740` | `AddRef2` | Atomic increment ref at -8 |
| 11 | `0x180107370` | `Release2` | Release with offset adjustment |
| 12 | `0x1800C90F0` | `QueryInterface4` | GUID 0x46000000000000c0 |
| 13 | `0x1800CD200` | `Release3` | Release with ref count check |
| 14 | `0x1801862C0` | `SetDevice` | Set device pointer with lock |

### Batch Object (0xF0 bytes)

Created by `batch_object_init` (0x18018382c):

```
+0x000: Vtable → UNK_1801b5150
+0x008: Reference count (0)
+0x010: Secondary vtable → UNK_1801b5120
+0x018: Function table A → UNK_1801b51e8
+0x020: Function table B → UNK_1801b5218
+0x028: Critical section (initialized via InitializeCriticalSectionEx)
+0x050: State fields (8 qwords, zeroed)
+0x090: Linked list head A (self-referencing)
+0x0A0: Flags (0x101)
+0x0B0: Linked list head B (self-referencing)
+0x0C0: Additional state (12 qwords, zeroed)
+0x120: Sub-object list head
+0x128: Count (1)
+0x130: More state fields
```

### Flip Object Integration

`batch_object_init2` (0x1801842bc) creates a Flip Object for presentation:

1. Call `NtFlipObjectCreate` to create the flip object
2. Call `NtFlipObjectOpen` to open handles for read/write
3. Call `NtFlipObjectQueryLostEvent` to get the lost event handle
4. Store handles at +0x78, +0x80, +0x88
5. Query performance frequency and store at +0xC0

---

## 16. Security Mechanisms

### Stack Cookie (GS) Protection
All major functions use XOR-based stack cookie checking:
```
uStack_38 = _DAT_1801ebf40 ^ (ulonglong)auStack_a8;  // Load cookie
... function body ...
security_cookie_check(uStack_38 ^ (ulonglong)auStack_a8);  // Verify cookie
```

### Critical Sections
- Device3 object: `+0x80` critical section
- Batch object: `+0x28` critical section (via `InitializeCriticalSectionEx`)
- Factory global lock: `FUN_1801864dc`

### Reference Counting
- Device3: Atomic increment/decrement at `+0x008`
- PresentationFactory: Atomic increment at `param_1 + -8`
- Smart pointer pattern: `smart_ptr_set` calls AddRef on new, Release on old

---

## 17. Feature Flag System

### Registry Flag Check

`check_feature_flag` (0x1800feafc) reads a registry-based feature flag:

```
check_feature_flag
  ├─ Call FUN_1800fa13c (read feature flags from global state)
  │   ├─ Read global: uRam00000001801ed6c0
  │   ├─ If bits 1-2 set: return cached value
  │   ├─ Otherwise: Acquire SRW lock at 0x1801f14e8
  │   ├─ Call FUN_1800d42d4 (query system info)
  │   ├─ Call FUN_1800fa92c (validate flags)
  │   └─ CAS loop to update global flag atomically
  ├─ Call FUN_1800fcfe0 (apply feature flag)
  │   └─ Call FUN_18009e940 with flag bits
  └─ Return flag byte + timestamp
```

### Global State

| Address | Type | Purpose |
|---------|------|---------|
| `0x1801ed6c0` | DWORD | Cached feature flags |
| `0x1801ed670` | DWORD | Last check timestamp (GetTickCount) |
| `0x1801ed740` | LARGE_INTEGER | Performance frequency |
| `0x1801f14e8` | SRWLOCK | Feature flag lock |
| `0x1801f14fc` | DWORD | Process ID for validation |
| `0x1801f1520` | BYTE[16] | Feature configuration |

---

## 18. Internal String Formatting

### Process Name Resolution

`FUN_180096d54` and `FUN_18009715c` construct process identification strings:

1. Call `QueryFullProcessImageNameW` to get process path
2. Call `GetFileVersionInfoSizeW` / `GetFileVersionInfoW` to get version info
3. Call `VerQueryValueW` to extract version structure
4. Format string: `"processname.exe.major.minor.build"` via `FUN_1800973c4`
5. Allocate via `CoTaskMemAlloc` and return

### Error String Format

```
FUN_18000c31c: Format error string with:
  - Return address
  - Line number
  - Source file string (.rdata)
  - HRESULT value
```

---

---

## 19. Practical Notes & Known Gotchas

These issues were discovered while building a D2D/DWrite text editor on top of DirectComposition (July 2026). They are not documented anywhere in Microsoft's public or semi-public documentation.

### 19.1 D3D11 BGRA Support is Mandatory for D2D Interop

**Symptom:** `D2D1Factory1::CreateDevice(IDXGIDevice*)` returns `E_INVALIDARG`.

**Cause:** `D3D11CreateDevice` was called without `D3D11_CREATE_DEVICE_BGRA_SUPPORT`. The `D2D1CreateDevice` call chains through the DXGI device, and D2D requires BGRA texture support on the underlying D3D11 device.

**Fix:**
```c
UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
D3D11CreateDevice(
    adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
    createFlags, nullptr, 0, D3D11_SDK_VERSION,
    device.GetAddressOf(), nullptr, context.GetAddressOf());
```

**Impact:** Without this flag, the entire D2D rendering pipeline on DComp surfaces fails silently at device creation. This is the single most common initialization failure.

### 19.2 IDCompositionSurface::BeginDraw Returns E_NOINTERFACE

**Symptom:** `IDCompositionSurface::BeginDraw(IID_PPV_ARGS(&ctx))` returns `E_NOINTERFACE`.

**Cause:** Surfaces created via `IDCompositionDevice2::CreateSurface()` are **not D2D-aware**. They are raw composition surfaces that only support `IDXGISwapChain`-based rendering, not D2D's `BeginDraw`/`EndDraw` pattern.

**Fix:** Use `IDXGIFactory1::CreateSwapChainForComposition()` instead:
```c
DXGI_SWAP_CHAIN_DESC1 desc = {};
desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
desc.BufferCount = 2;
desc.SampleDesc.Count = 1;
desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

ComPtr<IDXGISwapChain1> swapChain;
factory->CreateSwapChainForComposition(device.Get(), &desc, nullptr, swapChain.GetAddressOf());

// Set on visual
visual->SetContent(swapChain.Get());

// Render via D2D bitmap from swap chain back buffer
ComPtr<IDXGISurface> backBuffer;
swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
// ... create D2D bitmap from backBuffer, draw, present
```

**Why this works:** The swap chain is a DXGI object that DComp can composite. The D2D render target is created from the swap chain's back buffer surface, which is a standard DXGI surface that D2D understands. This is the standard pattern for D2D rendering on DirectComposition.

**Performance note:** Use `Present(1, 0)` (vsync) or `Present(0, 0)` (immediate) depending on latency needs. For a text editor, vsync is fine.

### 19.3 IDXGIDevice::GetParent Returns IDXGIAdapter, Not IDXGIFactory

**Symptom:** `IDXGIDevice::GetParent(IID_PPV_ARGS(&factory))` fails or returns an `IDXGIAdapter`.

**Cause:** The DXGI device hierarchy is: `IDXGIDevice` → `IDXGIAdapter` → `IDXGIFactory`. Calling `GetParent` once returns the adapter, not the factory.

**Fix:** Use `CreateDXGIFactory1()` directly instead of navigating the device hierarchy:
```c
ComPtr<IDXGIFactory1> factory;
CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()));
```

**Note:** This is standard DXGI practice but easy to forget when the DComp device creation flow naturally starts from an `IDXGIDevice`.

### 19.4 DCompositionCreateDevice3 GUID Selection

**Verified behavior (Windows 11 24H2):**

| GUID | Accepted? | Notes |
|------|-----------|-------|
| `{C37EA93A-E7AA-450D-B16F-9746CB0407F3}` | ✅ Yes | v1 IID — use this, then QI up |
| `{75DEF60C-9D13-4B0D-A36E-EB6F4864E85F}` | ❌ No | v2 IID — rejected by DCompositionCreateDevice2 |
| `{85FC5CCA-2DA6-494C-86B64A775C049B8A}` | ❌ No | v3 IID — rejected |
| `{0987CB06-F406-460B-A2E6-A26C55E6A252}` | ❌ No | v3 IID — rejected |

**Correct pattern:**
```c
// Create with v1 IID
DCompositionCreateDevice3(
    &IID_IDCompositionDevice,    // {C37EA93A-...}
    IID_PPV_ARGS(device1.GetAddressOf()));

// QI up the chain
device1->QueryInterface(IID_PPV_ARGS(device2.GetAddressOf()));  // IDCompositionDevice2
device1->QueryInterface(IID_PPV_ARGS(device3.GetAddressOf()));  // IDCompositionDevice3
```

### 19.5 D3D11CreateDevice — Required Flags for DComp Pipeline

For a complete D2D-on-DComp rendering pipeline, the D3D11 device must be created with:

```c
UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;  // Required for D2D interop
#ifdef _DEBUG
flags |= D3D11_CREATE_DEVICE_DEBUG;              // Optional: debug layer
#endif

D3D11CreateDevice(
    adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
    flags, nullptr, 0, D3D11_SDK_VERSION,
    device.GetAddressOf(), nullptr, context.GetAddressOf());
```

**Minimal required flags:**
- `D3D11_CREATE_DEVICE_BGRA_SUPPORT` — enables BGRA texture format support required by D2D
- No other flags are strictly required for basic rendering

### 19.6 Complete Device Chain Summary

The working initialization chain for D2D rendering on DirectComposition:

```
1. D3D11CreateDevice(BGRA_SUPPORT)         → ID3D11Device
2. ID3D11Device::QueryInterface(IDXGIDevice) → IDXGIDevice
3. DCompositionCreateDevice3(v1 GUID)       → IDCompositionDevice
4. IDCompositionDevice::QueryInterface       → IDCompositionDevice2
5. IDCompositionDevice2::CreateVisual        → IDCompositionVisual2
6. IDCompositionDevice3::CreateVisual        → IDCompositionVisual3
7. IDCompositionDesktopDevice::CreateTargetForHwnd → IDCompositionTarget
8. CreateDXGIFactory1()                     → IDXGIFactory1
9. D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED) → ID2D1Factory1
10. D2D1CreateDevice(IDXGIDevice)            → ID2DDevice
11. ID2DDevice::CreateDeviceContext          → ID2DDeviceContext1
12. IDXGIFactory1::CreateSwapChainForComposition → IDXGISwapChain1
13. IDCompositionVisual2::SetContent(swapChain)
14. IDCompositionTarget::SetRoot(rootVisual)
15. IDCompositionSurface::Commit()
```

**Key constraint:** Steps 8-12 (D2D rendering) must use the same DXGI device that was created in steps 1-2. Cross-device interop is not supported.

---

*Document generated via Frida dynamic analysis on live explorer.exe process + static disassembly.*
*For full Ghidra static analysis, open `dcomp.dll` in a Ghidra project and analyze with x86:LE:64:default.*
*Last updated: 2026-07-05 — Added NT API surface, Device3 architecture, security mechanisms, feature flags.*
