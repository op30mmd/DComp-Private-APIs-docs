#include "DirectManipHelper.h"

DirectManipHelper::DirectManipHelper() {}

DirectManipHelper::~DirectManipHelper() {
    if (m_viewport) {
        m_viewport->Stop();
        m_viewport->Abandon();
        if (m_eventCookie) {
            m_viewport->RemoveEventHandler(m_eventCookie);
        }
    }
}

ULONG STDMETHODCALLTYPE DirectManipHelper::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE DirectManipHelper::Release() {
    ULONG count = InterlockedDecrement(&m_refCount);
    if (count == 0) delete this;
    return count;
}

HRESULT STDMETHODCALLTYPE DirectManipHelper::QueryInterface(REFIID iid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (iid == __uuidof(IUnknown)) {
        *ppv = static_cast<IDirectManipulationViewportEventHandler*>(this);
    } else if (iid == __uuidof(IDirectManipulationViewportEventHandler)) {
        *ppv = static_cast<IDirectManipulationViewportEventHandler*>(this);
    } else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DirectManipHelper::OnViewportStatusChanged(
    IDirectManipulationViewport* viewport,
    DIRECTMANIPULATION_STATUS current,
    DIRECTMANIPULATION_STATUS previous)
{
    m_animating = (current == DIRECTMANIPULATION_RUNNING || current == DIRECTMANIPULATION_INERTIA);
    if (m_animating) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DirectManipHelper::OnContentUpdated(
    IDirectManipulationViewport* viewport,
    IDirectManipulationContent* content)
{
    float matrix[6] = {};
    HRESULT hr = content->GetContentTransform(matrix, 6);
    if (SUCCEEDED(hr)) {
        m_scrollY = -matrix[5];
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DirectManipHelper::OnViewportUpdated(
    IDirectManipulationViewport* viewport)
{
    return S_OK;
}

bool DirectManipHelper::Initialize(HWND hwnd) {
    m_hwnd = hwnd;

    HRESULT hr = CoCreateInstance(
        CLSID_DirectManipulationManager, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&m_manager));
    if (FAILED(hr)) return false;

    hr = m_manager->GetUpdateManager(IID_PPV_ARGS(&m_updateManager));
    if (FAILED(hr)) return false;

    hr = m_manager->CreateViewport(nullptr, hwnd, IID_PPV_ARGS(&m_viewport));
    if (FAILED(hr)) return false;

    DIRECTMANIPULATION_CONFIGURATION config =
        (DIRECTMANIPULATION_CONFIGURATION)(0
            | DIRECTMANIPULATION_CONFIGURATION_INTERACTION
            | DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y
            | DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_INERTIA);
    hr = m_viewport->ActivateConfiguration(config);
    if (FAILED(hr)) return false;

    hr = m_viewport->SetViewportOptions(
        (DIRECTMANIPULATION_VIEWPORT_OPTIONS)(DIRECTMANIPULATION_VIEWPORT_OPTIONS_MANUALUPDATE |
        DIRECTMANIPULATION_VIEWPORT_OPTIONS_DISABLEPIXELSNAPPING));
    if (FAILED(hr)) return false;

    hr = m_viewport->AddEventHandler(hwnd, this, &m_eventCookie);
    if (FAILED(hr)) return false;

    hr = m_manager->Activate(hwnd);
    if (FAILED(hr)) return false;

    hr = m_viewport->Enable();
    if (FAILED(hr)) return false;

    m_viewport->GetPrimaryContent(IID_PPV_ARGS(&m_content));
    return true;
}

void DirectManipHelper::SetViewportSize(float w, float h) {
    m_viewportW = w;
    m_viewportH = h;
    if (m_viewport) {
        RECT vpRect = { 0, 0, (LONG)w, (LONG)h };
        m_viewport->SetViewportRect(&vpRect);
    }
}

void DirectManipHelper::SetContentHeight(float h) {
    m_contentH = h;
    if (m_content) {
        RECT contentRect = { 0, 0, (LONG)m_viewportW, (LONG)h };
        m_content->SetContentRect(&contentRect);
    }
}

void DirectManipHelper::ScrollTo(float scrollY, BOOL animate) {
    if (!m_viewport) return;
    if (m_viewportW <= 0 || m_viewportH <= 0) return;

    if (scrollY < 0) scrollY = 0;
    float maxScroll = m_contentH - m_viewportH;
    if (maxScroll < 0) maxScroll = 0;
    if (scrollY > maxScroll) scrollY = maxScroll;

    m_viewport->ZoomToRect(
        0.0f, scrollY,
        m_viewportW, scrollY + m_viewportH,
        animate);

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void DirectManipHelper::AddContact(UINT32 pointerId) {
    if (m_viewport) {
        m_viewport->SetContact(pointerId);
    }
}

void DirectManipHelper::ProcessInput(MSG* msg, BOOL* handled) {
    if (m_manager) {
        m_manager->ProcessInput(msg, handled);
    }
}

void DirectManipHelper::Update() {
    if (m_updateManager) {
        m_updateManager->Update(0);
    }
}
