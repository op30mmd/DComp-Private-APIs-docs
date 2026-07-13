#pragma once

#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class DCompHelper {
public:
    bool Initialize(HWND hwnd);
    bool ResizeSwapChain(UINT width, UINT height);
    bool TryFinishResize(HWND hwnd);
    void MarkFirstPresent() { m_firstPresentDone = true; }
    bool IsFirstPresentDone() const { return m_firstPresentDone; }

    IDXGISwapChain1* GetSwapChain() const { return m_swapChain.Get(); }
    ID2D1DeviceContext* GetD2DContext() const { return m_d2dContext.Get(); }
    ID2D1Factory1* GetD2DFactory() const { return m_d2dFactory.Get(); }
    IDWriteFactory* GetDWriteFactory() const { return m_dwriteFactory.Get(); }
    UINT GetSwapWidth() const { return m_swapWidth; }
    UINT GetSwapHeight() const { return m_swapHeight; }

    bool UpdateFrameStatistics();
    UINT64 GetLastFrameId() const { return m_lastFrameId; }
    const COMPOSITION_FRAME_STATS& GetStatistics() const { return m_stats; }

private:
    bool CreateDevice(IDXGIDevice* dxgiDevice);
    bool CreateVisualTree(HWND hwnd);
    HWND m_hwnd = nullptr;

    ComPtr<ID3D11Device> m_d3dDevice;
    ComPtr<IDCompositionDevice> m_device1;
    ComPtr<IDCompositionDevice2> m_device2;
    ComPtr<IDCompositionDevice3> m_device;
    ComPtr<IDCompositionTarget> m_target;
    ComPtr<IDCompositionVisual2> m_rootVisual;
    ComPtr<IDCompositionVisual2> m_editorVisual;
    ComPtr<IDXGISwapChain1> m_swapChain;
    ComPtr<IDXGIFactory2> m_dxgiFactory;
    ComPtr<ID2D1Factory1> m_d2dFactory;
    ComPtr<ID2D1Device> m_d2dDevice;
    ComPtr<ID2D1DeviceContext> m_d2dContext;
    ComPtr<IDWriteFactory> m_dwriteFactory;

    UINT m_swapWidth = 0;
    UINT m_swapHeight = 0;
    bool m_firstPresentDone = false;
    COMPOSITION_FRAME_STATS m_stats = {};
    UINT64 m_lastFrameId = 0;
};
