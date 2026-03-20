#pragma once


#include <chrono>
#include <array>

namespace nodepanda {

class PerformanceMetrics {
public:
    void beginFrame();
    void endFrame();

    float fps()         const { return m_fps; }
    float frameTimeMs() const { return m_frameTimeMs; }
    float memoryMB()    const { return m_memoryMB; }

    void updateMemoryUsage();

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    TimePoint m_frameStart{};

    static constexpr int SAMPLE_COUNT = 60;
    std::array<float, SAMPLE_COUNT> m_frameTimes{};
    int   m_sampleIndex    = 0;
    bool  m_bufferFull     = false;

    float m_fps            = 0.0f;
    float m_frameTimeMs    = 0.0f;
    float m_memoryMB       = 0.0f;
    int   m_memUpdateCount = 0; 
};

} 
