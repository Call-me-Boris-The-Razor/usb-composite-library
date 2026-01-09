/**
 * @file main_composite.cpp
 * @brief USB Composite Library - CDC + MSC Test
 * 
 * Минимальный composite: COM порт + USB флешка
 * Библиотека сама настроит PLL если нужно!
 */

#include "stm32h7xx_hal.h"
#include "usb_composite.h"
#include "usb_sdmmc.h"

usb::UsbDevice g_usb;
usb::SdmmcBlockDevice g_sd;

int main(void) {
    HAL_Init();
    
    // SD карта — библиотека сама настроит PLL!
    usb::SdmmcConfig sd_cfg;
    sd_cfg.instance = SDMMC1;
    sd_cfg.use_4bit_mode = true;
    
    g_sd.Init(sd_cfg);
    
    // USB
    g_usb.Init();
    
    if (g_sd.IsReady()) {
        g_usb.MscAttach(&g_sd);
    }
    
    g_usb.Start();
    
    uint32_t counter = 0;
    
    while (1) {
        g_usb.Process();
        
        static uint32_t last = 0;
        if (HAL_GetTick() - last >= 1000) {
            last = HAL_GetTick();
            
            if (g_usb.CdcIsConnected()) {
                g_usb.CdcPrintf("OkoRelay Composite #%lu\r\n", ++counter);
                g_usb.CdcPrintf("  SD: %s, %lu MB\r\n", 
                    g_sd.IsReady() ? "OK" : "FAIL",
                    g_sd.IsReady() ? (g_sd.GetBlockCount() * g_sd.GetBlockSize() / 1024 / 1024) : 0);
            }
        }
    }
}
