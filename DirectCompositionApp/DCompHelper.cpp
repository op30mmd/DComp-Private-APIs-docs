#include "DCompHelper.h"
#include <d3d11.h>
#include <dxgi.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <stdio.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

static const GUID IID_IDCompositionDesktopDevice =
    {0x5F4633FE, 0x1E08, 0x4CB8, {0x8C, 0x75, 0xCE, 0x24, 0x33, 0x3F, 0x56, 0x02}};
static const GUID IID_IDCompositionDevice1 =
    {0xC37EA93A, 0xE7AA, 0x450D, {0xB1, 0x6F, 0x97, 0x46, 0xCB, 0x04, 0x07, 0xF3}};
static const GUID IID_IDCompositionDevice2 =
    {0x75F6468D, 0x1B8E, 0x447C, {0x9B, 0xC6, 0x75, 0xFE, 0xA8, 0x0B, 0x5B, 0x25}};

#define DBG_LOG(fmt, ...) { char buf[512]; snprintf(buf, sizeof(buf), "[DComp] " fmt "\n", ##__VA_ARGS__); OutputDebugStringA(buf); }

bool DCompHelper::Initialize(HWND hwnd) {
    DBG_LOG("Initialize called, hwnd=%p", hwnd);
    HRESULT hr;

    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
        nullptr, 0, D3D11_SDK_VERSION, m_d3dDevice.GetAddressOf(), nullptr, nullptr);
    DBG_LOG("D3D11CreateDevice: 0x%08X", hr);
    if (FAILED(hr)) return false;

    ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_d3dDevice.As(&dxgiDevice);
    if (FAILED(hr)) return false;

    if (!CreateDevice(dxgiDevice.Get())) return false;

    ComPtr<IDCompositionDesktopDevice> desktopDevice;
    hr = m_device2.As(&desktopDevice);
    DBG_LOG("IDCompositionDesktopDevice: 0x%08X", hr);
    if (FAILED(hr)) return false;

    hr = desktopDevice->CreateTargetForHwnd(hwnd, TRUE, m_target.GetAddressOf());
    DBG_LOG("CreateTargetForHwnd: 0x%08X", hr);
    if (FAILED(hr)) return false;

    hr = CreateDXGIFactory1(IID_PPV_ARGS(m_dxgiFactory.GetAddressOf()));
    DBG_LOG("CreateDXGIFactory1: 0x%08X", hr);
    if (FAILED(hr)) return false;

    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1),
        nullptr, (void**)m_d2dFactory.GetAddressOf());
    DBG_LOG("D2D1CreateFactory: 0x%08X", hr);
    if (FAILED(hr)) return false;

    hr = m_d2dFactory->CreateDevice(dxgiDevice.Get(), m_d2dDevice.GetAddressOf());
    DBG_LOG("D2D1CreateDevice: 0x%08X", hr);
    if (FAILED(hr)) return false;

    hr = m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, m_d2dContext.GetAddressOf());
    DBG_LOG("D2D1CreateDeviceContext: 0x%08X", hr);
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        (IUnknown**)m_dwriteFactory.GetAddressOf());
    DBG_LOG("DWriteCreateFactory: 0x%08X", hr);
    if (FAILED(hr)) return false;

    return CreateVisualTree();
}

bool DCompHelper::CreateDevice(IDXGIDevice* dxgiDevice) {
    HRESULT hr = DCompositionCreateDevice3(dxgiDevice, IID_IDCompositionDevice1, (void**)m_device1.GetAddressOf());
    DBG_LOG("DCompositionCreateDevice3(v1): 0x%08X", hr);
    if (FAILED(hr)) return false;

    hr = m_device1.As(&m_device2);
    if (FAILED(hr)) return false;
    hr = m_device2.As(&m_device);
    DBG_LOG("Device chain: v1=%p v2=%p v3=%p", m_device1.Get(), m_device2.Get(), m_device.Get());
    return SUCCEEDED(hr);
}

bool DCompHelper::CreateVisualTree() {
    HRESULT hr = m_device2->CreateVisual(m_rootVisual.GetAddressOf());
    DBG_LOG("Root visual: 0x%08X", hr);
    if (FAILED(hr)) return false;

    hr = m_device2->CreateVisual(m_editorVisual.GetAddressOf());
    DBG_LOG("Editor visual: 0x%08X", hr);
    if (FAILED(hr)) return false;

    m_swapWidth = 800;
    m_swapHeight = 600;

    DXGI_SWAP_CHAIN_DESC1 scd = {};
    scd.Width = m_swapWidth;
    scd.Height = m_swapHeight;
    scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    hr = m_dxgiFactory->CreateSwapChainForComposition(
        m_d3dDevice.Get(), &scd, nullptr, m_swapChain.GetAddressOf());
    DBG_LOG("CreateSwapChainForComposition: 0x%08X", hr);
    if (FAILED(hr)) return false;

    m_editorVisual->SetContent(m_swapChain.Get());
    m_rootVisual->AddVisual(m_editorVisual.Get(), TRUE, nullptr);

    hr = m_target->SetRoot(m_rootVisual.Get());
    DBG_LOG("SetRoot: 0x%08X", hr);
    if (FAILED(hr)) return false;

    hr = m_device->Commit();
    DBG_LOG("Commit: 0x%08X", hr);
    return SUCCEEDED(hr);
}

bool DCompHelper::ResizeSwapChain(UINT width, UINT height) {
    if (width == 0 || height == 0) return true;
    if (width == m_swapWidth && height == m_swapHeight) return true;

    m_swapWidth = width;
    m_swapHeight = height;

    if (m_swapChain) {
        m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    }
    return true;
}

bool DCompHelper::UpdateFrameStatistics() {
    if (!m_device) return false;

    COMPOSITION_FRAME_ID frameId = 0;
    HRESULT hr = DCompositionGetFrameId(COMPOSITION_FRAME_ID_CONFIRMED, &frameId);
    if (SUCCEEDED(hr)) {
        m_lastFrameId = frameId;
    }

    hr = DCompositionGetStatistics(m_lastFrameId, &m_stats, 0, nullptr, nullptr);
    return SUCCEEDED(hr);
}
