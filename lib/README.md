# USB Composite Library v2.0

**Plug-and-play** USB Composite Device (CDC + MSC) библиотека для STM32H7 на базе TinyUSB.

## Возможности

- **CDC**: Virtual COM Port (логи, диагностика, CLI)
- **MSC**: Mass Storage (SD/eMMC как USB-флешка)
- **Гибкая конфигурация**: включение/отключение модулей через флаги
- **Toggle D+**: поддержка плат, требующих ручного перезапуска USB

## Quick Start (STM32H7)

```ini
# platformio.ini
lib_deps = 
    lib/usb_composite

build_flags = 
    -D USB_CDC_ENABLED
    -D USB_MSC_ENABLED
    -D STM32H743xx
```

```cpp
// main.cpp
#include "usb_composite.h"

usb::UsbDevice g_usb;

int main() {
    HAL_Init();
    SystemClock_Config();
    
    g_usb.Init();
    g_usb.Start();
    
    while (1) {
        g_usb.Process();
        
        if (g_usb.CdcTerminalOpened()) {
            g_usb.CdcPrintf("Hello USB!\r\n");
        }
    }
}
```

**Всё.** Никаких дополнительных файлов не нужно.

## Требования

- **PlatformIO** с фреймворком `stm32cube`
- **TinyUSB** (автоматически подтягивается как зависимость)
- **STM32H7** (тестировалось на STM32H743)

## Установка

### Вариант 1: Локальная библиотека

1. Скопируйте папку `usb_composite` в `lib/` вашего проекта
2. Добавьте в `platformio.ini`:

```ini
lib_deps = 
    lib/usb_composite
    ; TinyUSB (укажите путь или репозиторий)
    lib/tinyusb
```

### Вариант 2: Git submodule

```bash
git submodule add <repo_url> lib/usb_composite
```

## Конфигурация

### Флаги компиляции

Добавьте в `platformio.ini`:

```ini
build_flags = 
    ; Включить только CDC (COM порт)
    -D USB_CDC_ENABLED
    
    ; Включить только MSC (флешка)
    ; -D USB_MSC_ENABLED
    
    ; Или оба:
    ; -D USB_CDC_ENABLED
    ; -D USB_MSC_ENABLED
    
    ; Кастомные VID/PID (опционально, по умолчанию 0x0483/0x5743)
    ; -D USB_VID=0x0483
    ; -D USB_PID=0x5743
    
    ; Кастомные строки (опционально)
    -D USB_STR_MANUFACTURER=\"MyCompany\"
    -D USB_STR_PRODUCT=\"MyDevice\"
```

### tusb_config.h

**Не требуется!** Библиотека уже содержит `tusb_config.h` в `include/`.

Просто убедитесь что путь к библиотеке добавлен в include paths:

```ini
build_flags = 
    -I lib/usb_composite/include
```

Если нужна кастомная конфигурация — создайте свой `tusb_config.h` в проекте (он перекроет библиотечный).

### Linker Script

Для STM32H7 добавьте секцию `.dma_buffer` в RAM_D2:

```ld
.dma_buffer (NOLOAD) :
{
    . = ALIGN(32);
    *(.dma_buffer)
    . = ALIGN(32);
} >RAM_D2
```

## Использование

### Базовый пример (только CDC)

```cpp
#include "usb_composite.h"

usb::UsbDevice g_usb;

int main() {
    // Системная инициализация...
    HAL_Init();
    SystemClock_Config();
    
    // Конфигурация USB
    usb::Config cfg;
    cfg.dp_toggle_pin = {GPIOA, 12};  // PA12 = D+ (опционально)
    cfg.dp_toggle_ms = 10;            // 10 мс toggle
    
    // Инициализация и запуск
    g_usb.Init(cfg);
    g_usb.Start();
    
    while (1) {
        g_usb.Process();
        
        // Вывод логов
        if (g_usb.CdcIsConnected()) {
            g_usb.CdcPrintf("Tick: %lu\r\n", HAL_GetTick());
        }
        
        HAL_Delay(1000);
    }
}
```

