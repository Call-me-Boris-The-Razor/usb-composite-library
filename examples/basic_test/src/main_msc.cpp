/**
 * @file main_msc.cpp
 * @brief USB Composite Library - Minimal MSC Test
 * 
 * Минимальный пример: только HAL_Init() + SD + USB!
 * Всё остальное делает библиотека:
 * - HSI48 для USB
 * - SDMMC Clock от PLL
 * - SysTick_Handler
 * - IRQ Handlers
 * 
 * ВАЖНО: USB MSC инициализируется ТОЛЬКО после готовности SD карты!
 * Иначе Windows Explorer может крашнуться при попытке доступа к устройству
 * с нулевой ёмкостью.
 */

#include "stm32h7xx_hal.h"
#include "usb_composite.h"
#include "usb_sdmmc.h"

usb::UsbDevice g_usb;
usb::SdmmcBlockDevice g_sd;

/// Таймаут ожидания готовности SD карты (мс)
static constexpr uint32_t kSdReadyTimeoutMs = 3000;

int main(void) {
    // Только HAL_Init — всё остальное делает библиотека!
    HAL_Init();
    
    // SD карта — библиотека сама настроит SDMMC Clock
    usb::SdmmcConfig sd_cfg;
    sd_cfg.use_4bit_mode = true;
    
    if (!g_sd.Init(sd_cfg)) {
        // Ошибка инициализации SD — не запускаем USB MSC
        // Можно добавить индикацию ошибки (LED, etc.)
        while (1) {
            HAL_Delay(500);
        }
    }
    
    // Ждём готовности SD карты с таймаутом
    uint32_t start = HAL_GetTick();
    while (!g_sd.IsReady() && (HAL_GetTick() - start) < kSdReadyTimeoutMs) {
        HAL_Delay(10);
    }
    
    if (!g_sd.IsReady()) {
        // SD карта не готова — не запускаем USB MSC
        while (1) {
            HAL_Delay(500);
        }
    }
    
    // USB — инициализируем ПОСЛЕ готовности SD!
    g_usb.Init();
    g_usb.MscAttach(&g_sd);
    g_usb.Start();
    
    while (1) {
        g_usb.Process();
        
        // Опционально: проверка состояния SD карты
        // Если карта извлечена, можно вызвать g_usb.MscEject()
    }
}
