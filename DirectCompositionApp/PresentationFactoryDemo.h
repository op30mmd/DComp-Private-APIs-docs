#pragma once

#include <windows.h>
#include <dcomp.h>
#include <string>

// {8FB37B58-1D74-4F64-A49C-1F97A80A2EC0}
static const GUID IID_IPresentationFactory = 
    {0x8FB37B58, 0x1D74, 0x4F64, {0xA4, 0x9C, 0x1F, 0x97, 0xA8, 0x0A, 0x2E, 0xC0}};

// Function pointer type for CreatePresentationFactory
typedef HRESULT (WINAPI *CreatePresentationFactory_t)(
    IUnknown* punkDevice,
    REFIID riid,
    void** ppvFactory
);

class PresentationFactoryDemo {
public:
    PresentationFactoryDemo() = default;
    ~PresentationFactoryDemo() = default;

    bool Initialize(IUnknown* punkDevice);
    bool QueryPresentationFactory();
    
    bool IsAvailable() const { return m_available; }
    std::wstring GetStatusString() const;

private:
    IUnknown* m_presentationFactory = nullptr;
    bool m_available = false;
    HMODULE m_hDcomp = nullptr;
};
