# API Reference

**Версия 3.1.0**

Полная документация по API USB Composite Library.

## Что нового в 3.1.0

- **Исправлен краш Windows Explorer** — корректная обработка USB MSC с неготовой SD картой
- **Единый IBlockDevice** — убрано дублирование, используется `ports::IBlockDevice`
- **Правильный порядок инициализации** — USB MSC запускается только после готовности SD

## Содержание

- [Namespace usb](#namespace-usb)
- [Класс UsbDevice](#класс-usbdevice)
- [Класс SdmmcBlockDevice](#класс-sdmmcblockdevice)
- [Интерфейс IBlockDevice](#интерфейс-iblockdevice)
- [Структуры конфигурации](#структуры-конфигурации)
- [Callback типы](#callback-типы)
- [Enum State](#enum-state)

---

## Namespace usb

Все публичные символы находятся в namespace `usb::`.

```cpp
#include "usb_composite.h"

usb::UsbDevice device;
usb::Config config;
usb::State state;
```

---

## Класс UsbDevice

Основной класс для работы с USB устройством.

### Конструктор

```cpp
UsbDevice();
```

Создаёт экземпляр USB устройства. Копирование запрещено.

### Основные методы

#### Init

```cpp
bool Init(const Config& config = Config{});
```

Инициализирует USB подсистему.

**Параметры:**
- `config` — конфигурация устройства (опционально)

**Возвращает:** `true` при успешной инициализации

**Пример:**
```cpp
usb::Config cfg;
cfg.vid = 0x1234;
cfg.pid = 0x5678;
g_usb.Init(cfg);
```

#### Start

```cpp
bool Start();
```

Запускает USB устройство и подключается к хосту. Если настроен toggle D+, выполняет его.

**Возвращает:** `true` при успехе

#### Stop

```cpp
void Stop();
```

Останавливает USB устройство.

#### Process

```cpp
void Process();
```

Обрабатывает USB события. **Должен вызываться в main loop.**

```cpp
while (1) {
    g_usb.Process();
    // ... остальной код
}
```

#### IsInitialized

```cpp
bool IsInitialized() const;
```

**Возвращает:** `true` если устройство инициализировано

#### IsConnected

```cpp
bool IsConnected() const;
```

**Возвращает:** `true` если устройство подключено к хосту

#### GetState

```cpp
State GetState() const;
```

**Возвращает:** текущее состояние устройства (см. [Enum State](#enum-state))

---

### CDC методы

> ⚠️ Требуют `#define USB_CDC_ENABLED`

#### CdcIsConnected

```cpp
bool CdcIsConnected() const;
```

Проверяет подключение CDC (на основе DTR сигнала).

**Возвращает:** `true` если терминал подключён

#### CdcTerminalOpened

```cpp
bool CdcTerminalOpened() const;
```

Более надёжная проверка открытия терминала. Возвращает `true` после получения SET_LINE_CODING с baudrate ≠ 1200.

#### CdcResetTerminalFlag

```cpp
void CdcResetTerminalFlag();
```

Сбрасывает флаг открытия терминала.

#### CdcWrite

```cpp
uint32_t CdcWrite(const uint8_t* data, uint32_t len);
uint32_t CdcWrite(const char* str);
```

Записывает данные в CDC.

**Параметры:**
- `data` / `str` — данные для записи
- `len` — длина данных

**Возвращает:** количество записанных байт

#### CdcPrintf

```cpp
uint32_t CdcPrintf(const char* fmt, ...);
```

Форматированный вывод (аналог printf).

**Пример:**
```cpp
g_usb.CdcPrintf("Temperature: %.1f°C\r\n", temp);
```

#### CdcRead

```cpp
uint32_t CdcRead(uint8_t* buffer, uint32_t max_len);
```

Читает данные из буфера CDC.

**Параметры:**
- `buffer` — буфер для данных
- `max_len` — максимальное количество байт

**Возвращает:** количество прочитанных байт

#### CdcAvailable

```cpp
uint32_t CdcAvailable() const;
```

**Возвращает:** количество доступных байт для чтения

#### CdcFlushRx

```cpp
void CdcFlushRx();
```

Очищает буфер приёма.

#### CdcSetRxCallback

```cpp
void CdcSetRxCallback(CdcRxCallback callback, void* context = nullptr);
```

Устанавливает callback для обработки входящих данных.

**Пример:**
```cpp
void OnRx(const uint8_t* data, uint32_t len, void* ctx) {
    // Обработка данных
}

g_usb.CdcSetRxCallback(OnRx, nullptr);
```

#### CdcSetLineCodingCallback

```cpp
void CdcSetLineCodingCallback(CdcLineCodingCallback callback, void* context = nullptr);
```

Callback при изменении baudrate.

#### CdcSetDfuCallback

```cpp
void CdcSetDfuCallback(DfuJumpCallback callback, void* context = nullptr);
```

Callback для DFU режима (вызывается при baudrate 1200 bps).

---

### MSC методы

> ⚠️ Требуют `#define USB_MSC_ENABLED`

#### MscAttach

```cpp
void MscAttach(IBlockDevice* device);
```

Подключает блочное устройство к MSC.

**Параметры:**
- `device` — указатель на реализацию IBlockDevice

**Пример:**
```cpp
SdBlockDevice sd;
g_usb.MscAttach(&sd);
```

#### MscDetach

```cpp
void MscDetach();
```

Отключает блочное устройство.

#### MscIsBusy

```cpp
bool MscIsBusy() const;
```

**Возвращает:** `true` если идёт операция чтения/записи

#### MscIsAttached

```cpp
bool MscIsAttached() const;
```

**Возвращает:** `true` если устройство подключено

#### MscEject

```cpp
void MscEject();
```

Эмулирует извлечение диска.

---

## Класс SdmmcBlockDevice

> ⚠️ Требует `#define USB_MSC_ENABLED` и `#define USB_SDMMC_ENABLED`

Встроенный драйвер SD карт через SDMMC1. Реализует интерфейс `IBlockDevice`.

```cpp
#include "usb_sdmmc.h"

usb::SdmmcBlockDevice g_sd;
g_sd.Init(usb::presets::OkoRelay());
```

### Методы инициализации

#### Init

```cpp
bool Init(const SdmmcConfig& config = SdmmcConfig{});
```

Инициализирует SDMMC периферию и SD карту.

**Параметры:**
- `config` — конфигурация SDMMC (пины, скорости, режим)

**Возвращает:** `true` при успешной инициализации

**Пример:**
```cpp
// Использование пресета
g_sd.Init(usb::presets::OkoRelay());

// Или кастомная конфигурация
usb::SdmmcConfig cfg;
cfg.use_4bit_mode = true;
cfg.normal_clock_div = 4;  // Быстрее
g_sd.Init(cfg);
```

#### DeInit

```cpp
void DeInit();
```

Деинициализирует SDMMC.

### Методы статуса

#### IsCardInserted

```cpp
bool IsCardInserted() const;
```

**Возвращает:** `true` если карта вставлена и готова

#### GetCardInfo

```cpp
SdmmcCardInfo GetCardInfo() const;
```

**Возвращает:** информацию о карте (размер, тип, версия)

```cpp
auto info = g_sd.GetCardInfo();
printf("Capacity: %llu MB\n", info.capacity_bytes / (1024*1024));
printf("Block count: %lu\n", info.block_count);
```

#### GetState

```cpp
SdmmcState GetState() const;
```

**Возвращает:** текущее состояние (`NotInitialized`, `Ready`, `Busy`, `Error`)

#### GetDiagnostics

```cpp
SdmmcDiagnostics GetDiagnostics() const;
```

**Возвращает:** диагностическую информацию (HAL state, error codes)

#### Sync

```cpp
bool Sync();
```

Синхронизирует кэш с картой.

**Возвращает:** `true` при успехе

#### GetHandle

```cpp
SD_HandleTypeDef* GetHandle();
```

**Возвращает:** HAL handle для продвинутого использования

### IBlockDevice методы

```cpp
bool IsReady() const override;
uint32_t GetBlockCount() const override;
uint32_t GetBlockSize() const override;  // Всегда 512
bool Read(uint32_t lba, uint8_t* buffer, uint32_t count) override;
bool Write(uint32_t lba, const uint8_t* buffer, uint32_t count) override;
```

### Структуры

#### SdmmcConfig

```cpp
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
    
    // Alternate Function
    uint8_t alternate_function = GPIO_AF12_SDMMC1;
    
    // SDMMC Instance
    SDMMC_TypeDef* instance = SDMMC1;
    
    // Режим шины
    bool use_4bit_mode = true;
    
    // Clock dividers
    uint32_t init_clock_div = 598;    // 400kHz для инициализации
    uint32_t normal_clock_div = 8;    // 24MHz для работы
    
    // Таймауты (мс)
    uint32_t init_timeout_ms = 2000;
    uint32_t rw_timeout_ms = 2000;
};
```

#### SdmmcCardInfo

```cpp
struct SdmmcCardInfo {
    uint32_t block_count;       // Количество блоков
    uint32_t block_size;        // Размер блока (512)
    uint64_t capacity_bytes;    // Полная ёмкость
    uint32_t card_type;         // Тип карты
    uint32_t card_version;      // Версия
    bool is_ready;              // Готовность
};
```

#### SdmmcState

```cpp
enum class SdmmcState : uint8_t {
    NotInitialized,   // Не инициализирован
    Ready,            // Готов
    Busy,             // Занят
    Error,            // Ошибка
};
```

### Пресеты плат

```cpp
namespace usb::presets {
    SdmmcConfig OkoRelay();      // OkoRelay (стандартная распиновка)
    SdmmcConfig DevEBoxH743();   // DevEBox H743
    SdmmcConfig WeActH743();     // WeAct Studio H743
}
```

---

## Интерфейс IBlockDevice

Абстрактный интерфейс для подключения хранилища к MSC.

```cpp
struct IBlockDevice {
    virtual ~IBlockDevice() = default;
    
    virtual bool IsReady() const = 0;
    virtual uint32_t GetBlockCount() const = 0;
    virtual uint32_t GetBlockSize() const = 0;
    virtual bool Read(uint32_t lba, uint8_t* buffer, uint32_t count) = 0;
    virtual bool Write(uint32_t lba, const uint8_t* buffer, uint32_t count) = 0;
};
```

### Методы

| Метод | Описание |
|-------|----------|
| `IsReady()` | Проверка готовности устройства |
| `GetBlockCount()` | Общее количество блоков |
| `GetBlockSize()` | Размер блока (обычно 512) |
| `Read(lba, buffer, count)` | Чтение блоков |
| `Write(lba, buffer, count)` | Запись блоков |

### Пример реализации

```cpp
class SdCard : public usb::IBlockDevice {
public:
    bool IsReady() const override {
        return sd_initialized_;
    }
    
    uint32_t GetBlockCount() const override {
        return total_blocks_;
    }
    
    uint32_t GetBlockSize() const override {
        return 512;
    }
    
    bool Read(uint32_t lba, uint8_t* buffer, uint32_t count) override {
        return HAL_SD_ReadBlocks(&hsd, buffer, lba, count, 1000) == HAL_OK;
    }
    
    bool Write(uint32_t lba, const uint8_t* buffer, uint32_t count) override {
        return HAL_SD_WriteBlocks(&hsd, (uint8_t*)buffer, lba, count, 1000) == HAL_OK;
    }
};
```

---

## Структуры конфигурации

### Config

```cpp
struct Config {
    GpioPin dp_toggle_pin = {nullptr, 0};  // Пин D+ для toggle
    uint32_t dp_toggle_ms = 10;            // Длительность toggle (мс)
    uint16_t vid = 0x0483;                 // Vendor ID
    uint16_t pid = 0x5743;                 // Product ID
    const char* manufacturer = "STM32";    // Строка производителя
    const char* product = "USB Composite"; // Название продукта
    const char* serial = nullptr;          // Серийный номер
};
```

### GpioPin

```cpp
struct GpioPin {
    void* port;      // GPIO порт (GPIOA, GPIOB, ...)
    uint16_t pin;    // Номер пина (0-15)
};
```

---

## Callback типы

```cpp
// Callback при получении данных CDC
using CdcRxCallback = void(*)(const uint8_t* data, uint32_t len, void* context);

// Callback при изменении baudrate
using CdcLineCodingCallback = void(*)(uint32_t baudrate, void* context);

// Callback для DFU (1200 bps)
using DfuJumpCallback = void(*)(void* context);
```

---

## Enum State

```cpp
enum class State : uint8_t {
    NotInitialized = 0,  // Не инициализировано
    Disconnected   = 1,  // Отключено от хоста
    Connected      = 2,  // Подключено к хосту
    Configured     = 3,  // Сконфигурировано
    Suspended      = 4,  // Приостановлено
};
```

---

## Константы

```cpp
static constexpr uint32_t kDfuBaudrate = 1200;  // Магический baudrate для DFU
```
