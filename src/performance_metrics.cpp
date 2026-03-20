

#include "performance_metrics.h"
#include <numeric>

#ifdef _WIN32
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <psapi.h>       
    #pragma comment(lib, "psapi.lib")
#endif

namespace nodepanda {

void PerformanceMetrics::beginFrame() {
    m_frameStart = Clock::now();
}

void PerformanceMetrics::endFrame() {
    auto now = Clock::now();
    float dt = std::chrono::duration<float, std::milli>(now - m_frameStart).count();

    m_frameTimes[m_sampleIndex] = dt;
    m_sampleIndex = (m_sampleIndex + 1) % SAMPLE_COUNT;
    if (m_sampleIndex == 0) m_bufferFull = true;

    int count = m_bufferFull ? SAMPLE_COUNT : m_sampleIndex;
    if (count > 0) {
        float sum = 0.0f;
        for (int i = 0; i < count; ++i) sum += m_frameTimes[i];
        m_frameTimeMs = sum / (float)count;
        m_fps = (m_frameTimeMs > 0.001f) ? (1000.0f / m_frameTimeMs) : 0.0f;
    }

    if (++m_memUpdateCount >= 60) {
        m_memUpdateCount = 0;
        updateMemoryUsage();
    }
}

void PerformanceMetrics::updateMemoryUsage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        m_memoryMB = static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
    }
#else
    
    m_memoryMB = 0.0f;
#endif
}

} 
