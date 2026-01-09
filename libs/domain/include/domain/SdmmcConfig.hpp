/**
 * @file SdmmcConfig.hpp
 * @brief Конфигурация SDMMC без HAL зависимостей
 * 
 * Платформо-независимые настройки для SDMMC.
 */

#pragma once

#include <cstdint>

namespace usb::domain {

/**
 * @brief Конфигурация GPIO пина (без HAL типов)
 */
struct GpioPinConfig {
    uint8_t port_index;   ///< Индекс порта (0=A, 1=B, 2=C, 3=D, ...)
    uint8_t pin_number;   ///< Номер пина (0-15)
};

/**
 * @brief Конфигурация SDMMC (без HAL типов)
 * 
 * Все значения — платформо-независимые.
 * Адаптер переводит их в HAL-специфичные типы.
 */
struct SdmmcConfig {
    // SDMMC Instance
    uint8_t sdmmc_index = 1;          ///< 1 = SDMMC1, 2 = SDMMC2
    
    // GPIO пины (стандартная распиновка SDMMC1)
    GpioPinConfig clk = {2, 12};      ///< PC12
    GpioPinConfig cmd = {3, 2};       ///< PD2
    GpioPinConfig d0 = {2, 8};        ///< PC8
    GpioPinConfig d1 = {2, 9};        ///< PC9
    GpioPinConfig d2 = {2, 10};       ///< PC10
    GpioPinConfig d3 = {2, 11};       ///< PC11
    
    // Режим шины
    bool use_4bit_mode = true;
    
    // Clock dividers
    // SDMMC_CK = SDMMCCLK / (CLKDIV + 2)
    // При SDMMCCLK = 240MHz:
    //   init_clock_div = 598 -> 400kHz
    //   normal_clock_div = 8 -> 24MHz
    uint32_t init_clock_div = 598;
    uint32_t normal_clock_div = 8;
    
    // Таймауты (мс)
    uint32_t init_timeout_ms = 2000;
    uint32_t rw_timeout_ms = 2000;
    uint32_t ready_timeout_ms = 500;
};

/**
 * @brief Информация о SD карте (без HAL типов)
 */
struct SdmmcCardInfo {
    uint32_t block_count = 0;       ///< Количество блоков (512 байт)
    uint32_t block_size = 512;      ///< Размер блока
    uint64_t capacity_bytes = 0;    ///< Полная ёмкость в байтах
    uint32_t card_type = 0;         ///< Тип карты
    uint32_t card_version = 0;      ///< Версия карты
    bool is_ready = false;          ///< Готова к операциям
};

/**
 * @brief Диагностика SDMMC
 */
struct SdmmcDiagnostics {
    uint32_t hal_state = 0;
    uint32_t hal_error = 0;
    uint32_t sdmmc_sta = 0;
    uint32_t sdmmc_resp1 = 0;
};

// ============ Пресеты для популярных плат ============

namespace presets {

/// DevEBox H743 / WeAct H743 / OkoRelay (стандартная распиновка SDMMC1)
inline SdmmcConfig Stm32H7Standard() {
    return SdmmcConfig{};  // Значения по умолчанию
}

/// Алиас для OkoRelay
inline SdmmcConfig OkoRelay() {
    return Stm32H7Standard();
}

}  // namespace presets

}  // namespace usb::domain
