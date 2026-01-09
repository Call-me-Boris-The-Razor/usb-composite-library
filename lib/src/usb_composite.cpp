/**
 * @file usb_composite.cpp
 * @brief USB Composite Device — реализация основного класса
 */

#include "usb_composite.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <atomic>

extern "C" {
#include "tusb.h"
}

// HAL для GPIO (toggle D+)
#if defined(STM32H7) || defined(STM32H743xx) || defined(STM32H750xx)
#include "stm32h7xx_hal.h"
#elif defined(STM32F4)
#include "stm32f4xx_hal.h"
#elif defined(STM32F7)
#include "stm32f7xx_hal.h"
#else
// Заглушка для других платформ
#define HAL_GPIO_WritePin(port, pin, state)
#define HAL_Delay(ms)
#define GPIO_PIN_SET   1
#define GPIO_PIN_RESET 0
#endif

// Forward declarations для платформозависимых функций
extern "C" void InitUsbGpio();
extern "C" void InitUsbClock();
extern "C" void InitUsbOtg();
extern "C" void InitUsbNvic();

namespace usb {

//--------------------------------------------------------------------+
// Глобальные переменные для TinyUSB callbacks
//--------------------------------------------------------------------+

static UsbDevice* g_usb_instance = nullptr;

#ifdef USB_CDC_ENABLED
static CdcRxCallback g_cdc_rx_callback = nullptr;
static void* g_cdc_rx_context = nullptr;
static CdcLineCodingCallback g_cdc_lc_callback = nullptr;
static void* g_cdc_lc_context = nullptr;
static DfuJumpCallback g_dfu_callback = nullptr;
static void* g_dfu_context = nullptr;

/// Флаг: терминал открыт (получен SET_LINE_CODING с baudrate != 1200)
static volatile bool g_terminal_opened = false;
#endif

#ifdef USB_MSC_ENABLED
static IBlockDevice* g_msc_device = nullptr;
static bool g_msc_ejected = false;

/// Атомарный счётчик активных MSC операций для корректного IsBusy
static std::atomic<int> g_msc_ops_count{0};

/// RAII-guard для счётчика операций MSC
struct MscBusyGuard {
    MscBusyGuard() { g_msc_ops_count.fetch_add(1, std::memory_order_relaxed); }
    ~MscBusyGuard() { g_msc_ops_count.fetch_sub(1, std::memory_order_relaxed); }
    MscBusyGuard(const MscBusyGuard&) = delete;
    MscBusyGuard& operator=(const MscBusyGuard&) = delete;
};
#endif

//--------------------------------------------------------------------+
// UsbDevice реализация
//--------------------------------------------------------------------+

bool UsbDevice::Init(const Config& config) {
    if (initialized_) {
        return true;
    }
    
    config_ = config;
    g_usb_instance = this;
    
    // Инициализация GPIO для USB (PA11/PA12)
    InitUsbGpio();
    
    // Инициализация тактирования USB
    InitUsbClock();
    
    // Настройка USB OTG регистров
    InitUsbOtg();
    
    // Настройка прерываний
    InitUsbNvic();
    
    // Инициализация TinyUSB
    if (!tusb_init()) {
        return false;
    }
    
    // Повторно применяем VBUS override (tusb_init делает сброс)
    InitUsbOtg();
    
    initialized_ = true;
    return true;
}

bool UsbDevice::Start() {
    if (!initialized_) {
        return false;
    }
    
    // Toggle D+ пин для перезапуска USB (если настроено)
    ToggleDpPin();
    
    return true;
}

void UsbDevice::Stop() {
    // TinyUSB device mode не имеет явного stop
}

void UsbDevice::Process() {
    if (initialized_) {
        tud_task();
    }
}

bool UsbDevice::IsConnected() const {
    return initialized_ && tud_ready();
}

State UsbDevice::GetState() const {
    if (!initialized_) {
        return State::NotInitialized;
    }
    if (tud_suspended()) {
        return State::Suspended;
    }
    if (tud_ready()) {
        return State::Configured;
    }
    if (tud_connected()) {
        return State::Connected;
    }
    return State::Disconnected;
}

void UsbDevice::ToggleDpPin() {
    if (config_.dp_toggle_pin.port == nullptr || config_.dp_toggle_ms == 0) {
        return;
    }
    
    auto* port = static_cast<GPIO_TypeDef*>(config_.dp_toggle_pin.port);
    uint16_t pin = static_cast<uint16_t>(1U << config_.dp_toggle_pin.pin);
    
    // Притягиваем D+ к земле
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
    HAL_Delay(config_.dp_toggle_ms);
    
    // Возвращаем в нормальное состояние (AF для USB)
    // Примечание: после этого нужно переинициализировать GPIO как AF
    // Это делается автоматически при следующем tusb_init или уже сделано
}

//--------------------------------------------------------------------+
// CDC методы
//--------------------------------------------------------------------+

#ifdef USB_CDC_ENABLED

bool UsbDevice::CdcIsConnected() const {
    return initialized_ && tud_cdc_connected();
}

uint32_t UsbDevice::CdcWrite(const uint8_t* data, uint32_t len) {
    if (!initialized_) return 0;
    
    uint32_t written = tud_cdc_write(data, len);
    tud_cdc_write_flush();
    return written;
}

uint32_t UsbDevice::CdcWrite(const char* str) {
    return CdcWrite(reinterpret_cast<const uint8_t*>(str), strlen(str));
}

uint32_t UsbDevice::CdcPrintf(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    if (len <= 0) return 0;
    return CdcWrite(reinterpret_cast<const uint8_t*>(buf), static_cast<uint32_t>(len));
}

uint32_t UsbDevice::CdcRead(uint8_t* buffer, uint32_t max_len) {
    if (!initialized_) return 0;
    return tud_cdc_read(buffer, max_len);
}

uint32_t UsbDevice::CdcAvailable() const {
    if (!initialized_) return 0;
    return tud_cdc_available();
}

void UsbDevice::CdcFlushRx() {
    if (initialized_) {
        tud_cdc_read_flush();
    }
}

void UsbDevice::CdcSetRxCallback(CdcRxCallback callback, void* context) {
    cdc_rx_callback_ = callback;
    cdc_rx_context_ = context;
    g_cdc_rx_callback = callback;
    g_cdc_rx_context = context;
}

void UsbDevice::CdcSetLineCodingCallback(CdcLineCodingCallback callback, void* context) {
    cdc_lc_callback_ = callback;
    cdc_lc_context_ = context;
    g_cdc_lc_callback = callback;
    g_cdc_lc_context = context;
}

void UsbDevice::CdcSetDfuCallback(DfuJumpCallback callback, void* context) {
    dfu_callback_ = callback;
    dfu_context_ = context;
    g_dfu_callback = callback;
    g_dfu_context = context;
}

bool UsbDevice::CdcTerminalOpened() const {
    return g_terminal_opened;
}

void UsbDevice::CdcResetTerminalFlag() {
    g_terminal_opened = false;
}

#endif // USB_CDC_ENABLED

//--------------------------------------------------------------------+
// MSC методы
//--------------------------------------------------------------------+

#ifdef USB_MSC_ENABLED

void UsbDevice::MscAttach(IBlockDevice* device) {
    msc_device_ = device;
    g_msc_device = device;
    g_msc_ejected = false;
}

void UsbDevice::MscDetach() {
    msc_device_ = nullptr;
    g_msc_device = nullptr;
}

bool UsbDevice::MscIsBusy() const {
    // Реальная проверка занятости через атомарный счётчик операций
    return g_msc_ops_count.load(std::memory_order_relaxed) > 0;
}

void UsbDevice::MscEject() {
    g_msc_ejected = true;
}

#endif // USB_MSC_ENABLED

//--------------------------------------------------------------------+
// Приватные методы инициализации (платформозависимые)
//--------------------------------------------------------------------+

// Слабые функции — могут быть переопределены в проекте

__attribute__((weak))
void InitUsbGpio() {
#if defined(STM32H7) || defined(STM32H743xx) || defined(STM32H750xx)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // PA11 = USB_DM, PA12 = USB_DP
    GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = 10;  // AF10 = OTG_FS
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
#endif
}

__attribute__((weak))
void InitUsbClock() {
#if defined(STM32H7) || defined(STM32H743xx) || defined(STM32H750xx)
    __HAL_RCC_USB2_OTG_FS_CLK_ENABLE();
    
    __HAL_RCC_USB2_OTG_FS_FORCE_RESET();
    HAL_Delay(2);
    __HAL_RCC_USB2_OTG_FS_RELEASE_RESET();
#endif
}

__attribute__((weak))
void InitUsbOtg() {
#if defined(STM32H7) || defined(STM32H743xx) || defined(STM32H750xx)
    #ifdef USB2_OTG_FS_PERIPH_BASE
    auto* USBx = reinterpret_cast<USB_OTG_GlobalTypeDef*>(USB2_OTG_FS_PERIPH_BASE);
    #else
    auto* USBx = reinterpret_cast<USB_OTG_GlobalTypeDef*>(0x40080000UL);
    #endif
    
    // Disable VBUS sensing
    USBx->GCCFG &= ~USB_OTG_GCCFG_VBDEN;
    
    // B-peripheral session valid override
    USBx->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
    USBx->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;
    
    // Enable transceiver (PWRDWN=1 на H7 = PHY включен)
    USBx->GCCFG |= USB_OTG_GCCFG_PWRDWN;
#endif
}

__attribute__((weak))
void InitUsbNvic() {
#if defined(STM32H7) || defined(STM32H743xx) || defined(STM32H750xx)
    HAL_NVIC_SetPriority(OTG_FS_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
#endif
}

}  // namespace usb

//--------------------------------------------------------------------+
// TinyUSB Callbacks (extern "C")
//--------------------------------------------------------------------+

extern "C" {

// Время в миллисекундах для TinyUSB (weak — можно переопределить в проекте)
__attribute__((weak))
uint32_t board_millis(void) {
#if defined(STM32H7) || defined(STM32H743xx) || defined(STM32H750xx) || \
    defined(STM32F4) || defined(STM32F7)
    return HAL_GetTick();
#else
    return 0;  // Переопределите в проекте!
#endif
}

// USB IRQ Handlers (weak — можно переопределить в проекте)
#if defined(STM32H7) || defined(STM32H743xx) || defined(STM32H750xx)

__attribute__((weak))
void OTG_FS_IRQHandler(void) {
    tusb_int_handler(0, true);
}

// Fallback для плат где OTG_FS_IRQn указывает на HS (некоторые версии HAL)
__attribute__((weak))
void OTG_HS_IRQHandler(void) {
    tusb_int_handler(0, true);
}

#endif

//--------------------------------------------------------------------+
// CDC Callbacks
//--------------------------------------------------------------------+

#if CFG_TUD_CDC

void tud_cdc_rx_cb(uint8_t itf) {
    (void)itf;
    
#ifdef USB_CDC_ENABLED
    // Вызываем callback если установлен
    if (usb::g_cdc_rx_callback != nullptr && tud_cdc_available()) {
        uint8_t buf[64];
        uint32_t count = tud_cdc_read(buf, sizeof(buf));
        if (count > 0) {
            usb::g_cdc_rx_callback(buf, count, usb::g_cdc_rx_context);
        }
    }
#else
    tud_cdc_read_flush();
#endif
}

void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const* p_line_coding) {
    (void)itf;
    
#ifdef USB_CDC_ENABLED
    uint32_t baudrate = p_line_coding->bit_rate;
    
    // 1200 bps = Magic baud rate для DFU
    if (baudrate == usb::kDfuBaudrate) {
        // Вызываем DFU callback если установлен
        if (usb::g_dfu_callback != nullptr) {
            usb::g_dfu_callback(usb::g_dfu_context);
        }
    } else {
        // Любой другой baudrate = терминал открылся
        usb::g_terminal_opened = true;
    }
    
    // Вызываем пользовательский callback (если установлен)
    if (usb::g_cdc_lc_callback != nullptr) {
        usb::g_cdc_lc_callback(baudrate, usb::g_cdc_lc_context);
    }
#endif
}

#endif // CFG_TUD_CDC

//--------------------------------------------------------------------+
// MSC Callbacks
//--------------------------------------------------------------------+

#if CFG_TUD_MSC

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], 
                        uint8_t product_id[16], uint8_t product_rev[4]) {
    (void)lun;
    
    const char vid[] = "USB";
    const char pid[] = "Composite MSC";
    const char rev[] = "1.0";
    
    memset(vendor_id, ' ', 8);
    memset(product_id, ' ', 16);
    memset(product_rev, ' ', 4);
    
    memcpy(vendor_id, vid, strlen(vid));
    memcpy(product_id, pid, strlen(pid));
    memcpy(product_rev, rev, strlen(rev));
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void)lun;
    
