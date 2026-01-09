/**
 * @file usb_sdmmc.cpp
 * @brief Реализация SDMMC Block Device для USB MSC
 * 
 * v3.0.0: pImpl паттерн для скрытия HAL деталей
 */

#include "usb_sdmmc.h"

#if defined(USB_MSC_ENABLED) && defined(USB_SDMMC_ENABLED)

#include "stm32h7xx_hal.h"
#include <cstring>

namespace usb {

// ============ Константы ============

static constexpr uint32_t kMaxPhysBlockSize = 2048;
static constexpr uint32_t kLogBlockSize = SdmmcBlockDevice::kBlockSize;

// ============ pImpl структура (HAL детали внутри!) ============

struct SdmmcImpl {
    SD_HandleTypeDef hsd = {};
    SdmmcConfig config = {};
    SdmmcState state = SdmmcState::NotInitialized;
    SdmmcCardInfo card_info = {};
    uint32_t phys_block_size = 512;
    
    // Буферы (теперь члены класса, не глобальные!)
    uint8_t phys_buffer[kMaxPhysBlockSize] __attribute__((aligned(4)));
    uint8_t cache_buffer[kMaxPhysBlockSize] __attribute__((aligned(4)));
    uint32_t cached_phys_lba = UINT32_MAX;
    bool cache_dirty = false;
    
    // Вспомогательные методы
    GPIO_TypeDef* GetGpioPort(uint8_t port_index);
    uint16_t GetGpioPin(uint8_t pin_number);
    SDMMC_TypeDef* GetSdmmcInstance(uint8_t index);
    void InitGpio();
    void DeInitGpio();
    bool WaitReady(uint32_t timeout_ms);
    bool ReadDirect(uint32_t lba, uint8_t* buffer, uint32_t count);
    bool WriteDirect(uint32_t lba, const uint8_t* buffer, uint32_t count);
    bool FlushCache();
};

// ============ SdmmcImpl вспомогательные методы ============

GPIO_TypeDef* SdmmcImpl::GetGpioPort(uint8_t port_index) {
    static GPIO_TypeDef* const ports[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH};
    if (port_index < sizeof(ports)/sizeof(ports[0])) {
        return ports[port_index];
    }
    return GPIOC;  // Default
}

uint16_t SdmmcImpl::GetGpioPin(uint8_t pin_number) {
    return static_cast<uint16_t>(1U << pin_number);
}

SDMMC_TypeDef* SdmmcImpl::GetSdmmcInstance(uint8_t index) {
    if (index == 1) return SDMMC1;
#ifdef SDMMC2
    if (index == 2) return SDMMC2;
#endif
    return SDMMC1;
}

// ============ Реализация SdmmcBlockDevice ============

SdmmcBlockDevice::SdmmcBlockDevice() : impl_(new SdmmcImpl{}) {}

SdmmcBlockDevice::~SdmmcBlockDevice() {
    DeInit();
    delete impl_;
}

bool SdmmcBlockDevice::Init(const SdmmcConfig& config) {
    if (impl_->state != SdmmcState::NotInitialized) {
        return true;
    }
    
    impl_->config = config;
    impl_->cached_phys_lba = UINT32_MAX;
    impl_->cache_dirty = false;
    impl_->phys_block_size = kLogBlockSize;
    
    // 1. Если PLL не готов - настраиваем автоматически
    if (!__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY)) {
        RCC_OscInitTypeDef RCC_OscInit = {0};
        RCC_ClkInitTypeDef RCC_ClkInit = {0};
        
        HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
        __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
        while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
        
        RCC_OscInit.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48;
        RCC_OscInit.HSEState = RCC_HSE_ON;
        RCC_OscInit.HSI48State = RCC_HSI48_ON;
        RCC_OscInit.PLL.PLLState = RCC_PLL_ON;
        RCC_OscInit.PLL.PLLSource = RCC_PLLSOURCE_HSE;
        
        #if defined(HSE_VALUE) && (HSE_VALUE == 25000000)
        RCC_OscInit.PLL.PLLM = 5;
        RCC_OscInit.PLL.PLLN = 192;
        #elif defined(HSE_VALUE) && (HSE_VALUE == 8000000)
        RCC_OscInit.PLL.PLLM = 1;
        RCC_OscInit.PLL.PLLN = 120;
        #else
        RCC_OscInit.PLL.PLLSource = RCC_PLLSOURCE_HSI;
        RCC_OscInit.PLL.PLLM = 8;
        RCC_OscInit.PLL.PLLN = 120;
        #endif
        
        RCC_OscInit.PLL.PLLP = 2;
        RCC_OscInit.PLL.PLLQ = 4;
        RCC_OscInit.PLL.PLLR = 2;
        RCC_OscInit.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
        RCC_OscInit.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
        RCC_OscInit.PLL.PLLFRACN = 0;
        
        if (HAL_RCC_OscConfig(&RCC_OscInit) != HAL_OK) {
            RCC_OscInit.HSEState = RCC_HSE_OFF;
            RCC_OscInit.PLL.PLLSource = RCC_PLLSOURCE_HSI;
            RCC_OscInit.PLL.PLLM = 8;
            RCC_OscInit.PLL.PLLN = 120;
            HAL_RCC_OscConfig(&RCC_OscInit);
        }
        
        RCC_ClkInit.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                                RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
        RCC_ClkInit.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
        RCC_ClkInit.SYSCLKDivider = RCC_SYSCLK_DIV1;
        RCC_ClkInit.AHBCLKDivider = RCC_HCLK_DIV2;
        RCC_ClkInit.APB3CLKDivider = RCC_APB3_DIV2;
        RCC_ClkInit.APB1CLKDivider = RCC_APB1_DIV2;
        RCC_ClkInit.APB2CLKDivider = RCC_APB2_DIV2;
        RCC_ClkInit.APB4CLKDivider = RCC_APB4_DIV2;
        HAL_RCC_ClockConfig(&RCC_ClkInit, FLASH_LATENCY_4);
        
        HAL_PWREx_EnableUSBVoltageDetector();
    }
    
