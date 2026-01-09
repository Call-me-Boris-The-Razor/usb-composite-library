# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.1.1] - 2026-01-10

### Fixed
- **PlatformIO package** — добавлены `libs/*` и `src/*` в export для корректной установки

---

## [3.1.0] - 2026-01-10

### Fixed
- **Windows Explorer crash** — исправлен краш Explorer при подключении USB MSC с неготовой SD картой
- **SCSI sense codes** — добавлена корректная установка SCSI sense при `block_count=0`
- **Порядок инициализации** — USB MSC теперь запускается только после готовности SD карты

### Changed
- **Единый IBlockDevice** — убрано дублирование, используется `ports::IBlockDevice`
- **Include paths** — добавлены пути к `libs/ports/include` и `libs/domain/include`

### Improved
- **Примеры** — `main_msc.cpp` теперь показывает правильный порядок инициализации с ожиданием готовности SD

---

## [3.0.0] - 2026-01-10

### Breaking Changes
- **SdmmcConfig API** — `instance` заменён на `sdmmc_index`, GPIO пины через `GpioPinConfig`
- **Публичный API без HAL** — заголовки не содержат HAL типов

### Added
- **pImpl паттерн** — HAL детали скрыты в реализации
- **libs/ структура** — ports/, domain/, adapters/ слои
- **IBlockDevice, IClock, ILogger** — чистые интерфейсы без HAL
- **MockBlockDevice, MockClock** — для unit тестов
- **.clang-format, .clang-tidy** — автоформатирование и статанализ
- **Unity тесты** — структура для unit тестов

### Changed
- **Глобальные static убраны** — буферы теперь члены класса (pImpl)
- **GpioPinConfig** — платформо-независимая конфигурация GPIO

---

## [2.4.0] - 2026-01-10

### Added
- **Auto PLL Config** — библиотека автоматически настроит PLL если он не настроен
- **HSE Auto-Detection** — определяет HSE_VALUE (25/8 MHz) или fallback на HSI
- **Full SystemClock Built-in** — для MSC не нужен SystemClock_Config от пользователя
- **Auto DFU Script** — `extra_script.py` для автоматического 1200 bps touch

### Changed
- **Truly Out of Box** — пользователю достаточно `HAL_Init()` даже для MSC
- **Composite Example** — минимальный CDC + MSC без SystemClock_Config

---

## [2.3.0] - 2026-01-10

### Added
- **Built-in HSI48 Init** — USB Clock автоматически настраивается из HSI48
- **Built-in SDMMC Clock** — SDMMC Clock автоматически настраивается от PLL
- **Built-in SysTick_Handler** — weak функция, HAL_IncTick() из коробки

### Changed
- **Minimal User Code** — пользователю достаточно `HAL_Init()` + `g_usb.Init()` + `g_usb.Start()`
- **Simplified Examples** — примеры сокращены до минимума

---

## [2.2.0] - 2026-01-10

### Added
- **Auto DFU Jump** — автоматический переход в системный bootloader при 1200 bps (без callback)
- **USB Diagnostics** — `GetDiagnostics()` для отладки инициализации USB
- **Test Examples** — тестовые примеры в `examples/basic_test/`

### Changed
- **No Linker Script Required** — Slave Mode без DMA, буферы в любой RAM
- **No Weak IRQ Handlers** — `OTG_FS_IRQHandler` и `OTG_HS_IRQHandler` гарантированно работают
- **TinyUSB 0.16.0** — обновлён API (`tud_int_handler` вместо `tusb_int_handler`)
- **extern "C" Init Functions** — корректная C linkage для weak функций

### Fixed
- Исправлена линковка weak функций инициализации
- Исправлен вызов IRQ handler для TinyUSB 0.16+

---

## [2.1.0] - 2025-01-09

### Added
- **SDMMC Driver** — встроенный драйвер SD карт (`SdmmcBlockDevice`)
- **Board Presets** — готовые конфигурации для плат (`OkoRelay()`, `DevEBoxH743()`, `WeActH743()`)
- **MSC SCSI Strings** — кастомизация SCSI Vendor/Product через флаги `USB_MSC_VENDOR`, `USB_MSC_PRODUCT`
- **Sector Translation Layer** — поддержка SD NAND с 1024-байт физическими секторами

### Changed
- Реструктуризация проекта — плоская структура вместо вложенной `lib/`
- Улучшенная документация с примерами

---

## [2.0.0] - 2024-01-09

### Added
- **CDC Terminal Detection** — `CdcTerminalOpened()` для надёжного определения открытия терминала
- **DFU Callback** — `CdcSetDfuCallback()` для автоматического перехода в bootloader при 1200 bps
- **Line Coding Callback** — `CdcSetLineCodingCallback()` для отслеживания изменения baudrate
- **Adapters** — готовые адаптеры для интеграции (`usb_adapters.h`)
- **Linker Script** — готовый фрагмент для STM32H7 DMA буферов

### Changed
- **API Refactoring** — единый класс `UsbDevice` вместо отдельных модулей
- **Namespace** — весь код в namespace `usb::`
- **Config** — структура `Config` для конфигурации при инициализации

### Fixed
- Toggle D+ теперь работает корректно на платах без VBUS detection
- Улучшена стабильность при быстром переподключении

## [1.0.0] - 2023-06-15

### Added
- Initial release
- CDC (Virtual COM Port) support
- MSC (Mass Storage) support
- TinyUSB integration
- STM32H7 support
