/**
 * @file usb_composite.h
 * @brief USB Composite Device (CDC + MSC) — публичный API
 * 
 * Библиотека для работы с USB на STM32H7:
 * - CDC: Virtual COM Port (логи, диагностика, CLI)
 * - MSC: Mass Storage (SD/eMMC как флешка)
 * 
 * Активация через флаги компиляции:
 * - USB_CDC_ENABLED: включает CDC (COM порт)
 * - USB_MSC_ENABLED: включает MSC (флешка)
 * 
 * @note Требует TinyUSB (подключается отдельно)
 * @note Для некоторых плат требуется toggle D+ пина для запуска USB
 * 
 * @example
 * ```cpp
 * #include "usb_composite.h"
 * 
 * usb::UsbDevice g_usb;
 * 
 * int main() {
 *     // Конфигурация
 *     usb::Config cfg;
 *     cfg.dp_toggle_pin = {GPIOA, 12};  // PA12 = D+
 *     cfg.dp_toggle_ms = 10;
 *     
 *     g_usb.Init(cfg);
 *     g_usb.Start();
 *     
 *     while(1) {
 *         g_usb.Process();
 *         
 *         // CDC: вывод логов
 *         if (g_usb.CdcIsConnected()) {
 *             g_usb.CdcWrite("Hello\n", 6);
 *         }
 *     }
 * }
 * ```
 */

#pragma once

#include <cstdint>
#include <cstddef>

// Проверка что хотя бы один модуль включён
#if !defined(USB_CDC_ENABLED) && !defined(USB_MSC_ENABLED)
#warning "USB Composite: ни CDC, ни MSC не включены. Определите USB_CDC_ENABLED и/или USB_MSC_ENABLED"
#endif

namespace usb {

//--------------------------------------------------------------------+
// Типы и константы
//--------------------------------------------------------------------+

/// Состояние USB устройства
enum class State : uint8_t {
    NotInitialized = 0,  ///< Не инициализировано
    Disconnected   = 1,  ///< Отключено от хоста
    Connected      = 2,  ///< Подключено к хосту
    Configured     = 3,  ///< Сконфигурировано хостом
    Suspended      = 4,  ///< Приостановлено (suspend)
};

/// Конфигурация пина GPIO
struct GpioPin {
    void* port;          ///< GPIO порт (например, GPIOA)
    uint16_t pin;        ///< Номер пина (0-15)
};

/// Интерфейс блочного устройства для MSC
struct IBlockDevice {
    virtual ~IBlockDevice() = default;
    
    /// Проверка готовности
    virtual bool IsReady() const = 0;
    
    /// Получить количество блоков
    virtual uint32_t GetBlockCount() const = 0;
    
    /// Получить размер блока (обычно 512)
    virtual uint32_t GetBlockSize() const = 0;
    
    /// Чтение блоков
    /// @param lba Логический адрес блока
    /// @param buffer Буфер для данных
    /// @param count Количество блоков
    /// @return true если успешно
    virtual bool Read(uint32_t lba, uint8_t* buffer, uint32_t count) = 0;
    
    /// Запись блоков
    /// @param lba Логический адрес блока
    /// @param buffer Данные для записи
    /// @param count Количество блоков
    /// @return true если успешно
    virtual bool Write(uint32_t lba, const uint8_t* buffer, uint32_t count) = 0;
};

/// Конфигурация USB устройства
struct Config {
    /// Пин D+ для ручного переподключения (опционально)
    /// Если port == nullptr, toggle не выполняется
    GpioPin dp_toggle_pin = {nullptr, 0};
    
    /// Длительность toggle D+ в мс (0 = не делать toggle)
    uint32_t dp_toggle_ms = 10;
    
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
};

//--------------------------------------------------------------------+
// Callback типы
//--------------------------------------------------------------------+

#ifdef USB_CDC_ENABLED
/// Callback при получении данных CDC
/// @param data Указатель на данные
/// @param len Длина данных
using CdcRxCallback = void(*)(const uint8_t* data, uint32_t len, void* context);

/// Callback при изменении line coding (baudrate и т.д.)
/// @param baudrate Новый baudrate
using CdcLineCodingCallback = void(*)(uint32_t baudrate, void* context);

/// Callback для DFU jump (вызывается при baudrate 1200)
/// Должен выполнить переход в bootloader
using DfuJumpCallback = void(*)(void* context);

/// Магический baudrate для DFU (1200 bps touch)
static constexpr uint32_t kDfuBaudrate = 1200;
#endif

//--------------------------------------------------------------------+
// Основной класс USB устройства
//--------------------------------------------------------------------+

/**
 * @brief USB Composite Device
 * 
 * Контракты:
 * - Init() вызывается один раз при старте
 * - Start() после Init() для запуска USB
 * - Process() вызывать в main loop
 */
class UsbDevice {
public:
    UsbDevice() = default;
    ~UsbDevice() = default;
    
