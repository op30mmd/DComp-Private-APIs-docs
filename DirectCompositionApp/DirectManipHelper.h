#pragma once

#include <windows.h>
#include <directmanipulation.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class DirectManipHelper : public IDirectManipulationViewportEventHandler {
public:
    DirectManipHelper();
    ~DirectManipHelper();

    bool Initialize(HWND hwnd);
    void CleanUp();
    void SetViewportSize(float w, float h);
    void SetContentHeight(float h);
    void ScrollTo(float scrollY, BOOL animate);
    void AddContact(UINT32 pointerId);
    void ProcessInput(MSG* msg, BOOL* handled);
    void Update();

    float GetScrollY() const { return m_scrollY; }
    bool IsAnimating() const { return m_animating; }

    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppv) override;
    HRESULT STDMETHODCALLTYPE OnViewportStatusChanged(
        IDirectManipulationViewport* viewport,
        DIRECTMANIPULATION_STATUS current,
        DIRECTMANIPULATION_STATUS previous) override;
    HRESULT STDMETHODCALLTYPE OnContentUpdated(
        IDirectManipulationViewport* viewport,
        IDirectManipulationContent* content) override;
    HRESULT STDMETHODCALLTYPE OnViewportUpdated(
        IDirectManipulationViewport* viewport) override;

private:
    LONG m_refCount = 0;
    ComPtr<IDirectManipulationManager> m_manager;
    ComPtr<IDirectManipulationUpdateManager> m_updateManager;
    ComPtr<IDirectManipulationViewport> m_viewport;
    ComPtr<IDirectManipulationContent> m_content;
    DWORD m_eventCookie = 0;
    HWND m_hwnd = nullptr;
    float m_scrollY = 0;
    float m_viewportW = 0;
    float m_viewportH = 0;
    float m_contentH = 0;
    bool m_animating = false;
};
