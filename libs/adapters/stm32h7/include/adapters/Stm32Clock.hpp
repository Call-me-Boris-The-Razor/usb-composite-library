/**
 * @file Stm32Clock.hpp
 * @brief STM32 HAL реализация IClock
 */

#pragma once

#include "ports/IClock.hpp"
#include "stm32h7xx_hal.h"

namespace usb::adapters {

/**
 * @brief STM32 HAL реализация системных часов
 */
class Stm32Clock final : public ports::IClock {
public:
    [[nodiscard]] uint32_t GetTickMs() const override {
        return HAL_GetTick();
    }
    
    void DelayMs(uint32_t ms) override {
        HAL_Delay(ms);
    }
};

}  // namespace usb::adapters
