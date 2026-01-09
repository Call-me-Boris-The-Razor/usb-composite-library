<div align="center">

# 🔌 USB Composite Library

[![Version](https://img.shields.io/badge/version-2.0.0-blue.svg)](https://github.com/OkoDev/usb-composite)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-STM32H7-orange.svg)]()
[![Framework](https://img.shields.io/badge/framework-PlatformIO-purple.svg)]()
[![TinyUSB](https://img.shields.io/badge/TinyUSB-0.16.0+-yellow.svg)](https://github.com/hathach/tinyusb)

**Plug-and-play USB Composite Device (CDC + MSC) для STM32H7**

*Virtual COM Port + Mass Storage на базе TinyUSB — в одной строке кода*

[Быстрый старт](#-быстрый-старт) •
[Документация](#-документация) •
[Примеры](#-примеры) •
[API](#-api-reference)

</div>

---

## ✨ Возможности

| Функция | Описание |
|---------|----------|
| 🖥️ **CDC** | Virtual COM Port — логи, диагностика, CLI |
| 💾 **MSC** | Mass Storage — SD/eMMC как USB-флешка |
| ⚡ **Plug & Play** | Минимальная конфигурация — работает из коробки |
| 🔧 **Модульность** | Включайте только нужные модули через флаги |
| 🔄 **DFU Ready** | Поддержка 1200 bps touch для перехода в bootloader |
| 🎛️ **Toggle D+** | Совместимость с платами без VBUS detection |

---

## 🚀 Быстрый старт

### 1. Установка

```ini
# platformio.ini
[env:stm32h743]
platform = ststm32
board = genericSTM32H743VI
framework = stm32cube

lib_deps = 
    lib/usb_composite
    hathach/tinyusb@^0.16.0

build_flags = 
    -D USB_CDC_ENABLED
    -D USB_MSC_ENABLED
    -D STM32H743xx
```

### 2. Минимальный код

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
            g_usb.CdcPrintf("Hello USB! Tick: %lu\r\n", HAL_GetTick());
        }
        
        HAL_Delay(1000);
    }
}
```

**Готово!** 🎉 USB устройство работает.

---

## 📁 Структура проекта

```
usb_composite/
├── include/
│   ├── usb_composite.h        # Основной API
│   ├── usb_composite_config.h # Конфигурация TinyUSB
│   ├── usb_adapters.h         # Адаптеры для интеграции
│   └── tusb_config.h          # TinyUSB config (auto-generated)
├── src/
│   ├── usb_composite.cpp      # Реализация UsbDevice
│   └── usb_descriptors.c      # USB дескрипторы
├── linker/
│   └── stm32h7_dma_section.ld # Linker script для DMA буферов
└── library.json               # PlatformIO manifest
```

---

## 📖 Документация

### Конфигурация через флаги

| Флаг | Описание | По умолчанию |
|------|----------|--------------|
| `USB_CDC_ENABLED` | Включить CDC (COM порт) | — |
| `USB_MSC_ENABLED` | Включить MSC (флешка) | — |
| `USB_VID` | Vendor ID | `0x0483` |
| `USB_PID` | Product ID | `0x5743` |
| `USB_STR_MANUFACTURER` | Строка производителя | `"STM32"` |
| `USB_STR_PRODUCT` | Название продукта | `"USB Composite"` |

```ini
# platformio.ini
build_flags = 
    -D USB_CDC_ENABLED
    -D USB_MSC_ENABLED
    -D USB_VID=0x1234
    -D USB_PID=0x5678
    -D USB_STR_MANUFACTURER=\"MyCompany\"
    -D USB_STR_PRODUCT=\"MyDevice\"
```

### Linker Script

Для STM32H7 буферы USB должны находиться в Non-Cacheable RAM. Добавьте в linker script:

```ld
/* Секция для DMA буферов в RAM_D2 (Non-Cacheable) */
.dma_buffer (NOLOAD) :
{
    . = ALIGN(32);
    *(.dma_buffer)
    . = ALIGN(32);
} >RAM_D2
```

> 📄 Готовый фрагмент: [`linker/stm32h7_dma_section.ld`](lib/linker/stm32h7_dma_section.ld)

---

## 💡 Примеры

<details>
<summary><b>CDC — Virtual COM Port</b></summary>

```cpp
#include "usb_composite.h"

usb::UsbDevice g_usb;

void OnDataReceived(const uint8_t* data, uint32_t len, void* ctx) {
    // Echo received data
    g_usb.CdcWrite(data, len);
    
    // Обработка команд
    if (len > 0 && data[0] == '?') {
        g_usb.CdcWrite("Commands: ?, status, reboot\r\n");
    }
}

int main() {
    HAL_Init();
    SystemClock_Config();
    
    g_usb.Init();
    g_usb.CdcSetRxCallback(OnDataReceived, nullptr);
    g_usb.Start();
    
    while (1) {
        g_usb.Process();
        
        if (g_usb.CdcIsConnected()) {
            g_usb.CdcPrintf("[%lu] System running...\r\n", HAL_GetTick());
        }
        
        HAL_Delay(1000);
    }
}
```

</details>

<details>
<summary><b>MSC — SD карта как USB флешка</b></summary>

```cpp
#include "usb_composite.h"

// Реализация интерфейса блочного устройства
class SdBlockDevice : public usb::IBlockDevice {
public:
    bool IsReady() const override { return sd_initialized_; }
    uint32_t GetBlockCount() const override { return sd_block_count_; }
    uint32_t GetBlockSize() const override { return 512; }
    
    bool Read(uint32_t lba, uint8_t* buffer, uint32_t count) override {
        return HAL_SD_ReadBlocks(&hsd, buffer, lba, count, 1000) == HAL_OK;
    }
    
    bool Write(uint32_t lba, const uint8_t* buffer, uint32_t count) override {
        return HAL_SD_WriteBlocks(&hsd, (uint8_t*)buffer, lba, count, 1000) == HAL_OK;
    }
    
private:
    bool sd_initialized_ = true;
    uint32_t sd_block_count_ = 15523840; // 8GB SD card
};

usb::UsbDevice g_usb;
SdBlockDevice g_sd;

int main() {
    HAL_Init();
    SystemClock_Config();
    MX_SDMMC1_SD_Init();
    
    g_usb.Init();
    g_usb.MscAttach(&g_sd);
    g_usb.Start();
    
    while (1) {
        g_usb.Process();
    }
}
```

</details>

<details>
<summary><b>DFU — переход в bootloader</b></summary>

```cpp
#include "usb_composite.h"

extern void JumpToBootloader();  // Ваша реализация

void OnDfuRequest(void* ctx) {
    // Вызывается при baudrate 1200 bps
    JumpToBootloader();
}

int main() {
    g_usb.Init();
    g_usb.CdcSetDfuCallback(OnDfuRequest, nullptr);
    g_usb.Start();
    
    while (1) {
        g_usb.Process();
    }
}
```

</details>

<details>
<summary><b>Toggle D+ для плат без VBUS</b></summary>

```cpp
#include "usb_composite.h"

usb::UsbDevice g_usb;

int main() {
    HAL_Init();
    SystemClock_Config();
    
    // Конфигурация с toggle D+
    usb::Config cfg;
    cfg.dp_toggle_pin = {GPIOA, 12};  // PA12 = D+
    cfg.dp_toggle_ms = 10;            // 10 мс
    
    g_usb.Init(cfg);
    g_usb.Start();  // Автоматический toggle
    
    while (1) {
        g_usb.Process();
    }
}
```

</details>

---

## 📚 API Reference

### Класс `UsbDevice`

#### Основные методы

| Метод | Описание |
|-------|----------|
| `Init(config)` | Инициализация USB |
| `Start()` | Запуск USB (+ toggle D+ если настроено) |
| `Stop()` | Остановка USB |
| `Process()` | Обработка USB *(вызывать в main loop)* |
| `IsConnected()` | Проверка подключения к хосту |
| `GetState()` | Получить состояние (`State` enum) |

#### CDC методы *(требует `USB_CDC_ENABLED`)*

| Метод | Описание |
|-------|----------|
| `CdcIsConnected()` | Проверка DTR (терминал подключён) |
| `CdcTerminalOpened()` | Терминал реально открыт (SET_LINE_CODING) |
| `CdcWrite(data, len)` | Записать данные |
| `CdcWrite(str)` | Записать C-строку |
| `CdcPrintf(fmt, ...)` | Форматированный вывод |
| `CdcRead(buf, max)` | Прочитать данные |
| `CdcAvailable()` | Количество байт в буфере |
| `CdcFlushRx()` | Очистить буфер приёма |
| `CdcSetRxCallback(cb, ctx)` | Callback на приём данных |
| `CdcSetLineCodingCallback(cb, ctx)` | Callback на смену baudrate |
| `CdcSetDfuCallback(cb, ctx)` | Callback для DFU (1200 bps) |

#### MSC методы *(требует `USB_MSC_ENABLED`)*

| Метод | Описание |
|-------|----------|
| `MscAttach(device)` | Подключить блочное устройство |
| `MscDetach()` | Отключить устройство |
| `MscIsBusy()` | Идёт операция чтения/записи |
| `MscIsAttached()` | Устройство подключено |
| `MscEject()` | Эмуляция извлечения |

### Интерфейс `IBlockDevice`

```cpp
struct IBlockDevice {
    virtual bool IsReady() const = 0;
    virtual uint32_t GetBlockCount() const = 0;
    virtual uint32_t GetBlockSize() const = 0;  // Обычно 512
    virtual bool Read(uint32_t lba, uint8_t* buffer, uint32_t count) = 0;
    virtual bool Write(uint32_t lba, const uint8_t* buffer, uint32_t count) = 0;
};
```

---

## 🔧 Troubleshooting

<details>
<summary><b>USB не определяется</b></summary>

1. Проверьте тактирование USB (HSI48 или PLL)
2. Убедитесь что VBUS sensing отключён или настроен
3. Попробуйте toggle D+ пина
4. Проверьте linker script (секция `.dma_buffer`)

</details>

<details>
<summary><b>CDC не работает</b></summary>

1. Убедитесь что `USB_CDC_ENABLED` определён
2. Установите драйвер VCP для Windows (ST VCP Driver)
3. Проверьте `CdcIsConnected()` перед записью
4. Используйте `CdcTerminalOpened()` для более надёжной детекции

</details>

<details>
<summary><b>MSC не работает</b></summary>

1. Убедитесь что `USB_MSC_ENABLED` определён
2. Проверьте что `IBlockDevice::IsReady()` возвращает `true`
3. Размер блока должен быть 512 байт
4. Проверьте что SD карта инициализирована

</details>

---

## 📋 Требования

- **PlatformIO** с фреймворком `stm32cube`
- **TinyUSB** >= 0.16.0
- **STM32H7** (тестировалось на STM32H743)

---

## 📄 Лицензия

[MIT License](LICENSE) © OkoDev

---

<div align="center">

**[⬆ Наверх](#-usb-composite-library)**

Made with ❤️ for embedded developers

</div>
