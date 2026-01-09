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
 */

#include "stm32h7xx_hal.h"
#include "usb_composite.h"
#include "usb_sdmmc.h"

usb::UsbDevice g_usb;
usb::SdmmcBlockDevice g_sd;

int main(void) {
    // Только HAL_Init — всё остальное делает библиотека!
    HAL_Init();
    
    // SD карта — библиотека сама настроит SDMMC Clock
    usb::SdmmcConfig sd_cfg;
    sd_cfg.instance = SDMMC1;
    sd_cfg.use_4bit_mode = true;
    
    g_sd.Init(sd_cfg);
    
    // USB — работает из коробки
    g_usb.Init();
    
    if (g_sd.IsReady()) {
        g_usb.MscAttach(&g_sd);
    }
    
    g_usb.Start();
    
    while (1) {
        g_usb.Process();
    }
}
