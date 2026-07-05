#include "PresentationFactoryDemo.h"
#include <sstream>

#pragma comment(lib, "dcomp.lib")

bool PresentationFactoryDemo::Initialize(IUnknown* punkDevice) {
    if (!punkDevice) return false;

    m_hDcomp = GetModuleHandleW(L"dcomp.dll");
    if (!m_hDcomp) {
        m_hDcomp = LoadLibraryW(L"dcomp.dll");
        if (!m_hDcomp) return false;
    }

    auto pCreatePresentationFactory = reinterpret_cast<CreatePresentationFactory_t>(
        GetProcAddress(m_hDcomp, "CreatePresentationFactory")
    );

    if (!pCreatePresentationFactory) {
        m_available = false;
        return false;
    }

    HRESULT hr = pCreatePresentationFactory(
        punkDevice,
        IID_IPresentationFactory,
        reinterpret_cast<void**>(&m_presentationFactory)
    );

    m_available = SUCCEEDED(hr);
    return m_available;
}

bool PresentationFactoryDemo::QueryPresentationFactory() {
    if (!m_presentationFactory) return false;
    return true;
}

std::wstring PresentationFactoryDemo::GetStatusString() const {
    std::wstringstream ss;
    ss << L"=== Presentation Factory (Private API) ===\n";
    ss << L"Status: " << (m_available ? L"Available" : L"Not Available") << L"\n";
    ss << L"GUID: {8FB37B58-1D74-4F64-A49C-1F97A80A2EC0}\n";
    
    if (m_available && m_presentationFactory) {
        ss << L"Interface: " << std::hex << (uintptr_t)m_presentationFactory << L"\n";
    } else {
        ss << L"Note: CreatePresentationFactory is undocumented\n";
        ss << L"It may not be available on all Windows versions\n";
    }
    
    return ss.str();
}