    // 2. Настраиваем SDMMC clock
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_SDMMC;
    PeriphClkInit.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
    
    // 3. Включаем тактирование SDMMC
    SDMMC_TypeDef* sdmmc = impl_->GetSdmmcInstance(config.sdmmc_index);
    if (sdmmc == SDMMC1) {
        __HAL_RCC_SDMMC1_CLK_ENABLE();
        __HAL_RCC_SDMMC1_FORCE_RESET();
        HAL_Delay(10);
        __HAL_RCC_SDMMC1_RELEASE_RESET();
        HAL_Delay(10);
    }
#ifdef SDMMC2
    else if (sdmmc == SDMMC2) {
        __HAL_RCC_SDMMC2_CLK_ENABLE();
        __HAL_RCC_SDMMC2_FORCE_RESET();
        HAL_Delay(10);
        __HAL_RCC_SDMMC2_RELEASE_RESET();
        HAL_Delay(10);
    }
#endif
    
    // 4. Включаем тактирование GPIO
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    
    // 5. Инициализация GPIO
    impl_->InitGpio();
    
    // 6. Конфигурация HAL SD
    impl_->hsd.Instance = sdmmc;
    impl_->hsd.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    impl_->hsd.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    impl_->hsd.Init.BusWide = SDMMC_BUS_WIDE_1B;
    impl_->hsd.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    impl_->hsd.Init.ClockDiv = config.init_clock_div;
    
    // 7. Инициализация карты
    if (HAL_SD_Init(&impl_->hsd) != HAL_OK) {
        impl_->state = SdmmcState::Error;
        return false;
    }
    
    // 8. Получаем информацию о карте
    HAL_SD_CardInfoTypeDef hal_info;
    if (HAL_SD_GetCardInfo(&impl_->hsd, &hal_info) != HAL_OK) {
        impl_->state = SdmmcState::Error;
        return false;
    }
    
    // 9. Парсим CSD если нужно
    if (hal_info.BlockNbr == 0) {
        uint32_t csd_struct = (impl_->hsd.CSD[3] >> 30) & 0x03;
        if (csd_struct == 1) {
            uint32_t c_size = ((impl_->hsd.CSD[2] & 0x3F) << 16) | 
                              ((impl_->hsd.CSD[1] >> 16) & 0xFFFF);
            uint64_t capacity = (static_cast<uint64_t>(c_size) + 1) * 512 * 1024;
            hal_info.BlockNbr = capacity / kLogBlockSize;
        }
    } else if (hal_info.BlockSize > kLogBlockSize) {
        uint32_t mult = hal_info.BlockSize / kLogBlockSize;
        hal_info.BlockNbr *= mult;
    }
    
