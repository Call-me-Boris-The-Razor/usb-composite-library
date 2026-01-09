<div align="center">

<img src="https://raw.githubusercontent.com/hathach/tinyusb/master/docs/assets/logo.svg" alt="TinyUSB" width="100"/>

# 🔌 USB Composite Library

### Plug-and-Play USB для STM32H7

[![Version](https://img.shields.io/badge/version-2.4.0-blue.svg?style=for-the-badge)](https://github.com/Call-me-Boris-The-Razor/usb-composite-library)
[![License](https://img.shields.io/badge/license-MIT-green.svg?style=for-the-badge)](LICENSE)
[![Platform](https://img.shields.io/badge/STM32-H7-orange.svg?style=for-the-badge&logo=stmicroelectronics)](https://www.st.com/en/microcontrollers-microprocessors/stm32h7-series.html)
[![TinyUSB](https://img.shields.io/badge/TinyUSB-0.16+-yellow.svg?style=for-the-badge)](https://github.com/hathach/tinyusb)

**CDC** (Virtual COM Port) + **MSC** (Mass Storage) + **SDMMC** (SD Card Driver)

*Всё в одном. Работает из коробки.*

---

[🚀 Quick Start](#-quick-start) •
[📖 Документация](#-документация) •
[💡 Примеры](#-примеры) •
[📚 API](#-api-reference)

</div>

---

## ✨ Возможности

| | Функция | Описание |
|:---:|---------|----------|
| 🖥️ | **CDC** | Virtual COM Port — логи, диагностика, CLI, отладка |
| 💾 | **MSC** | Mass Storage — SD карта как USB флешка |
| 📀 | **SDMMC** | Встроенный драйвер SD карт (SDMMC1, 4-bit, DMA) |
| ⚡ | **Plug & Play** | Минимум кода — максимум результата |
| 🔧 | **Модульность** | Включай только то, что нужно |
| 🔄 | **DFU Ready** | 1200 bps touch для перехода в bootloader |
| 🎛️ | **Presets** | Готовые конфиги для популярных плат |

---

## 🚀 Quick Start

```ini
# platformio.ini
lib_deps = 
    lib/usb_composite

build_flags = 
    -D USB_CDC_ENABLED
    -D USB_MSC_ENABLED
    -D USB_SDMMC_ENABLED
    -D STM32H743xx
```

```cpp
// main.cpp — CDC + MSC + SD карта (минимальный код!)
#include "usb_composite.h"
#include "usb_sdmmc.h"

usb::UsbDevice g_usb;
usb::SdmmcBlockDevice g_sd;

int main() {
    HAL_Init();  // ← Только это! Библиотека сама настроит PLL и clocks!
    
    // SD карта
    usb::SdmmcConfig sd_cfg;
    sd_cfg.instance = SDMMC1;
    sd_cfg.use_4bit_mode = true;
    g_sd.Init(sd_cfg);
    
    // USB
    g_usb.Init();
    g_usb.MscAttach(&g_sd);
    g_usb.Start();
    
    while (1) {
        g_usb.Process();
    }
}
```

**Всё.** Без `SystemClock_Config()`. Библиотека делает всё сама! 🎉

---

## 📋 Требования

| Компонент | Версия | Примечание |
|-----------|--------|------------|
| **PlatformIO** | любая | framework: `stm32cube` |
| **TinyUSB** | 0.15.0 | автоматически из GitHub |
| **MCU** | STM32H7 | тестировалось на H743, H750 |

---

## 📦 Установка

<details>
<summary><b>🔸 Вариант 1: Локальная библиотека</b></summary>

1. Скопируйте папку в `lib/` вашего проекта
2. Добавьте в `platformio.ini`:

```ini
lib_deps = 
    lib/usb_composite
    ; TinyUSB установится автоматически из GitHub
```

</details>

<details>
<summary><b>🔸 Вариант 2: Git submodule</b></summary>

```bash
git submodule add https://github.com/Call-me-Boris-The-Razor/usb-composite-library.git lib/usb_composite
```

</details>

---

## 📁 Структура проекта

```
usb_composite/
├── 📂 include/
│   ├── usb_composite.h         # 🎯 Главный API
│   ├── usb_sdmmc.h             # 💾 SDMMC драйвер
│   ├── usb_adapters.h          # 🔌 Адаптеры интеграции
│   ├── usb_composite_config.h  # ⚙️ Конфигурация TinyUSB
│   └── tusb_config.h           # 📝 TinyUSB config
├── 📂 src/
│   ├── usb_composite.cpp       # Реализация UsbDevice
│   ├── usb_sdmmc.cpp           # Реализация SDMMC
│   └── usb_descriptors.c       # USB дескрипторы
├── 📂 linker/
│   └── stm32h7_dma_section.ld  # Linker script фрагмент
└── 📄 library.json             # PlatformIO manifest
```

## ⚙️ Конфигурация

### Флаги компиляции

Добавьте в `platformio.ini`:

```ini
build_flags = 
    ; Включить только CDC (COM порт)
    -D USB_CDC_ENABLED
    
    ; Включить только MSC (флешка)
    ; -D USB_MSC_ENABLED
    
    ; Включить встроенный SDMMC драйвер
    ; -D USB_SDMMC_ENABLED
    
    ; Комбинации:
    ; CDC только:           -D USB_CDC_ENABLED
    ; MSC + SD:             -D USB_MSC_ENABLED -D USB_SDMMC_ENABLED
    ; CDC + MSC + SD:       -D USB_CDC_ENABLED -D USB_MSC_ENABLED -D USB_SDMMC_ENABLED
    
    ; Кастомные VID/PID (опционально, по умолчанию 0x0483/0x5743)
    ; -D USB_VID=0x0483
    ; -D USB_PID=0x5743
    
    ; Кастомные USB строки (опционально)
    ; -D USB_STR_MANUFACTURER=\"MyCompany\"
    ; -D USB_STR_PRODUCT=\"MyDevice\"
    
    ; Кастомные MSC SCSI строки (видны в свойствах диска)
    ; -D USB_MSC_VENDOR=\"CRSF\"
    ; -D USB_MSC_PRODUCT=\"SD\ NAND\ Storage\"
```

### Полный список флагов

| Флаг | По умолчанию | Описание |
|------|--------------|----------|
| `USB_CDC_ENABLED` | — | Включить CDC (COM порт) |
| `USB_MSC_ENABLED` | — | Включить MSC (флешка) |
| `USB_SDMMC_ENABLED` | — | Включить встроенный SDMMC драйвер |
| `USB_VID` | `0x0483` | Vendor ID |
| `USB_PID` | `0x5743` | Product ID |
| `USB_STR_MANUFACTURER` | `"STM32"` | Строка производителя |
| `USB_STR_PRODUCT` | `"USB Composite"` | Название продукта |
| `USB_MSC_VENDOR` | `"USB"` | SCSI Vendor (8 символов) |
| `USB_MSC_PRODUCT` | `"Mass Storage"` | SCSI Product (16 символов) |

---

## 💾 SDMMC (SD карта)

Библиотека включает готовый драйвер SD карт через SDMMC1.

### Пресеты для плат

```cpp
// OkoRelay / DevEBox H743 / WeAct H743 (стандартная распиновка SDMMC1)
g_sd.Init(usb::presets::OkoRelay());

// Или с кастомной конфигурацией
usb::SdmmcConfig cfg;
cfg.use_4bit_mode = true;
cfg.init_clock_div = 598;    // 400kHz @ 240MHz
cfg.normal_clock_div = 8;    // 24MHz @ 240MHz
g_sd.Init(cfg);
```

### Распиновка по умолчанию (SDMMC1)

| Сигнал | Пин | Описание |
|--------|-----|----------|
| CLK | PC12 | Тактовый сигнал |
| CMD | PD2 | Команды |
| D0 | PC8 | Data 0 |
| D1 | PC9 | Data 1 |
| D2 | PC10 | Data 2 |
| D3 | PC11 | Data 3 |

### Примеры использования

**Только флешка (без COM порта):**

```cpp
#include "usb_composite.h"
#include "usb_sdmmc.h"

usb::UsbDevice g_usb;
usb::SdmmcBlockDevice g_sd;

int main() {
    HAL_Init();
    SystemClock_Config();
    
    g_sd.Init(usb::presets::OkoRelay());
    
    g_usb.Init();
    g_usb.MscAttach(&g_sd);
    g_usb.Start();
    
    while (1) {
        g_usb.Process();
    }
}
```

**Только COM порт (без флешки):**

```cpp
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
            g_usb.CdcPrintf("Hello!\r\n");
        }
    }
}
```

### SdmmcBlockDevice API

| Метод | Описание |
|-------|----------|
| `Init(config)` | Инициализация SDMMC |
| `DeInit()` | Деинициализация |
| `IsCardInserted()` | Проверка наличия карты |
| `GetCardInfo()` | Информация о карте |
| `GetState()` | Состояние (Ready/Busy/Error) |
| `GetDiagnostics()` | Диагностика (HAL state/error) |
| `Sync()` | Сброс кэша на диск |
| `IsReady()` | Готовность (IBlockDevice) |
| `GetBlockCount()` | Количество блоков |
| `GetBlockSize()` | Размер блока (512) |
| `Read(lba, buf, cnt)` | Чтение блоков |
| `Write(lba, buf, cnt)` | Запись блоков |

### tusb_config.h

**Не требуется!** Библиотека уже содержит `tusb_config.h` в `include/`.

Просто убедитесь что путь к библиотеке добавлен в include paths:

```ini
build_flags = 
    -I lib/usb_composite/include
```

Если нужна кастомная конфигурация — создайте свой `tusb_config.h` в проекте (он перекроет библиотечный).

### Linker Script

**Не требуется!** Библиотека использует Slave Mode (polling) без DMA, поэтому буферы могут быть в любой RAM.

---

## 💡 Примеры

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

---

## 📚 API Reference

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

---

## 🔌 Адаптеры для интеграции

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

---

## 🔧 Платформозависимые функции

Библиотека работает "из коробки" для STM32H7:

- **IRQ Handlers** — `OTG_FS_IRQHandler` и `OTG_HS_IRQHandler` уже реализованы
- **board_millis()** — использует `HAL_GetTick()`
- **VBUS sensing** — автоматически отключается
- **Linker script** — не требуется (используется Slave Mode)

Если нужно переопределить IRQ handlers — добавьте флаг:

```ini
build_flags = -D USB_COMPOSITE_OWN_IRQ_HANDLERS
```

Slot-функции инициализации (weak, можно переопределить):

- `InitUsbGpio()` — инициализация GPIO PA11/PA12
- `InitUsbClock()` — включение тактирования USB
- `InitUsbOtg()` — настройка USB OTG регистров
- `InitUsbNvic()` — настройка прерываний

---

## 🔄 Toggle D+ пина

Некоторые платы (без детектора VBUS) требуют ручного toggle D+ для запуска USB:

```cpp
usb::Config cfg;
cfg.dp_toggle_pin = {GPIOA, 12};  // PA12
cfg.dp_toggle_ms = 10;            // 10 мс
g_usb.Init(cfg);
g_usb.Start();  // Здесь выполнится toggle
```

---

## ❓ Troubleshooting

### Диагностика USB

Библиотека предоставляет диагностику инициализации:

```cpp
auto diag = g_usb.GetDiagnostics();
printf("tusb_init: %s\n", diag.tusb_init_ok ? "OK" : "FAIL");
printf("USB base: 0x%08lX\n", diag.usb_base_addr);
printf("GCCFG: 0x%08lX\n", diag.gccfg);
printf("GOTGCTL: 0x%08lX\n", diag.gotgctl);
```

### USB не определяется

1. Проверьте диагностику (`GetDiagnostics()`)
2. Проверьте тактирование USB (HSI48 или PLL)
3. Попробуйте toggle D+ пина

### CDC не работает

1. Проверьте что `USB_CDC_ENABLED` определён
2. Установите драйвер VCP (для Windows)
3. Проверьте `CdcIsConnected()` перед записью

### MSC не работает

1. Проверьте что `USB_MSC_ENABLED` определён
2. Убедитесь что `IBlockDevice::IsReady()` возвращает true
3. Проверьте размер блока (должен быть 512)

---

## 📄 Лицензия

Этот проект распространяется под лицензией **MIT**. См. файл [LICENSE](LICENSE).

---

<div align="center">

### ⭐ Если библиотека полезна — поставьте звезду!

**Made with ❤️ for STM32 developers**

[![GitHub](https://img.shields.io/badge/GitHub-Call--me--Boris--The--Razor-181717?style=flat-square&logo=github)](https://github.com/Call-me-Boris-The-Razor)

</div>
