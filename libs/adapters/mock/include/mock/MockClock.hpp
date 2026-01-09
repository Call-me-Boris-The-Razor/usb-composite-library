/**
 * @file MockClock.hpp
 * @brief Mock реализация IClock для тестов
 */

#pragma once

#include "ports/IClock.hpp"

namespace usb::mock {

/**
 * @brief Mock часы для unit тестов
 * 
 * Позволяет контролировать время в тестах.
 */
class MockClock : public ports::IClock {
public:
    [[nodiscard]] uint32_t GetTickMs() const override {
        return current_tick_ms_;
    }
    
    void DelayMs(uint32_t ms) override {
        current_tick_ms_ += ms;
        delay_calls_++;
        last_delay_ms_ = ms;
    }
    
    // Test helpers
    void SetTick(uint32_t tick_ms) { current_tick_ms_ = tick_ms; }
    void AdvanceTick(uint32_t delta_ms) { current_tick_ms_ += delta_ms; }
    
    uint32_t GetDelayCallCount() const { return delay_calls_; }
    uint32_t GetLastDelayMs() const { return last_delay_ms_; }
    
    void Reset() {
        current_tick_ms_ = 0;
        delay_calls_ = 0;
        last_delay_ms_ = 0;
    }
    
private:
    uint32_t current_tick_ms_ = 0;
    uint32_t delay_calls_ = 0;
    uint32_t last_delay_ms_ = 0;
};

}  // namespace usb::mock