### CDC + MSC (флешка)

```cpp
#include "usb_composite.h"

// Реализация блочного устройства (например, SD карта)
class SdBlockDevice : public usb::IBlockDevice {
public:
    bool IsReady() const override { return sd_ready_; }
    uint32_t GetBlockCount() const override { return block_count_; }
    uint32_t GetBlockSize() const override { return 512; }
    
    bool Read(uint32_t lba, uint8_t* buffer, uint32_t count) override {
        // Ваша реализация чтения SD
        return HAL_SD_ReadBlocks(&hsd, buffer, lba, count, 1000) == HAL_OK;
    }
    
    bool Write(uint32_t lba, const uint8_t* buffer, uint32_t count) override {
        // Ваша реализация записи SD
        return HAL_SD_WriteBlocks(&hsd, (uint8_t*)buffer, lba, count, 1000) == HAL_OK;
    }
    
private:
    bool sd_ready_ = true;
    uint32_t block_count_ = 1000000;
};

usb::UsbDevice g_usb;
SdBlockDevice g_sd;

int main() {
    HAL_Init();
    SystemClock_Config();
    
    // Инициализация SD карты
    // ...
    
    // USB
    g_usb.Init();
    g_usb.MscAttach(&g_sd);  // Подключаем SD к MSC
    g_usb.Start();
    
    while (1) {
        g_usb.Process();
        
        // Логи через CDC
        g_usb.CdcPrintf("MSC busy: %d\r\n", g_usb.MscIsBusy());
    }
}
```

### Callback при получении данных CDC

```cpp
void OnCdcRx(const uint8_t* data, uint32_t len, void* ctx) {
    // Обработка полученных данных
    if (len > 0 && data[0] == '?') {
        g_usb.CdcWrite("Help: ...\r\n");
    }
}

int main() {
    g_usb.Init();
    g_usb.CdcSetRxCallback(OnCdcRx, nullptr);
    g_usb.Start();
    // ...
}
```

### DFU через 1200 bps touch

Библиотека автоматически определяет магический baudrate 1200 bps.
Установите callback для перехода в bootloader:

```cpp
#include "usb_composite.h"

// Внешняя функция перехода в DFU (реализуется в проекте)
extern void ScheduleBootloaderJump();

void OnDfuRequest(void* ctx) {
    // Вызывается автоматически при baudrate == 1200
    ScheduleBootloaderJump();
}

int main() {
    g_usb.Init();
    g_usb.CdcSetDfuCallback(OnDfuRequest, nullptr);
    g_usb.Start();
    // ...
}
```

### Определение открытия терминала

```cpp
int main() {
    g_usb.Init();
    g_usb.Start();
    
    while (1) {
        g_usb.Process();
        
        // CdcTerminalOpened() — более надёжный индикатор чем CdcIsConnected()
        // Устанавливается при получении SET_LINE_CODING с baudrate != 1200
        if (g_usb.CdcTerminalOpened()) {
            g_usb.CdcPrintf("Terminal connected!\r\n");
            g_usb.CdcResetTerminalFlag();  // Сбросить флаг после обработки
        }
    }
}
```

## API Reference

### UsbDevice

| Метод | Описание |
|-------|----------|
| `Init(config)` | Инициализация USB |
| `Start()` | Запуск USB (toggle D+ если настроено) |
| `Stop()` | Остановка USB |
| `Process()` | Обработка USB (вызывать в main loop) |
| `IsConnected()` | Проверка подключения к хосту |
| `GetState()` | Получить текущее состояние |

### CDC методы (требует USB_CDC_ENABLED)

