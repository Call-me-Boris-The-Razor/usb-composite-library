/**
 * @file usb_composite_config.h
 * @brief Конфигурация TinyUSB для USB Composite библиотеки
 * 
 * Этот файл должен быть скопирован в проект и переименован в tusb_config.h,
 * либо включён из вашего tusb_config.h
 * 
 * Флаги управления:
 * - USB_CDC_ENABLED: включает CDC (Virtual COM Port)
 * - USB_MSC_ENABLED: включает MSC (Mass Storage)
 */

#ifndef USB_COMPOSITE_CONFIG_H_
#define USB_COMPOSITE_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Определение флагов по умолчанию
//--------------------------------------------------------------------+

// Если ничего не определено — включаем оба
#if !defined(USB_CDC_ENABLED) && !defined(USB_MSC_ENABLED)
#define USB_CDC_ENABLED 1
#define USB_MSC_ENABLED 1
#endif

//--------------------------------------------------------------------+
// STM32H7 USB OTG Fix
//--------------------------------------------------------------------+

// STM32H7: TinyUSB ожидает USB_OTG_FS, а в H7 это USB2_OTG_FS
#if defined(STM32H7) || defined(STM32H743xx) || defined(STM32H750xx)
#include "stm32h7xx.h"
#ifndef USB_OTG_FS
#ifdef USB2_OTG_FS
#define USB_OTG_FS  USB2_OTG_FS
#endif
#endif
#endif

//--------------------------------------------------------------------+
// Board Configuration
//--------------------------------------------------------------------+

// RHPort: 0 = OTG_FS (PA11/PA12 на STM32H7)
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT      0
#endif

// Скорость: Full Speed для OTG_FS
#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED   OPT_MODE_FULL_SPEED
#endif

// RHPort0 mode: Device Full Speed
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

//--------------------------------------------------------------------+
// Common Configuration
//--------------------------------------------------------------------+

// MCU: STM32H7
#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU          OPT_MCU_STM32H7
#endif

// OS: без RTOS
#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS           OPT_OS_NONE
#endif

// Debug level (0 = off)
#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG        0
#endif

// Device stack
#define CFG_TUD_ENABLED       1
#define CFG_TUD_MAX_SPEED     BOARD_TUD_MAX_SPEED

//--------------------------------------------------------------------+
// Memory Configuration
//--------------------------------------------------------------------+

// STM32H7: буферы USB должны быть в Non-Cacheable памяти или cache-maintained
// Используйте секцию .dma_buffer в Linker Script
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION  __attribute__((section(".dma_buffer")))
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN    __attribute__((aligned(32)))
#endif

// DWC2: slave mode (без DMA)
#ifndef CFG_TUD_DWC2_DMA_ENABLE
#define CFG_TUD_DWC2_DMA_ENABLE   0
#endif

#ifndef CFG_TUD_DWC2_SLAVE_ENABLE
#define CFG_TUD_DWC2_SLAVE_ENABLE 1
#endif

//--------------------------------------------------------------------+
// Endpoint Configuration
//--------------------------------------------------------------------+

// Endpoint 0 max packet size
#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE    64
#endif

//--------------------------------------------------------------------+
// CDC Configuration
//--------------------------------------------------------------------+

#ifdef USB_CDC_ENABLED
#define CFG_TUD_CDC               1
#ifndef CFG_TUD_CDC_RX_BUFSIZE
#define CFG_TUD_CDC_RX_BUFSIZE    512
#endif
#ifndef CFG_TUD_CDC_TX_BUFSIZE
#define CFG_TUD_CDC_TX_BUFSIZE    512
#endif
#else
#define CFG_TUD_CDC               0
#endif

//--------------------------------------------------------------------+
// MSC Configuration
//--------------------------------------------------------------------+

#ifdef USB_MSC_ENABLED
#define CFG_TUD_MSC               1
#ifndef CFG_TUD_MSC_EP_BUFSIZE
#define CFG_TUD_MSC_EP_BUFSIZE    512
#endif
#else
#define CFG_TUD_MSC               0
#endif

//--------------------------------------------------------------------+
// Неиспользуемые классы
//--------------------------------------------------------------------+

#ifndef CFG_TUD_HID
#define CFG_TUD_HID               0
#endif

#ifndef CFG_TUD_MIDI
#define CFG_TUD_MIDI              0
#endif

#ifndef CFG_TUD_VENDOR
#define CFG_TUD_VENDOR            0
#endif

#ifdef __cplusplus
}
#endif

#endif // USB_COMPOSITE_CONFIG_H_
