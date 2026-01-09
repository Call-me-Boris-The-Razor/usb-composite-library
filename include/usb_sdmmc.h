/**
 * @file usb_sdmmc.h
 * @brief SDMMC Block Device для USB MSC
 * 
 * Реализация IBlockDevice для SD/SDHC/SDXC карт через SDMMC1 STM32H7.
 * 
 * @note Публичный API не содержит HAL зависимостей (v3.0.0+)
 * 
 * Использование:
 * ```cpp
 * usb::SdmmcBlockDevice g_sd;
 * g_sd.Init(usb::SdmmcConfig{});
 * g_usb.MscAttach(&g_sd);
 * ```
 */

#pragma once

#include "usb_composite.h"

#if defined(USB_MSC_ENABLED) && defined(USB_SDMMC_ENABLED)

#include <cstdint>

namespace usb {

// ============ Конфигурация (без HAL типов!) ============

/**
 * @brief Конфигурация GPIO пина
 */
struct GpioPinConfig {
    uint8_t port_index;   ///< Индекс порта (0=A, 1=B, 2=C, 3=D, ...)
    uint8_t pin_number;   ///< Номер пина (0-15)
};

/**
 * @brief Конфигурация SDMMC (без HAL зависимостей)
 */
struct SdmmcConfig {
    // SDMMC Instance (1 = SDMMC1, 2 = SDMMC2)
    uint8_t sdmmc_index = 1;
    
    // GPIO пины (стандартная распиновка SDMMC1)
    GpioPinConfig clk = {2, 12};      ///< PC12
    GpioPinConfig cmd = {3, 2};       ///< PD2
    GpioPinConfig d0 = {2, 8};        ///< PC8
    GpioPinConfig d1 = {2, 9};        ///< PC9
    GpioPinConfig d2 = {2, 10};       ///< PC10
    GpioPinConfig d3 = {2, 11};       ///< PC11
    
    // Режим шины
    bool use_4bit_mode = true;
    
    // Clock dividers (для 240MHz kernel clock)
    // SDMMC_CK = SDMMCCLK / (CLKDIV + 2)
    uint32_t init_clock_div = 598;    ///< 400kHz для инициализации
    uint32_t normal_clock_div = 8;    ///< 24MHz для нормальной работы
    
    // Таймауты (мс)
    uint32_t init_timeout_ms = 2000;
    uint32_t rw_timeout_ms = 2000;
    uint32_t ready_timeout_ms = 500;
};

/**
 * @brief Информация о карте
 */
struct SdmmcCardInfo {
    uint32_t block_count = 0;       ///< Количество блоков (512 байт)
    uint32_t block_size = 512;      ///< Размер блока (всегда 512)
    uint64_t capacity_bytes = 0;    ///< Полная ёмкость в байтах
    uint32_t card_type = 0;         ///< Тип карты
    uint32_t card_version = 0;      ///< Версия карты
    bool is_ready = false;          ///< Готова к операциям
};

/**
 * @brief Состояние SDMMC
 */
enum class SdmmcState : uint8_t {
    NotInitialized,   ///< Не инициализирован
    Ready,            ///< Готов к операциям
    Busy,             ///< Занят операцией
    Error,            ///< Ошибка
};

/**
 * @brief Диагностическая информация
 */
struct SdmmcDiagnostics {
    uint32_t hal_state = 0;      ///< HAL state
    uint32_t hal_error = 0;      ///< HAL error code
    uint32_t sdmmc_sta = 0;      ///< SDMMC STA регистр
    uint32_t sdmmc_resp1 = 0;    ///< RESP1
};

// ============ Класс SdmmcBlockDevice ============

// Forward declaration для pImpl
struct SdmmcImpl;

/**
 * @brief SDMMC Block Device для USB MSC
 * 
 * Реализует интерфейс IBlockDevice для работы с SD картами.
 * 
 * @note HAL детали скрыты через pImpl паттерн (v3.0.0+)
 */
class SdmmcBlockDevice : public IBlockDevice {
public:
    SdmmcBlockDevice();
    ~SdmmcBlockDevice();
    
    SdmmcBlockDevice(const SdmmcBlockDevice&) = delete;
    SdmmcBlockDevice& operator=(const SdmmcBlockDevice&) = delete;
    
    /// Размер логического блока (всегда 512)
    static constexpr uint32_t kBlockSize = 512;
    
    // ============ Инициализация ============
    
    /**
     * @brief Инициализация SDMMC
     * @param config Конфигурация
     * @return true если успешно
     */
    bool Init(const SdmmcConfig& config = SdmmcConfig{});
    
    /**
     * @brief Деинициализация
     */
    void DeInit();
    
    // ============ Статус ============
    
    /**
     * @brief Проверка наличия карты
     * @return true если карта вставлена и готова
     */
    bool IsCardInserted() const;
    
    /**
     * @brief Получить информацию о карте
     */
    SdmmcCardInfo GetCardInfo() const;
    
    /**
     * @brief Получить состояние
     */
    SdmmcState GetState() const;
    
    /**
     * @brief Получить диагностику
     */
    SdmmcDiagnostics GetDiagnostics() const;
    
    /**
     * @brief Синхронизация (сброс кэша на диск)
     * @return true если успешно
     */
    bool Sync();
    
    // ============ IBlockDevice ============
    
    bool IsReady() const override;
    uint32_t GetBlockCount() const override;
    uint32_t GetBlockSize() const override;
    bool Read(uint32_t lba, uint8_t* buffer, uint32_t count) override;
    bool Write(uint32_t lba, const uint8_t* buffer, uint32_t count) override;

private:
    SdmmcImpl* impl_;  ///< pImpl для скрытия HAL деталей
};

// ============ Пресеты для плат ============

namespace presets {

/**
 * @brief Конфигурация для платы OkoRelay
 * 
 * Пины: PC8-PC11 (D0-D3), PC12 (CLK), PD2 (CMD)
 * SDMMC1, 4-bit mode, AF12
 */
inline SdmmcConfig OkoRelay() {
    SdmmcConfig cfg;
    // Все значения по умолчанию соответствуют OkoRelay
    return cfg;
}

/**
 * @brief Конфигурация для DevEBox STM32H743
 * 
 * Идентична OkoRelay (стандартная распиновка SDMMC1)
 */
inline SdmmcConfig DevEBoxH743() {
    return OkoRelay();
}

/**
 * @brief Конфигурация для WeAct Studio H743
 * 
 * Идентична OkoRelay (стандартная распиновка SDMMC1)
 */
inline SdmmcConfig WeActH743() {
    return OkoRelay();
}

}  // namespace presets

}  // namespace usb

#endif  // USB_MSC_ENABLED && USB_SDMMC_ENABLED