#ifdef USB_MSC_ENABLED
    if (usb::g_msc_ejected) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);
        return false;
    }
    
    if (usb::g_msc_device == nullptr || !usb::g_msc_device->IsReady()) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);
        return false;
    }
    
    return true;
#else
    return false;
#endif
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
    (void)lun;
    
#ifdef USB_MSC_ENABLED
    if (usb::g_msc_device != nullptr && usb::g_msc_device->IsReady()) {
        *block_count = usb::g_msc_device->GetBlockCount();
        *block_size = static_cast<uint16_t>(usb::g_msc_device->GetBlockSize());
    } else {
        *block_count = 0;
        *block_size = 512;
    }
#else
    *block_count = 0;
    *block_size = 512;
#endif
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, 
                           bool start, bool load_eject) {
    (void)lun;
    (void)power_condition;
    
#ifdef USB_MSC_ENABLED
    if (load_eject) {
        usb::g_msc_ejected = !start;
    }
#endif
    
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, 
                          void* buffer, uint32_t bufsize) {
    (void)lun;
    (void)offset;
    
#ifdef USB_MSC_ENABLED
    if (usb::g_msc_device == nullptr || !usb::g_msc_device->IsReady() || usb::g_msc_ejected) {
        return -1;
    }
    
    // RAII-guard для отслеживания занятости
    usb::MscBusyGuard busy_guard;
    
    uint32_t block_size = usb::g_msc_device->GetBlockSize();
    uint32_t block_count = bufsize / block_size;
    if (block_count == 0) return 0;
    
    if (!usb::g_msc_device->Read(lba, static_cast<uint8_t*>(buffer), block_count)) {
        return -1;
    }
    
    return static_cast<int32_t>(bufsize);
#else
    return -1;
#endif
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, 
                           uint8_t* buffer, uint32_t bufsize) {
    (void)lun;
    (void)offset;
    
#ifdef USB_MSC_ENABLED
    if (usb::g_msc_device == nullptr || !usb::g_msc_device->IsReady() || usb::g_msc_ejected) {
        return -1;
    }
    
    // RAII-guard для отслеживания занятости
    usb::MscBusyGuard busy_guard;
    
    uint32_t block_size = usb::g_msc_device->GetBlockSize();
    uint32_t block_count = bufsize / block_size;
    if (block_count == 0) return 0;
    
    if (!usb::g_msc_device->Write(lba, buffer, block_count)) {
        return -1;
    }
    
    return static_cast<int32_t>(bufsize);
#else
    return -1;
#endif
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], 
                        void* buffer, uint16_t bufsize) {
    (void)buffer;
    (void)bufsize;
    
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
    return -1;
}

#endif // CFG_TUD_MSC

} // extern "C"
