/**
 * @file usb_sdmmc.h
 * @brief SDMMC Block Device для USB MSC
 * 
 * Реализация IBlockDevice для SD/SDHC/SDXC карт через SDMMC1 STM32H7.
 * Включает:
 * - Автоматическую инициализацию GPIO и HAL_SD
 * - Sector Translation Layer для SD NAND (1024-байт физ. секторы)
 * - DMA bounce buffers в RAM_D2
 * - Пресеты для плат (OkoRelay, DevEBox H743)
 * 
 * Использование:
 * ```cpp
 * usb::SdmmcBlockDevice g_sd;
 * g_sd.Init(usb::presets::OkoRelay());
 * g_usb.MscAttach(&g_sd);
 * ```
 */

#pragma once

#include "usb_composite.h"

#if defined(USB_MSC_ENABLED) && defined(USB_SDMMC_ENABLED)

#include "stm32h7xx_hal.h"
#include <cstdint>

namespace usb {

// ============ Конфигурация ============

/**
 * @brief Конфигурация SDMMC
 */
struct SdmmcConfig {
    // GPIO CLK (по умолчанию PC12)
    GPIO_TypeDef* clk_port = GPIOC;
    uint16_t clk_pin = GPIO_PIN_12;
    
    // GPIO CMD (по умолчанию PD2)
    GPIO_TypeDef* cmd_port = GPIOD;
    uint16_t cmd_pin = GPIO_PIN_2;
    
    // GPIO D0-D3 (по умолчанию PC8-PC11)
    GPIO_TypeDef* data_port = GPIOC;
    uint16_t d0_pin = GPIO_PIN_8;
    uint16_t d1_pin = GPIO_PIN_9;
    uint16_t d2_pin = GPIO_PIN_10;
    uint16_t d3_pin = GPIO_PIN_11;
    
    // Alternate Function (по умолчанию AF12 для SDMMC1)
    uint8_t alternate_function = GPIO_AF12_SDMMC1;
    
    // SDMMC Instance
    SDMMC_TypeDef* instance = SDMMC1;
    
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
    uint32_t block_count;       ///< Количество блоков (512 байт)
    uint32_t block_size;        ///< Размер блока (всегда 512)
    uint64_t capacity_bytes;    ///< Полная ёмкость в байтах
    uint32_t card_type;         ///< Тип карты (SD_CARD, SDHC_CARD, etc.)
    uint32_t card_version;      ///< Версия карты
    bool is_ready;              ///< Готова к операциям
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
    uint32_t hal_state;      ///< HAL state
    uint32_t hal_error;      ///< HAL error code
    uint32_t sdmmc_sta;      ///< SDMMC STA регистр
    uint32_t sdmmc_resp1;    ///< RESP1
};

// ============ Класс SdmmcBlockDevice ============

/**
 * @brief SDMMC Block Device для USB MSC
 * 
 * Реализует интерфейс IBlockDevice для работы с SD картами.
 * Поддерживает SD NAND с физическими секторами 1024 байт.
 */
class SdmmcBlockDevice : public IBlockDevice {
public:
    SdmmcBlockDevice() = default;
    ~SdmmcBlockDevice();
    
    SdmmcBlockDevice(const SdmmcBlockDevice&) = delete;
    SdmmcBlockDevice& operator=(const SdmmcBlockDevice&) = delete;
    
    /// Размер логического блока (всегда 512)
    static constexpr uint32_t kBlockSize = 512;
    
    // ============ Инициализация ============
    
    /**
     * @brief Инициализация SDMMC
     * @param config Конфигурация (по умолчанию OkoRelay)
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
    SdmmcCardInfo GetCardInfo() const { return card_info_; }
    
    /**
     * @brief Получить состояние
     */
    SdmmcState GetState() const { return state_; }
    
    /**
     * @brief Получить диагностику
     */
    SdmmcDiagnostics GetDiagnostics() const;
    
    /**
     * @brief Синхронизация (сброс кэша на диск)
     * @return true если успешно
     */
    bool Sync();
    
    /**
     * @brief Получить HAL handle (для продвинутого использования)
     */
    SD_HandleTypeDef* GetHandle() { return &hsd_; }
    
    // ============ IBlockDevice ============
    
    bool IsReady() const override;
    uint32_t GetBlockCount() const override;
    uint32_t GetBlockSize() const override;
    bool Read(uint32_t lba, uint8_t* buffer, uint32_t count) override;
    bool Write(uint32_t lba, const uint8_t* buffer, uint32_t count) override;

private:
    void InitGpio();
    void DeInitGpio();
    bool WaitReady(uint32_t timeout_ms);
    
    // Sector Translation Layer
    bool ReadDirect(uint32_t lba, uint8_t* buffer, uint32_t count);
    bool WriteDirect(uint32_t lba, const uint8_t* buffer, uint32_t count);
    bool FlushCache();
    
    SD_HandleTypeDef hsd_ = {};
    SdmmcConfig config_ = {};
    SdmmcState state_ = SdmmcState::NotInitialized;
    SdmmcCardInfo card_info_ = {};
    
    /// Физический размер сектора (512 или 1024 для SD NAND)
    uint32_t phys_block_size_ = 1024;
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