    // Запрет копирования
    UsbDevice(const UsbDevice&) = delete;
    UsbDevice& operator=(const UsbDevice&) = delete;
    
    //----------------------------------------------------------------+
    // Основные методы
    //----------------------------------------------------------------+
    
    /// Инициализация USB
    /// @param config Конфигурация устройства
    /// @return true если успешно
    bool Init(const Config& config = Config{});
    
    /// Запуск USB (подключение к хосту)
    /// Выполняет toggle D+ если настроено
    bool Start();
    
    /// Остановка USB
    void Stop();
    
    /// Обработка USB (вызывать в main loop)
    void Process();
    
    /// Проверить инициализацию
    bool IsInitialized() const { return initialized_; }
    
    /// Проверить подключение к хосту
    bool IsConnected() const;
    
    /// Получить текущее состояние
    State GetState() const;
    
    //----------------------------------------------------------------+
    // CDC методы (только если USB_CDC_ENABLED)
    //----------------------------------------------------------------+
    
#ifdef USB_CDC_ENABLED
    /// Проверить подключение CDC (терминал открыт)
    bool CdcIsConnected() const;
    
    /// Записать данные в CDC
    /// @param data Данные
    /// @param len Длина
    /// @return Количество записанных байт
    uint32_t CdcWrite(const uint8_t* data, uint32_t len);
    
    /// Записать строку в CDC
    uint32_t CdcWrite(const char* str);
    
    /// Записать с форматированием (printf-style)
    uint32_t CdcPrintf(const char* fmt, ...);
    
    /// Прочитать данные из CDC
    /// @param buffer Буфер для данных
    /// @param max_len Максимальная длина
    /// @return Количество прочитанных байт
    uint32_t CdcRead(uint8_t* buffer, uint32_t max_len);
    
    /// Проверить наличие данных для чтения
    uint32_t CdcAvailable() const;
    
    /// Очистить буфер приёма
    void CdcFlushRx();
    
    /// Установить callback получения данных
    void CdcSetRxCallback(CdcRxCallback callback, void* context = nullptr);
    
    /// Установить callback изменения line coding
    void CdcSetLineCodingCallback(CdcLineCodingCallback callback, void* context = nullptr);
    
    /// Установить callback для DFU jump (при baudrate 1200)
    /// @param callback Функция перехода в bootloader (например, utils::ScheduleBootloaderJump)
    void CdcSetDfuCallback(DfuJumpCallback callback, void* context = nullptr);
    
    /// Проверить, открыт ли терминал (получен SET_LINE_CODING с baudrate != 1200)
    /// Более надёжный индикатор чем CdcIsConnected() (DTR)
    bool CdcTerminalOpened() const;
    
    /// Сброс флага терминала (при переподключении USB)
    void CdcResetTerminalFlag();
#endif

    //----------------------------------------------------------------+
    // MSC методы (только если USB_MSC_ENABLED)
    //----------------------------------------------------------------+
    
#ifdef USB_MSC_ENABLED
    /// Подключить блочное устройство к MSC
    /// @param device Указатель на устройство (должен жить пока подключён)
    void MscAttach(IBlockDevice* device);
    
    /// Отключить блочное устройство
    void MscDetach();
    
    /// Проверить занятость MSC (идёт операция чтения/записи)
    bool MscIsBusy() const;
    
    /// Проверить подключение MSC устройства
    bool MscIsAttached() const { return msc_device_ != nullptr; }
    
    /// Эмулировать извлечение диска (eject)
    void MscEject();
#endif

private:
    bool initialized_ = false;
    Config config_{};
    
#ifdef USB_MSC_ENABLED
    IBlockDevice* msc_device_ = nullptr;
#endif

#ifdef USB_CDC_ENABLED
    CdcRxCallback cdc_rx_callback_ = nullptr;
    void* cdc_rx_context_ = nullptr;
    CdcLineCodingCallback cdc_lc_callback_ = nullptr;
    void* cdc_lc_context_ = nullptr;
    DfuJumpCallback dfu_callback_ = nullptr;
    void* dfu_context_ = nullptr;
#endif

    /// Toggle D+ пин для перезапуска USB
    void ToggleDpPin();
};

}  // namespace usb
