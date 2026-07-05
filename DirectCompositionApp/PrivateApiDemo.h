#pragma once

#include <windows.h>
#include <dcomp.h>
#include <string>

class PrivateApiDemo {
public:
    PrivateApiDemo() = default;
    ~PrivateApiDemo() = default;

    bool QueryFrameStatistics();
    bool BoostCompositorClock(bool enable);
    bool WaitForCompositorClock(DWORD timeout);
    
    std::wstring GetStatisticsString() const;
    std::wstring GetFrameIdString() const;

private:
    COMPOSITION_FRAME_STATS m_stats = {};
    COMPOSITION_FRAME_ID m_frameId = 0;
    bool m_statsValid = false;
};
