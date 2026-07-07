// OpenMeshOS — HeapMonitor.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Periodic heap and stack diagnostics for long-running embedded firmware.
// Uses ESP-IDF heap_info and FreeRTOS task APIs. No Arduino String.

#include "HeapMonitor.h"
#include "../utils/Log.h"
#include "../hardware/PlatformCompat.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Esp.h>

namespace oms {

HeapMonitor& HeapMonitor::instance() {
    static HeapMonitor inst;
    return inst;
}

void HeapMonitor::init(uint32_t logIntervalSec,
                        uint32_t warnHeapBytes,
                        uint32_t criticalHeapBytes) {
    _logIntervalSec = logIntervalSec;
    _warnHeapBytes = warnHeapBytes;
    _criticalHeapBytes = criticalHeapBytes;
    _lastLogMs = millis();
    _initialized = true;

    // Log initial state
    OMS_LOG("Heap", "Monitor started: free=%u min=%u interval=%lus warn=%u crit=%u",
            (unsigned)freeHeap(), (unsigned)minFreeHeap(),
            (unsigned long)_logIntervalSec,
            (unsigned)_warnHeapBytes, (unsigned)_criticalHeapBytes);

    // Log initial task stack HWMs
    logAllStackHWM();
}

void HeapMonitor::tick() {
    if (!_initialized) return;

    uint32_t free = freeHeap();

    // Immediate critical alert (every time, no debounce)
    if (free < _criticalHeapBytes) {
        OMS_LOG("Heap", "CRITICAL: free heap %u bytes (below %u threshold) — EMERGENCY REBOOT",
                (unsigned)free, (unsigned)_criticalHeapBytes);
        logAllStackHWM();

        // Heap watchdog: force reboot to prevent undefined behaviour.
        // Wait 500ms for log flush, then restart.
        delay(500);
        ESP.restart();
    }

    // Immediate warning (debounced via periodic log interval)
    if (free < _warnHeapBytes) {
        OMS_LOG("Heap", "WARNING: free heap %u bytes (below %u threshold)",
                (unsigned)free, (unsigned)_warnHeapBytes);
    }

    // Periodic log at configured interval
    uint32_t now = millis();
    if (now - _lastLogMs >= _logIntervalSec * 1000) {
        _lastLogMs = now;
        OMS_LOG("Heap", "free=%u min=%u largestBlock=%u",
                (unsigned)free, (unsigned)minFreeHeap(),
                (unsigned)platform::largestFreeBlock());

        // Also log task stacks with periodic heap report
        logAllStackHWM();
    }
}

uint32_t HeapMonitor::freeHeap() const {
    return platform::freeHeap();
}

uint32_t HeapMonitor::minFreeHeap() const {
    return platform::minFreeHeap();
}

uint32_t HeapMonitor::stackHWM(TaskHandle_t task) const {
    return uxTaskGetStackHighWaterMark(task);
}

void HeapMonitor::logAllStackHWM() const {
    // Log stack high-water marks for known tasks.
    // We avoid vTaskList() because it requires configUSE_TRACE_FACILITY
    // and configUSE_STATS_FORMATTING_FUNCTIONS, which may not be enabled
    // in all build configurations. Instead, we query tasks by handle.

    // Current task (loop task)
    OMS_LOG("Stack", "loop HWM=%u", (unsigned)uxTaskGetStackHighWaterMark(nullptr));

    // Try known task names. If the task doesn't exist, handle is null.
    static const char* const s_knownTasks[] = {
        "loop1",     // Arduino loop task (second core)
        "lv_timer",  // LVGL timer task
        "tiT",       // ESP-IDF timer service task
        "IDLE0",     // Idle task core 0
        "IDLE1",     // Idle task core 1
    };

    for (const char* name : s_knownTasks)
    {
        TaskHandle_t h = xTaskGetHandle(name);
        if (h)
        {
            OMS_LOG("Stack", "  %s HWM=%u", name,
                     (unsigned)uxTaskGetStackHighWaterMark(h));
        }
    }
}

}  // namespace oms