| Метод | Описание |
|-------|----------|
| `CdcIsConnected()` | Проверка подключения CDC (DTR) |
| `CdcTerminalOpened()` | Терминал открыт (SET_LINE_CODING получен) |
| `CdcResetTerminalFlag()` | Сброс флага терминала |
| `CdcWrite(data, len)` | Запись данных |
| `CdcWrite(str)` | Запись строки |
| `CdcPrintf(fmt, ...)` | Форматированный вывод |
| `CdcRead(buf, max)` | Чтение данных |
| `CdcAvailable()` | Количество доступных байт |
| `CdcFlushRx()` | Очистка буфера приёма |
| `CdcSetRxCallback(cb, ctx)` | Callback получения данных |
| `CdcSetLineCodingCallback(cb, ctx)` | Callback изменения baudrate |
| `CdcSetDfuCallback(cb, ctx)` | Callback для DFU (1200 bps) |

### MSC методы (требует USB_MSC_ENABLED)

| Метод | Описание |
|-------|----------|
| `MscAttach(device)` | Подключить блочное устройство |
| `MscDetach()` | Отключить устройство |
| `MscIsBusy()` | Проверка занятости |
| `MscIsAttached()` | Проверка подключения |
| `MscEject()` | Эмуляция извлечения |

### IBlockDevice интерфейс

Для подключения своего хранилища реализуйте интерфейс:

```cpp
struct IBlockDevice {
    virtual bool IsReady() const = 0;
    virtual uint32_t GetBlockCount() const = 0;
    virtual uint32_t GetBlockSize() const = 0;
    virtual bool Read(uint32_t lba, uint8_t* buffer, uint32_t count) = 0;
    virtual bool Write(uint32_t lba, const uint8_t* buffer, uint32_t count) = 0;
};
```

## Адаптеры для интеграции

Файл `include/usb_adapters.h` содержит примеры адаптеров для интеграции с проектом:

### UsbDebugAdapter (IDebugOutput → UsbDevice)

```cpp
#define HAS_DEBUG_INTERFACE  // Активирует адаптер
#include "usb_adapters.h"

usb::UsbDevice g_usb;
usb::UsbDebugAdapter g_usb_debug(&g_usb);

// Подключение к системе логирования
SystemLogger::GetInstance().SetOutput(&g_usb_debug);
```

### SdBlockAdapter (SdDisk → IBlockDevice)

```cpp
#define HAS_SD_DISK  // Активирует адаптер
#include "usb_adapters.h"

drivers::SdDisk g_sd;
usb::SdBlockAdapter g_sd_adapter(&g_sd);

// Подключение к USB MSC
g_usb.MscAttach(&g_sd_adapter);
```

## Платформозависимые функции

Библиотека содержит weak-функции инициализации для STM32H7:

- `InitUsbGpio()` — инициализация GPIO PA11/PA12
- `InitUsbClock()` — включение тактирования USB
- `InitUsbOtg()` — настройка USB OTG регистров
- `InitUsbNvic()` — настройка прерываний

Для других платформ переопределите эти функции в своём проекте.

## Toggle D+ пина

Некоторые платы (без детектора VBUS) требуют ручного toggle D+ для запуска USB:

```cpp
usb::Config cfg;
cfg.dp_toggle_pin = {GPIOA, 12};  // PA12
cfg.dp_toggle_ms = 10;            // 10 мс
g_usb.Init(cfg);
g_usb.Start();  // Здесь выполнится toggle
```

## Troubleshooting

### USB не определяется

1. Проверьте тактирование USB (HSI48 или PLL)
2. Убедитесь что VBUS sensing отключён (или настроен)
3. Попробуйте toggle D+ пина

### CDC не работает

1. Проверьте что `USB_CDC_ENABLED` определён
2. Установите драйвер VCP (для Windows)
3. Проверьте `CdcIsConnected()` перед записью

### MSC не работает

1. Проверьте что `USB_MSC_ENABLED` определён
2. Убедитесь что `IBlockDevice::IsReady()` возвращает true
3. Проверьте размер блока (должен быть 512)

## Лицензия

MIT License
