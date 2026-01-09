# Руководство по интеграции

Пошаговая инструкция по интеграции USB Composite Library в ваш проект.

## Содержание

- [Требования](#требования)
- [Установка](#установка)
- [Конфигурация PlatformIO](#конфигурация-platformio)
- [Настройка Linker Script](#настройка-linker-script)
- [Инициализация системы](#инициализация-системы)
- [Интеграция с существующим кодом](#интеграция-с-существующим-кодом)

---

## Требования

### Hardware
- STM32H7 микроконтроллер (тестировалось на H743, H750)
- USB Full Speed порт (PA11/PA12)
- Опционально: SD карта для MSC

### Software
- PlatformIO
- Framework: stm32cube
- TinyUSB >= 0.16.0

---

## Установка

### Вариант 1: Локальная копия

```bash
# Скопируйте библиотеку в lib/ вашего проекта
cp -r usb_composite /path/to/your/project/lib/
```

### Вариант 2: Git Submodule

```bash
cd /path/to/your/project
git submodule add <repo_url> lib/usb_composite
```

### Вариант 3: PlatformIO Library

```ini
# platformio.ini
lib_deps = 
    lib/usb_composite
```

---

## Конфигурация PlatformIO

### Минимальная конфигурация

```ini
[env:stm32h743]
platform = ststm32
board = genericSTM32H743VI
framework = stm32cube

lib_deps = 
    lib/usb_composite
    hathach/tinyusb@^0.16.0

build_flags = 
    -D STM32H743xx
    -D USB_CDC_ENABLED
```

### Полная конфигурация

```ini
[env:stm32h743]
platform = ststm32
board = genericSTM32H743VI
framework = stm32cube

lib_deps = 
    lib/usb_composite
    hathach/tinyusb@^0.16.0

build_flags = 
    ; Микроконтроллер
    -D STM32H743xx
    
    ; USB модули
    -D USB_CDC_ENABLED
    -D USB_MSC_ENABLED
    
    ; Кастомные VID/PID
    -D USB_VID=0x1234
    -D USB_PID=0x5678
    
    ; Строки устройства
    -D USB_STR_MANUFACTURER=\"MyCompany\"
    -D USB_STR_PRODUCT=\"MyDevice\"
    
    ; Include paths
    -I lib/usb_composite/include

; Кастомный linker script (опционально)
board_build.ldscript = custom_linker.ld
```

---

## Настройка Linker Script

### Зачем нужен кастомный linker script?

STM32H7 имеет особенность: USB DMA может работать только с определёнными областями памяти. Буферы TinyUSB должны находиться в Non-Cacheable RAM (обычно RAM_D2).

### Добавление секции .dma_buffer

Добавьте в ваш linker script (или создайте новый):

```ld
/* Memory regions (пример для STM32H743) */
MEMORY
{
    FLASH (rx)   : ORIGIN = 0x08000000, LENGTH = 2M
    DTCMRAM (xrw): ORIGIN = 0x20000000, LENGTH = 128K
    RAM_D1 (xrw) : ORIGIN = 0x24000000, LENGTH = 512K
    RAM_D2 (xrw) : ORIGIN = 0x30000000, LENGTH = 288K
    RAM_D3 (xrw) : ORIGIN = 0x38000000, LENGTH = 64K
}

/* Sections */
SECTIONS
{
    /* ... существующие секции ... */
    
    /* DMA буферы для USB (Non-Cacheable RAM) */
    .dma_buffer (NOLOAD) :
    {
        . = ALIGN(32);
        *(.dma_buffer)
        . = ALIGN(32);
    } >RAM_D2
}
```

### Использование готового фрагмента

Библиотека содержит готовый фрагмент в `linker/stm32h7_dma_section.ld`. Включите его в ваш основной linker script:

```ld
INCLUDE "lib/usb_composite/linker/stm32h7_dma_section.ld"
```

---

## Инициализация системы

### Порядок инициализации

```cpp
#include "usb_composite.h"

usb::UsbDevice g_usb;

int main() {
    // 1. HAL инициализация
    HAL_Init();
    
    // 2. Настройка системного тактирования
    SystemClock_Config();
    
    // 3. Инициализация периферии (GPIO, SD, etc.)
    // MX_GPIO_Init();
    // MX_SDMMC1_SD_Init();
    
    // 4. USB инициализация
    usb::Config cfg;
    cfg.dp_toggle_pin = {GPIOA, 12};  // Опционально
    cfg.dp_toggle_ms = 10;
    
    g_usb.Init(cfg);
    
    // 5. Настройка callbacks (опционально)
    g_usb.CdcSetRxCallback(OnRxCallback, nullptr);
    
    // 6. Запуск USB
    g_usb.Start();
    
    // 7. Main loop
    while (1) {
        g_usb.Process();
        // ... ваш код
    }
}
```

### Настройка тактирования USB

USB требует 48 МГц тактирование. Убедитесь что в `SystemClock_Config()` настроен HSI48 или PLL для USB:

```c
void SystemClock_Config(void) {
    // ... основная настройка тактирования ...
    
    // USB тактирование от HSI48
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    
    // Выбор источника для USB
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
    PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
}
```

---

## Интеграция с существующим кодом

### Интеграция с системой логирования

Файл `usb_adapters.h` содержит готовые адаптеры:

```cpp
#define HAS_DEBUG_INTERFACE  // Включить адаптер
#include "usb_adapters.h"

usb::UsbDevice g_usb;
usb::UsbDebugAdapter g_debug(&g_usb);

// Подключение к вашей системе логирования
Logger::SetOutput(&g_debug);
```

### Интеграция с SD картой

```cpp
#define HAS_SD_DISK  // Включить адаптер
#include "usb_adapters.h"

drivers::SdDisk g_sd;
usb::SdBlockAdapter g_sd_adapter(&g_sd);

void main() {
    // ... инициализация ...
    
    g_usb.MscAttach(&g_sd_adapter);
    g_usb.Start();
}
```

### Интеграция с RTOS

При использовании FreeRTOS или другой RTOS:

```cpp
// Задача USB
void UsbTask(void* param) {
    usb::UsbDevice* usb = (usb::UsbDevice*)param;
    
    while (1) {
        usb->Process();
        vTaskDelay(1);  // Небольшая задержка
    }
}

int main() {
    g_usb.Init();
    g_usb.Start();
    
    xTaskCreate(UsbTask, "USB", 256, &g_usb, 2, nullptr);
    vTaskStartScheduler();
}
```

---

## Проверка работоспособности

### Тест CDC

```cpp
while (1) {
    g_usb.Process();
    
    if (g_usb.CdcTerminalOpened()) {
        g_usb.CdcPrintf("USB CDC работает! Tick: %lu\r\n", HAL_GetTick());
        HAL_Delay(1000);
    }
}
```

### Тест MSC

После подключения USB:
1. Устройство должно появиться в системе как съёмный диск
2. Проверьте чтение/запись файлов
3. Безопасно извлеките устройство

---

## Следующие шаги

- [API Reference](API.md) — полное описание API
- [Примеры](../README.md#-примеры) — готовые примеры кода
- [Troubleshooting](../README.md#-troubleshooting) — решение проблем