    // 10. Переключение на 4-bit режим
    if (config.use_4bit_mode) {
        if (HAL_SD_ConfigWideBusOperation(&impl_->hsd, SDMMC_BUS_WIDE_4B) == HAL_OK) {
            SDMMC_InitTypeDef init;
            init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
            init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
            init.BusWide = SDMMC_BUS_WIDE_4B;
            init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
            init.ClockDiv = config.normal_clock_div;
            SDMMC_Init(impl_->hsd.Instance, init);
        }
    }
    
    // 11. Заполняем информацию о карте
    impl_->card_info.block_count = hal_info.BlockNbr;
    impl_->card_info.block_size = kLogBlockSize;
    impl_->card_info.capacity_bytes = static_cast<uint64_t>(hal_info.BlockNbr) * kLogBlockSize;
    impl_->card_info.card_type = impl_->hsd.SdCard.CardType;
    impl_->card_info.card_version = impl_->hsd.SdCard.CardVersion;
    impl_->card_info.is_ready = true;
    
    if (impl_->card_info.block_count == 0) {
        impl_->state = SdmmcState::Error;
        return false;
    }
    
    impl_->state = SdmmcState::Ready;
    return true;
}

void SdmmcBlockDevice::DeInit() {
    if (impl_->state == SdmmcState::NotInitialized) {
        return;
    }
    
    impl_->FlushCache();
    HAL_SD_DeInit(&impl_->hsd);
    impl_->DeInitGpio();
    
    SDMMC_TypeDef* sdmmc = impl_->GetSdmmcInstance(impl_->config.sdmmc_index);
    if (sdmmc == SDMMC1) {
        __HAL_RCC_SDMMC1_CLK_DISABLE();
    }
#ifdef SDMMC2
    else if (sdmmc == SDMMC2) {
        __HAL_RCC_SDMMC2_CLK_DISABLE();
    }
#endif
    
    impl_->state = SdmmcState::NotInitialized;
    impl_->card_info = {};
}

// ============ SdmmcImpl методы ============

void SdmmcImpl::InitGpio() {
    GPIO_InitTypeDef gpio = {};
    uint8_t af = (config.sdmmc_index == 1) ? GPIO_AF12_SDMMC1 : GPIO_AF12_SDMMC1;
    
    // CLK
    gpio.Pin = GetGpioPin(config.clk.pin_number);
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = af;
    HAL_GPIO_Init(GetGpioPort(config.clk.port_index), &gpio);
    
    // CMD
    gpio.Pin = GetGpioPin(config.cmd.pin_number);
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GetGpioPort(config.cmd.port_index), &gpio);
    
    // D0
    gpio.Pin = GetGpioPin(config.d0.pin_number);
    HAL_GPIO_Init(GetGpioPort(config.d0.port_index), &gpio);
    
    // D1-D3 (if 4-bit mode)
    if (config.use_4bit_mode) {
        gpio.Pin = GetGpioPin(config.d1.pin_number);
        HAL_GPIO_Init(GetGpioPort(config.d1.port_index), &gpio);
        gpio.Pin = GetGpioPin(config.d2.pin_number);
        HAL_GPIO_Init(GetGpioPort(config.d2.port_index), &gpio);
        gpio.Pin = GetGpioPin(config.d3.pin_number);
        HAL_GPIO_Init(GetGpioPort(config.d3.port_index), &gpio);
    }
}

void SdmmcImpl::DeInitGpio() {
    HAL_GPIO_DeInit(GetGpioPort(config.clk.port_index), GetGpioPin(config.clk.pin_number));
    HAL_GPIO_DeInit(GetGpioPort(config.cmd.port_index), GetGpioPin(config.cmd.pin_number));
    HAL_GPIO_DeInit(GetGpioPort(config.d0.port_index), GetGpioPin(config.d0.pin_number));
    
    if (config.use_4bit_mode) {
        HAL_GPIO_DeInit(GetGpioPort(config.d1.port_index), GetGpioPin(config.d1.pin_number));
        HAL_GPIO_DeInit(GetGpioPort(config.d2.port_index), GetGpioPin(config.d2.pin_number));
        HAL_GPIO_DeInit(GetGpioPort(config.d3.port_index), GetGpioPin(config.d3.pin_number));
    }
}

