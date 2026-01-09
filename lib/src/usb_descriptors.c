/**
 * @file usb_descriptors.c
 * @brief USB дескрипторы для Composite Device (CDC + MSC)
 * 
 * Автоматически генерирует дескрипторы в зависимости от флагов:
 * - USB_CDC_ENABLED: добавляет CDC интерфейсы
 * - USB_MSC_ENABLED: добавляет MSC интерфейс
 */

#include "tusb.h"
#include <string.h>

//--------------------------------------------------------------------+
// Подсчёт интерфейсов
//--------------------------------------------------------------------+

#ifdef USB_CDC_ENABLED
#define CDC_ITF_COUNT 2  // CDC Control + CDC Data
#else
#define CDC_ITF_COUNT 0
#endif

#ifdef USB_MSC_ENABLED
#define MSC_ITF_COUNT 1
#else
#define MSC_ITF_COUNT 0
#endif

#define ITF_NUM_TOTAL (CDC_ITF_COUNT + MSC_ITF_COUNT)

//--------------------------------------------------------------------+
// Номера интерфейсов
//--------------------------------------------------------------------+

enum {
#ifdef USB_CDC_ENABLED
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
#endif
#ifdef USB_MSC_ENABLED
    #ifdef USB_CDC_ENABLED
    ITF_NUM_MSC = 2,
    #else
    ITF_NUM_MSC = 0,
    #endif
#endif
    _ITF_NUM_TOTAL = ITF_NUM_TOTAL
};

//--------------------------------------------------------------------+
// Endpoint адреса
//--------------------------------------------------------------------+

#ifdef USB_CDC_ENABLED
#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82
#endif

#ifdef USB_MSC_ENABLED
    #ifdef USB_CDC_ENABLED
    #define EPNUM_MSC_OUT     0x03
    #define EPNUM_MSC_IN      0x83
    #else
    #define EPNUM_MSC_OUT     0x01
    #define EPNUM_MSC_IN      0x81
    #endif
#endif

//--------------------------------------------------------------------+
// Размер конфигурации
//--------------------------------------------------------------------+

#ifdef USB_CDC_ENABLED
#define CDC_DESC_LEN TUD_CDC_DESC_LEN
#else
#define CDC_DESC_LEN 0
#endif

#ifdef USB_MSC_ENABLED
#define MSC_DESC_LEN TUD_MSC_DESC_LEN
#else
#define MSC_DESC_LEN 0
#endif

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CDC_DESC_LEN + MSC_DESC_LEN)

//--------------------------------------------------------------------+
// VID/PID (можно переопределить через build_flags)
//--------------------------------------------------------------------+

#ifndef USB_VID
#define USB_VID   0x0483  // ST Microelectronics
#endif

#ifndef USB_PID
#define USB_PID   0x5743  // CDC + MSC Composite
#endif

#ifndef USB_BCD
#define USB_BCD   0x0200  // USB 2.0
#endif

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+

static tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,

#if ITF_NUM_TOTAL > 1
    // Composite device: использовать IAD
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
#elif defined(USB_CDC_ENABLED)
    // Только CDC
    .bDeviceClass       = TUSB_CLASS_CDC,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
#elif defined(USB_MSC_ENABLED)
    // Только MSC
    .bDeviceClass       = 0,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
#else
    .bDeviceClass       = 0,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
#endif

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

uint8_t const* tud_descriptor_device_cb(void) {
    return (uint8_t const*)&desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

static uint8_t const desc_fs_configuration[] = {
    // Config: number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

#ifdef USB_CDC_ENABLED
    // CDC: Interface number, string index, EP notification, EP data out, EP data in
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
#endif

#ifdef USB_MSC_ENABLED
    // MSC: Interface number, string index, EP out, EP in, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 5, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
#endif
};

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_fs_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// Строковые дескрипторы (можно переопределить через build_flags)
#ifndef USB_STR_MANUFACTURER
#define USB_STR_MANUFACTURER "STM32"
#endif

#ifndef USB_STR_PRODUCT
#define USB_STR_PRODUCT "USB Composite"
#endif

#ifndef USB_STR_SERIAL
#define USB_STR_SERIAL "123456"
#endif

#ifndef USB_STR_CDC
#define USB_STR_CDC "CDC Port"
#endif

#ifndef USB_STR_MSC
#define USB_STR_MSC "Storage"
#endif

static char const* string_desc_arr[] = {
    NULL,                    // 0: Language (handled separately)
    USB_STR_MANUFACTURER,    // 1: Manufacturer
    USB_STR_PRODUCT,         // 2: Product
    USB_STR_SERIAL,          // 3: Serial
    USB_STR_CDC,             // 4: CDC Interface
    USB_STR_MSC,             // 5: MSC Interface
};

static uint16_t desc_str[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;

    uint8_t chr_count;

    if (index == 0) {
        // Language ID: English (US)
        desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | 4);
        desc_str[1] = 0x0409;
        return desc_str;
    }

    if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
        return NULL;
    }

    const char* str = string_desc_arr[index];
    if (str == NULL) {
        return NULL;
    }

    chr_count = (uint8_t)strlen(str);
    if (chr_count > 31) {
        chr_count = 31;
    }

    // Convert ASCII to UTF-16
    for (uint8_t i = 0; i < chr_count; i++) {
        desc_str[1 + i] = str[i];
    }

    // First byte = length (including header), second byte = descriptor type
    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

    return desc_str;
}
