#include "PrivateApiDemo.h"
#include <sstream>
#include <iomanip>

#pragma comment(lib, "dcomp.lib")

bool PrivateApiDemo::QueryFrameStatistics() {
    HRESULT hr = DCompositionGetFrameId(COMPOSITION_FRAME_ID_CONFIRMED, &m_frameId);
    if (FAILED(hr)) {
        m_statsValid = false;
        return false;
    }

    hr = DCompositionGetStatistics(
        m_frameId,
        &m_stats,
        0,
        nullptr,
        nullptr
    );
    
    m_statsValid = SUCCEEDED(hr);
    return m_statsValid;
}

bool PrivateApiDemo::BoostCompositorClock(bool enable) {
    HRESULT hr = DCompositionBoostCompositorClock(enable ? TRUE : FALSE);
    return SUCCEEDED(hr);
}

bool PrivateApiDemo::WaitForCompositorClock(DWORD timeout) {
    HRESULT hr = DCompositionWaitForCompositorClock(0, nullptr, timeout);
    return SUCCEEDED(hr);
}

std::wstring PrivateApiDemo::GetFrameIdString() const {
    if (!m_statsValid) return L"Frame ID: N/A";
    
    std::wstringstream ss;
    ss << L"Frame ID: " << m_frameId;
    return ss.str();
}

std::wstring PrivateApiDemo::GetStatisticsString() const {
    if (!m_statsValid) return L"Statistics: N/A";
    
    std::wstringstream ss;
    ss << L"=== Frame Statistics ===\n";
    ss << L"Frame ID: " << m_frameId << L"\n";
    
    double startTimeMs = m_stats.startTime / 10000.0;
    double targetTimeMs = m_stats.targetTime / 10000.0;
    double framePeriodMs = m_stats.framePeriod / 10000.0;
    
    ss << L"Start Time: " << std::fixed << std::setprecision(2) << startTimeMs << L" ms\n";
    ss << L"Target Time: " << std::fixed << std::setprecision(2) << targetTimeMs << L" ms\n";
    ss << L"Frame Period: " << std::fixed << std::setprecision(2) << framePeriodMs << L" ms\n";
    
    if (framePeriodMs > 0) {
        double fps = 1000.0 / framePeriodMs;
        ss << L"FPS: " << std::fixed << std::setprecision(1) << fps << L"\n";
    }
    
    return ss.str();
}