bool SdmmcImpl::WaitReady(uint32_t timeout_ms) {
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout_ms) {
        if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER) {
            return true;
        }
        HAL_Delay(1);
    }
    return false;
}

bool SdmmcImpl::ReadDirect(uint32_t lba, uint8_t* buffer, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        if (HAL_SD_ReadBlocks(&hsd, phys_buffer, lba + i, 1, config.rw_timeout_ms) != HAL_OK) {
            return false;
        }
        if (!WaitReady(config.rw_timeout_ms)) {
            return false;
        }
        memcpy(buffer + i * kLogBlockSize, phys_buffer, kLogBlockSize);
    }
    return true;
}

bool SdmmcImpl::WriteDirect(uint32_t lba, const uint8_t* buffer, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        memcpy(phys_buffer, buffer + i * kLogBlockSize, kLogBlockSize);
        if (HAL_SD_WriteBlocks(&hsd, phys_buffer, lba + i, 1, config.rw_timeout_ms) != HAL_OK) {
            return false;
        }
        if (!WaitReady(config.rw_timeout_ms)) {
            return false;
        }
    }
    return true;
}

bool SdmmcImpl::FlushCache() {
    if (!cache_dirty || cached_phys_lba == UINT32_MAX) {
        return true;
    }
    if (HAL_SD_WriteBlocks(&hsd, cache_buffer, cached_phys_lba, 1, config.rw_timeout_ms) != HAL_OK) {
        return false;
    }
    if (!WaitReady(config.rw_timeout_ms)) {
        return false;
    }
    cache_dirty = false;
    return true;
}

// ============ SdmmcBlockDevice публичные методы ============

bool SdmmcBlockDevice::IsCardInserted() const {
    if (impl_->state != SdmmcState::Ready) {
        return false;
    }
    return HAL_SD_GetCardState(&impl_->hsd) == HAL_SD_CARD_TRANSFER;
}

bool SdmmcBlockDevice::IsReady() const {
    return impl_->state == SdmmcState::Ready && impl_->card_info.is_ready;
}

uint32_t SdmmcBlockDevice::GetBlockCount() const {
    return impl_->card_info.block_count;
}

uint32_t SdmmcBlockDevice::GetBlockSize() const {
    return kBlockSize;
}

SdmmcCardInfo SdmmcBlockDevice::GetCardInfo() const {
    return impl_->card_info;
}

SdmmcState SdmmcBlockDevice::GetState() const {
    return impl_->state;
}

SdmmcDiagnostics SdmmcBlockDevice::GetDiagnostics() const {
    SdmmcDiagnostics diag = {};
    diag.hal_error = HAL_SD_GetError(&impl_->hsd);
    diag.hal_state = impl_->hsd.State;
    diag.sdmmc_sta = impl_->hsd.Instance->STA;
    diag.sdmmc_resp1 = impl_->hsd.Instance->RESP1;
    return diag;
}

bool SdmmcBlockDevice::Sync() {
    return impl_->FlushCache();
}

bool SdmmcBlockDevice::Read(uint32_t lba, uint8_t* buffer, uint32_t count) {
    if (impl_->state != SdmmcState::Ready || buffer == nullptr || count == 0) {
        return false;
    }
    return impl_->ReadDirect(lba, buffer, count);
}

bool SdmmcBlockDevice::Write(uint32_t lba, const uint8_t* buffer, uint32_t count) {
    if (impl_->state != SdmmcState::Ready || buffer == nullptr || count == 0) {
        return false;
    }
    return impl_->WriteDirect(lba, buffer, count);
}

}  // namespace usb

// ============ HAL MSP Callbacks ============

extern "C" void HAL_SD_MspInit(SD_HandleTypeDef* hsd) {
    // GPIO инициализируется в SdmmcBlockDevice::Init()
    (void)hsd;
}

extern "C" void HAL_SD_MspDeInit(SD_HandleTypeDef* hsd) {
    // GPIO деинициализируется в SdmmcBlockDevice::DeInit()
    (void)hsd;
}

#endif  // USB_MSC_ENABLED && USB_SDMMC_ENABLED
