/**
 * @file main.cpp
 * @brief USB Composite Library - Minimal CDC Test
 * 
 * Минимальный пример: только HAL_Init() и USB!
 * Всё остальное делает библиотека:
 * - HSI48 для USB
 * - SysTick_Handler
 * - IRQ Handlers
 * - DFU через 1200 bps
 */

#include "stm32h7xx_hal.h"
#include "usb_composite.h"

usb::UsbDevice g_usb;

int main(void) {
    // Только HAL_Init — всё остальное делает библиотека!
    HAL_Init();
    
    // USB — работает из коробки
    g_usb.Init();
    g_usb.Start();
    
    uint32_t counter = 0;
    
    while (1) {
        g_usb.Process();
        
        // Каждую секунду выводим статус
        static uint32_t last = 0;
        if (HAL_GetTick() - last >= 1000) {
            last = HAL_GetTick();
            
            if (g_usb.CdcIsConnected()) {
                g_usb.CdcPrintf("USB CDC Test #%lu\r\n", ++counter);
            }
        }
    }
}
