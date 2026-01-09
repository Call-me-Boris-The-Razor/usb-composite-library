/**
 * @file usb_adapters.h
 * @brief Примеры адаптеров для интеграции usb_composite с проектом
 * 
 * ВАЖНО: Этот файл — ШАБЛОН для копирования в ваш проект!
 * 
 * Адаптеры требуют типы из вашего проекта (IDebugOutput, SdDisk, Status и т.д.),
 * которые не являются частью библиотеки usb_composite.
 * 
 * Использование:
 * 1. Скопируйте нужный адаптер в свой проект
 * 2. Адаптируйте под свои типы
 * 3. Подключите свои заголовки
 */

#pragma once

#include "usb_composite.h"
#include <cstdarg>
#include <cstdio>
#include <cstdint>

namespace usb {

//--------------------------------------------------------------------+
// Пример 1: Простой адаптер для отладочного вывода
//--------------------------------------------------------------------+

/**
 * @brief Базовый интерфейс отладочного вывода (пример)
 * 
 * Замените на ваш интерфейс из проекта или используйте как есть.
 */
struct IDebugOutput {
    virtual ~IDebugOutput() = default;
    virtual bool Print(const char* str) = 0;
    virtual bool Printf(const char* fmt, ...) = 0;
    virtual bool Write(const uint8_t* data, size_t length) = 0;
    virtual bool IsReady() const = 0;
};

/**
 * @brief Адаптер USB CDC → IDebugOutput
 * 
 * Позволяет использовать USB CDC как отладочный вывод.
 * 
 * Пример:
 * ```cpp
 * usb::UsbDevice g_usb;
 * usb::UsbDebugAdapter g_usb_debug(&g_usb);
 * ```
 */
class UsbDebugAdapter : public IDebugOutput {
public:
    explicit UsbDebugAdapter(UsbDevice* usb) : usb_(usb) {}
    
    bool Print(const char* str) override {
        if (!usb_ || !usb_->CdcTerminalOpened()) {
            return false;
        }
        usb_->CdcWrite(str);
        return true;
    }
    
    bool Printf(const char* fmt, ...) override {
        if (!usb_ || !usb_->CdcTerminalOpened()) {
            return false;
        }
        
        char buf[256];
        va_list args;
        va_start(args, fmt);
        int len = vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        
        if (len > 0) {
            usb_->CdcWrite(reinterpret_cast<const uint8_t*>(buf), 
                          static_cast<uint32_t>(len));
        }
        return true;
    }
    
    bool Write(const uint8_t* data, size_t length) override {
        if (!usb_ || !usb_->CdcTerminalOpened()) {
            return false;
        }
        usb_->CdcWrite(data, static_cast<uint32_t>(length));
        return true;
    }
    
    bool IsReady() const override {
        return usb_ && usb_->CdcTerminalOpened();
    }

private:
    UsbDevice* usb_;
};

//--------------------------------------------------------------------+
// Пример 2: Шаблон адаптера для блочного устройства
//--------------------------------------------------------------------+

/**
 * @brief Шаблонный адаптер для любого блочного устройства
 * 
 * Требования к типу T:
 * - bool IsReady() const
 * - uint32_t GetBlockCount() const
 * - uint32_t GetBlockSize() const  (или constexpr kBlockSize)
 * - bool Read(uint32_t lba, uint8_t* buf, uint32_t cnt)
 * - bool Write(uint32_t lba, const uint8_t* buf, uint32_t cnt)
 * 
 * Пример:
 * ```cpp
 * MySdDriver g_sd;
 * usb::BlockDeviceAdapter<MySdDriver> g_sd_adapter(&g_sd);
 * g_usb.MscAttach(&g_sd_adapter);
 * ```
 */
template<typename T>
class BlockDeviceAdapter : public IBlockDevice {
public:
    explicit BlockDeviceAdapter(T* device) : device_(device) {}
    
    bool IsReady() const override {
        return device_ && device_->IsReady();
    }
    
    uint32_t GetBlockCount() const override {
        if (!device_) return 0;
        return device_->GetBlockCount();
    }
    
    uint32_t GetBlockSize() const override {
        return 512;  // Стандартный размер блока
    }
    
    bool Read(uint32_t lba, uint8_t* buffer, uint32_t count) override {
        if (!device_) return false;
        return device_->Read(lba, buffer, count);
    }
    
    bool Write(uint32_t lba, const uint8_t* buffer, uint32_t count) override {
        if (!device_) return false;
        return device_->Write(lba, buffer, count);
    }

private:
    T* device_;
};

}  // namespace usb

