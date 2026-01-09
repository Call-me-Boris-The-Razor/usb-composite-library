/**
 * @file IClock.hpp
 * @brief Интерфейс системного времени
 * 
 * Абстракция для получения времени и задержек.
 * Не содержит HAL/платформенных зависимостей.
 */

#pragma once

#include <cstdint>

namespace usb::ports {

/**
 * @brief Интерфейс системных часов
 * 
 * Контракт:
 * - GetTickMs() монотонно возрастает (с переполнением через ~49 дней)
 * - DelayMs() блокирует вызывающий поток
 */
struct IClock {
    virtual ~IClock() = default;
    
    /// Получить текущее время в миллисекундах
    [[nodiscard]] virtual uint32_t GetTickMs() const = 0;
    
    /// Блокирующая задержка в миллисекундах
    virtual void DelayMs(uint32_t ms) = 0;
    
    // Запрет копирования
    IClock(const IClock&) = delete;
    IClock& operator=(const IClock&) = delete;
    
protected:
    IClock() = default;
};

}  // namespace usb::ports
