# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
