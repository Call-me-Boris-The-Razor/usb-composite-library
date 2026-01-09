/**
 * @file IBlockDevice.hpp
 * @brief Интерфейс блочного устройства хранения
 * 
 * Абстракция для любого блочного устройства (SD, eMMC, Flash, RAM disk).
 * Не содержит HAL/платформенных зависимостей.
 */

#pragma once

#include <cstdint>

namespace usb::ports {

/**
 * @brief Интерфейс блочного устройства
 * 
 * Контракт:
 * - Размер блока фиксирован после инициализации
 * - Read/Write атомарны для одного блока
 * - Потокобезопасность: реализация должна обеспечить если нужно
 */
struct IBlockDevice {
    virtual ~IBlockDevice() = default;
    
    /// Проверка готовности устройства к операциям
    [[nodiscard]] virtual bool IsReady() const = 0;
    
    /// Получить количество блоков
    [[nodiscard]] virtual uint32_t GetBlockCount() const = 0;
    
    /// Получить размер блока в байтах (обычно 512)
    [[nodiscard]] virtual uint32_t GetBlockSize() const = 0;
    
    /**
     * @brief Чтение блоков
     * @param lba Логический адрес первого блока
     * @param buffer Буфер для данных (размер >= count * GetBlockSize())
     * @param count Количество блоков для чтения
     * @return true если успешно
     */
    virtual bool Read(uint32_t lba, uint8_t* buffer, uint32_t count) = 0;
    
    /**
     * @brief Запись блоков
     * @param lba Логический адрес первого блока
     * @param buffer Данные для записи (размер >= count * GetBlockSize())
     * @param count Количество блоков для записи
     * @return true если успешно
     */
    virtual bool Write(uint32_t lba, const uint8_t* buffer, uint32_t count) = 0;
    
    /**
     * @brief Синхронизация (сброс кэшей на носитель)
     * @return true если успешно
     */
    virtual bool Sync() { return true; }
    
    // Запрет копирования
    IBlockDevice(const IBlockDevice&) = delete;
    IBlockDevice& operator=(const IBlockDevice&) = delete;
    
protected:
    IBlockDevice() = default;
};

}  // namespace usb::ports
