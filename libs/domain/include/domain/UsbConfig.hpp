/**
 * @file UsbConfig.hpp
 * @brief Конфигурация USB без HAL зависимостей
 */

#pragma once

#include <cstdint>

namespace usb::domain {

/**
 * @brief Конфигурация USB устройства (без HAL типов)
 */
struct UsbConfig {
    /// VID устройства (по умолчанию ST Microelectronics)
    uint16_t vid = 0x0483;
    
    /// PID устройства (0x5743 = CDC+MSC Composite)
    uint16_t pid = 0x5743;
    
    /// Строка производителя
    const char* manufacturer = "STM32";
    
    /// Строка продукта
    const char* product = "USB Composite";
    
    /// Серийный номер (nullptr = использовать UID чипа)
    const char* serial = nullptr;
    
    /// Пин D+ для toggle (port_index, pin_number). {0xFF, 0} = не использовать
    uint8_t dp_port_index = 0xFF;
    uint8_t dp_pin_number = 0;
    
    /// Длительность toggle D+ в мс (0 = не делать toggle)
    uint32_t dp_toggle_ms = 10;
};

/**
 * @brief Состояние USB устройства
 */
enum class UsbState : uint8_t {
    NotInitialized = 0,
    Disconnected = 1,
    Connected = 2,
    Configured = 3,
    Suspended = 4,
};

/**
 * @brief Диагностика USB инициализации
 */
struct UsbDiagnostics {
    bool tusb_init_ok = false;
    uint32_t usb_base_addr = 0;
    uint32_t gccfg = 0;
    uint32_t gotgctl = 0;
};

}  // namespace usb::domain
