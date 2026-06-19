// OpenMeshOS — HeapMonitor.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Periodic heap and stack diagnostics for long-running embedded firmware.
// Logs free heap, min free heap (watermark), and task stack high-water
// marks every N seconds. Also provides a programmatic health check.

#pragma once

#include <Arduino.h>

namespace oms {

/// Heap and stack diagnostics. Call tick() in loop() to get periodic logs.
/// All thresholds are configurable via init().
class HeapMonitor {
public:
    static HeapMonitor& instance();

    /// Initialise with custom thresholds.
    /// \param logIntervalSec  How often to log diagnostics (default 60s)
    /// \param warnHeapBytes   Log a warning if free heap drops below this
    /// \param criticalHeapBytes  Log a critical alert if free heap below this
    void init(uint32_t logIntervalSec = 60,
              uint32_t warnHeapBytes = 30000,
              uint32_t criticalHeapBytes = 15000);

    /// Call in loop(). Logs diagnostics at the configured interval and
    /// emits immediate warnings when heap drops below thresholds.
    void tick();

    /// Get current free heap in bytes.
    uint32_t freeHeap() const;

    /// Get the minimum free heap ever seen (low watermark since boot).
    uint32_t minFreeHeap() const;

    /// Get stack high-water mark for a given task (0 = current task).
    /// Returns bytes remaining. Smaller = closer to overflow.
    uint32_t stackHWM(TaskHandle_t task = nullptr) const;

    /// Log all task stack HWMs immediately.
    void logAllStackHWM() const;

    /// Check if heap is healthy (above warning threshold).
    bool isHealthy() const { return freeHeap() >= _warnHeapBytes; }

private:
    HeapMonitor() = default;
    uint32_t _logIntervalSec = 60;
    uint32_t _warnHeapBytes = 30000;
    uint32_t _criticalHeapBytes = 15000;
    uint32_t _lastLogMs = 0;
    bool _initialized = false;
};

}  // namespace oms