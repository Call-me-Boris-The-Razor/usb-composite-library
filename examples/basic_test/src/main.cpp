/**
 * @file main.cpp
 * @brief USB Composite Library - Basic Test
 * 
 * Тест CDC (Virtual COM Port)
 * После прошивки должен появиться COM порт
 */

#include "stm32h7xx_hal.h"
#include "usb_composite.h"

usb::UsbDevice g_usb;

void SystemClock_Config(void);
void Error_Handler(void);

int main(void) {
    // HAL Init
    HAL_Init();
    
    // System Clock: 480 MHz from 25 MHz HSE
    SystemClock_Config();
    
    // USB Init
    usb::Config cfg;
    cfg.dp_toggle_ms = 0;  // Без toggle
    
    bool init_ok = g_usb.Init(cfg);
    g_usb.Start();
    
    // Получаем диагностику
    auto diag = g_usb.GetDiagnostics();
    
    uint32_t last_print = 0;
    uint32_t counter = 0;
    
    while (1) {
        g_usb.Process();
        
        // Каждую секунду выводим статус
        if (HAL_GetTick() - last_print >= 1000) {
            last_print = HAL_GetTick();
            counter++;
            
            if (g_usb.CdcIsConnected()) {
                g_usb.CdcPrintf("USB Composite Test #%lu\r\n", counter);
                g_usb.CdcPrintf("  State: %d\r\n", (int)g_usb.GetState());
                g_usb.CdcPrintf("  tusb_init: %s\r\n", diag.tusb_init_ok ? "OK" : "FAIL");
                g_usb.CdcPrintf("  GCCFG: 0x%08lX\r\n", diag.gccfg);
            }
        }
    }
}

/**
 * @brief System Clock Configuration
 * 480 MHz from 25 MHz HSE
 */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // Supply configuration
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    // Voltage scaling
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    // HSE + PLL1
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;  // For USB
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 5;   // 25 MHz / 5 = 5 MHz
    RCC_OscInitStruct.PLL.PLLN = 192; // 5 MHz * 192 = 960 MHz
    RCC_OscInitStruct.PLL.PLLP = 2;   // 960 / 2 = 480 MHz
    RCC_OscInitStruct.PLL.PLLQ = 4;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    // Clock configuration
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
        Error_Handler();
    }

    // USB Clock from HSI48
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
    PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        Error_Handler();
    }

    // Enable USB Voltage detector
    HAL_PWREx_EnableUSBVoltageDetector();
}

void Error_Handler(void) {
    __disable_irq();
    while (1) {}
}

// SysTick handler
extern "C" void SysTick_Handler(void) {
    HAL_IncTick();
}
