/**
 * @file usb_sdmmc.cpp
 * @brief Реализация SDMMC Block Device для USB MSC
 * 
 * Особенности:
 * - Sector Translation Layer для SD NAND (1024-байт физ. секторы)
 * - DMA bounce buffers в RAM_D2 (non-cacheable)
 * - Кэширование последнего физического блока для оптимизации RMW
 * - Поддержка SD/SDHC/SDXC
 */

#include "usb_sdmmc.h"

#if defined(USB_MSC_ENABLED) && defined(USB_SDMMC_ENABLED)

#include <cstring>

namespace usb {

// ============ Константы Sector Translation Layer ============

/// Максимальный размер физического сектора SD NAND (2048 байт для READ_BL_LEN=11)
static constexpr uint32_t kMaxPhysBlockSize = 2048;

/// Размер логического сектора (512 байт, для USB MSC)
static constexpr uint32_t kLogBlockSize = SdmmcBlockDevice::kBlockSize;

// ============ Буферы для Sector Translation Layer ============
// 
// Используем HAL_SD_ReadBlocks/WriteBlocks в polling mode (не DMA),
// поэтому буферы могут быть в любой RAM — не требуется linker script!
//

/// Физический буфер для операций чтения/записи
static uint8_t s_phys_buffer[kMaxPhysBlockSize] __attribute__((aligned(4)));

/// Кэш последнего прочитанного физического блока (для оптимизации RMW)
static uint8_t s_cache_buffer[kMaxPhysBlockSize] __attribute__((aligned(4)));

/// LBA последнего закэшированного физического блока (UINT32_MAX = невалидный)
static uint32_t s_cached_phys_lba = UINT32_MAX;

/// Флаг: кэш содержит модифицированные данные (dirty)
static bool s_cache_dirty = false;

// ============ Реализация SdmmcBlockDevice ============

SdmmcBlockDevice::~SdmmcBlockDevice() {
    DeInit();
}

bool SdmmcBlockDevice::Init(const SdmmcConfig& config) {
    if (state_ != SdmmcState::NotInitialized) {
        return true;  // Уже инициализирован
    }
    
    config_ = config;
    
    // Сброс кэша
    s_cached_phys_lba = UINT32_MAX;
    s_cache_dirty = false;
    phys_block_size_ = 1024;  // По умолчанию для SD NAND
    
    // 1. Настраиваем источник тактирования SDMMC (PLL1Q)
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_SDMMC;
    PeriphClkInit.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
    
    // 2. Включаем тактирование SDMMC
    if (config_.instance == SDMMC1) {
        __HAL_RCC_SDMMC1_CLK_ENABLE();
        __HAL_RCC_SDMMC1_FORCE_RESET();
        HAL_Delay(10);
        __HAL_RCC_SDMMC1_RELEASE_RESET();
        HAL_Delay(10);
    }
#ifdef SDMMC2
    else if (config_.instance == SDMMC2) {
        __HAL_RCC_SDMMC2_CLK_ENABLE();
        __HAL_RCC_SDMMC2_FORCE_RESET();
        HAL_Delay(10);
        __HAL_RCC_SDMMC2_RELEASE_RESET();
        HAL_Delay(10);
    }
#endif
    
    // 2. Включаем тактирование GPIO
    if (config_.clk_port == GPIOC || config_.data_port == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    }
    if (config_.cmd_port == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    }
    
    // 3. Инициализация GPIO
    InitGpio();
    
    // 4. Конфигурация HAL SD
    hsd_.Instance = config_.instance;
    hsd_.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hsd_.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd_.Init.BusWide = SDMMC_BUS_WIDE_1B;  // Начинаем с 1-bit
    hsd_.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd_.Init.ClockDiv = config_.init_clock_div;
    
    // 5. Инициализация карты
    HAL_StatusTypeDef hal_status = HAL_SD_Init(&hsd_);
    
    if (hal_status != HAL_OK) {
        state_ = SdmmcState::Error;
        return false;
    }
    
    // 6. Получаем информацию о карте
    HAL_SD_CardInfoTypeDef card_info;
    hal_status = HAL_SD_GetCardInfo(&hsd_, &card_info);
    
    if (hal_status != HAL_OK) {
        state_ = SdmmcState::Error;
        return false;
    }
    
    // 7. Определяем физический размер сектора из CSD READ_BL_LEN
    // READ_BL_LEN (4 bits): 83:80 -> CSD[2] bits 16:19
    uint32_t read_bl_len = (hsd_.CSD[2] >> 16) & 0x0F;
    uint32_t phys_block_len = 1 << read_bl_len;
    
    // HAL_SD использует CMD16 (SET_BLOCKLEN) при инициализации
    // Это устанавливает размер блока в 512 байт
    // STL НЕ НУЖЕН для стандартных SD карт
    phys_block_size_ = kLogBlockSize;  // Всегда 512 для стандартных карт
    
    // Если HAL вернул BlockNbr=0, парсим CSD вручную
    if (card_info.BlockNbr == 0) {
        uint32_t csd_struct = (hsd_.CSD[3] >> 30) & 0x03;
        
        if (csd_struct == 0) {
            // CSD v1.0 (Standard Capacity)
            uint32_t c_size = ((hsd_.CSD[1] >> 30) & 0x03) | ((hsd_.CSD[2] & 0x3FF) << 2);
            uint32_t c_size_mult = (hsd_.CSD[1] >> 15) & 0x07;
            uint32_t mult = 1 << (c_size_mult + 2);
            uint32_t block_nr = (c_size + 1) * mult;
            uint64_t capacity = static_cast<uint64_t>(block_nr) * phys_block_len;
            card_info.BlockNbr = capacity / kLogBlockSize;
            card_info.BlockSize = kLogBlockSize;
        } else if (csd_struct == 1) {
            // CSD v2.0 (High Capacity)
            uint32_t c_size = ((hsd_.CSD[2] & 0x3F) << 16) | ((hsd_.CSD[1] >> 16) & 0xFFFF);
            uint64_t capacity = (static_cast<uint64_t>(c_size) + 1) * 512 * 1024;
            card_info.BlockNbr = capacity / kLogBlockSize;
            card_info.BlockSize = kLogBlockSize;
        }
    } else if (card_info.BlockSize > kLogBlockSize) {
        // HAL вернул физический размер блока — пересчитываем в логический
        uint32_t multiplier = card_info.BlockSize / kLogBlockSize;
        card_info.BlockNbr *= multiplier;
        card_info.BlockSize = kLogBlockSize;
    }
    
    // Обновляем HAL структуру для работы с физическими блоками
    hsd_.SdCard.BlockSize = phys_block_size_;
    
    // 8. Переключение на 4-bit режим (если поддерживается)
    if (config_.use_4bit_mode) {
        hal_status = HAL_SD_ConfigWideBusOperation(&hsd_, SDMMC_BUS_WIDE_4B);
        if (hal_status == HAL_OK) {
            // Увеличиваем частоту после успешного переключения
            SDMMC_InitTypeDef init;
            init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
            init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
            init.BusWide = SDMMC_BUS_WIDE_4B;
            init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
            init.ClockDiv = config_.normal_clock_div;
            SDMMC_Init(hsd_.Instance, init);
        }
    }
    
    // 9. Заполняем информацию о карте
    card_info_.block_count = card_info.BlockNbr;
    card_info_.block_size = kLogBlockSize;
    card_info_.capacity_bytes = static_cast<uint64_t>(card_info.BlockNbr) * kLogBlockSize;
    card_info_.card_type = hsd_.SdCard.CardType;
    card_info_.card_version = hsd_.SdCard.CardVersion;
    card_info_.is_ready = true;
    
    if (card_info_.block_count == 0) {
        state_ = SdmmcState::Error;
        return false;
    }
    
    state_ = SdmmcState::Ready;
    return true;
}

void SdmmcBlockDevice::DeInit() {
    if (state_ == SdmmcState::NotInitialized) {
        return;
    }
    
    // Сбрасываем кэш
    FlushCache();
    
    HAL_SD_DeInit(&hsd_);
    DeInitGpio();
    
    if (config_.instance == SDMMC1) {
        __HAL_RCC_SDMMC1_CLK_DISABLE();
    }
#ifdef SDMMC2
    else if (config_.instance == SDMMC2) {
        __HAL_RCC_SDMMC2_CLK_DISABLE();
    }
#endif
    
    state_ = SdmmcState::NotInitialized;
    card_info_ = {};
}

void SdmmcBlockDevice::InitGpio() {
    GPIO_InitTypeDef gpio = {};
    
    // CLK
    gpio.Pin = config_.clk_pin;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = config_.alternate_function;
    HAL_GPIO_Init(config_.clk_port, &gpio);
    
    // CMD
    gpio.Pin = config_.cmd_pin;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(config_.cmd_port, &gpio);
    
    // D0-D3
    gpio.Pin = config_.d0_pin;
    if (config_.use_4bit_mode) {
        gpio.Pin |= config_.d1_pin | config_.d2_pin | config_.d3_pin;
    }
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(config_.data_port, &gpio);
}

void SdmmcBlockDevice::DeInitGpio() {
    HAL_GPIO_DeInit(config_.clk_port, config_.clk_pin);
    HAL_GPIO_DeInit(config_.cmd_port, config_.cmd_pin);
    
    uint16_t data_pins = config_.d0_pin;
    if (config_.use_4bit_mode) {
        data_pins |= config_.d1_pin | config_.d2_pin | config_.d3_pin;
    }
    HAL_GPIO_DeInit(config_.data_port, data_pins);
}

bool SdmmcBlockDevice::IsCardInserted() const {
    if (state_ != SdmmcState::Ready) {
        return false;
    }
    return HAL_SD_GetCardState(const_cast<SD_HandleTypeDef*>(&hsd_)) == HAL_SD_CARD_TRANSFER;
}

bool SdmmcBlockDevice::IsReady() const {
    return state_ == SdmmcState::Ready && card_info_.is_ready;
}

uint32_t SdmmcBlockDevice::GetBlockCount() const {
    return card_info_.block_count;
}

uint32_t SdmmcBlockDevice::GetBlockSize() const {
    return kBlockSize;  // Всегда 512
}

SdmmcDiagnostics SdmmcBlockDevice::GetDiagnostics() const {
    SdmmcDiagnostics diag = {};
    diag.hal_error = HAL_SD_GetError(const_cast<SD_HandleTypeDef*>(&hsd_));
    diag.hal_state = hsd_.State;
    diag.sdmmc_sta = hsd_.Instance->STA;
    diag.sdmmc_resp1 = hsd_.Instance->RESP1;
    return diag;
}

bool SdmmcBlockDevice::Sync() {
    return FlushCache();
}

bool SdmmcBlockDevice::WaitReady(uint32_t timeout_ms) {
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout_ms) {
        if (HAL_SD_GetCardState(&hsd_) == HAL_SD_CARD_TRANSFER) {
            return true;
        }
        HAL_Delay(1);
    }
    return false;
}

bool SdmmcBlockDevice::Read(uint32_t lba, uint8_t* buffer, uint32_t count) {
    if (state_ != SdmmcState::Ready) {
        return false;
    }
    
    if (buffer == nullptr || count == 0) {
        return false;
    }
    
    // Если физический сектор = 512, работаем напрямую
    if (phys_block_size_ == kLogBlockSize) {
        return ReadDirect(lba, buffer, count);
    }
    
    // Sector Translation Layer для SD NAND (phys > 512)
    uint32_t blocks_per_phys = phys_block_size_ / kLogBlockSize;
    
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t log_lba = lba + i;
        uint32_t phys_lba = log_lba / blocks_per_phys;
        uint32_t offset = (log_lba % blocks_per_phys) * kLogBlockSize;
        
        // Проверяем кэш
        if (s_cached_phys_lba != phys_lba) {
            // Сбрасываем dirty кэш если есть
            if (s_cache_dirty) {
                if (!FlushCache()) return false;
            }
            
            // Читаем физический блок в кэш
            if (HAL_SD_ReadBlocks(&hsd_, s_cache_buffer, phys_lba, 1, 
                                  config_.rw_timeout_ms) != HAL_OK) {
                return false;
            }
            
            if (!WaitReady(config_.rw_timeout_ms)) {
                return false;
            }
            
            s_cached_phys_lba = phys_lba;
            s_cache_dirty = false;
        }
        
        // Копируем нужные 512 байт из кэша
        memcpy(buffer + i * kLogBlockSize, s_cache_buffer + offset, kLogBlockSize);
    }
    
    return true;
}

bool SdmmcBlockDevice::ReadDirect(uint32_t lba, uint8_t* buffer, uint32_t count) {
    // Прямое чтение с bounce buffer (D-Cache безопасность)
    for (uint32_t i = 0; i < count; ++i) {
        if (HAL_SD_ReadBlocks(&hsd_, s_phys_buffer, lba + i, 1, 
                              config_.rw_timeout_ms) != HAL_OK) {
            return false;
        }
        
        if (!WaitReady(config_.rw_timeout_ms)) {
            return false;
        }
        
        memcpy(buffer + i * kLogBlockSize, s_phys_buffer, kLogBlockSize);
    }
    
    return true;
}

bool SdmmcBlockDevice::Write(uint32_t lba, const uint8_t* buffer, uint32_t count) {
    if (state_ != SdmmcState::Ready) {
        return false;
    }
    
    if (buffer == nullptr || count == 0) {
        return false;
    }
    
    // Если физический сектор = 512, работаем напрямую
    if (phys_block_size_ == kLogBlockSize) {
        return WriteDirect(lba, buffer, count);
    }
    
    // Sector Translation Layer с Read-Modify-Write
    uint32_t blocks_per_phys = phys_block_size_ / kLogBlockSize;
    
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t log_lba = lba + i;
        uint32_t phys_lba = log_lba / blocks_per_phys;
        uint32_t offset = (log_lba % blocks_per_phys) * kLogBlockSize;
        
        // Загружаем физический блок в кэш если его там нет
        if (s_cached_phys_lba != phys_lba) {
            // Сбрасываем dirty кэш
            if (s_cache_dirty) {
                if (!FlushCache()) return false;
            }
            
            // Читаем физический блок (Read phase of RMW)
            if (HAL_SD_ReadBlocks(&hsd_, s_cache_buffer, phys_lba, 1, 
                                  config_.rw_timeout_ms) != HAL_OK) {
                return false;
            }
            
            if (!WaitReady(config_.rw_timeout_ms)) {
                return false;
            }
            
            s_cached_phys_lba = phys_lba;
        }
        
        // Модифицируем нужные 512 байт в кэше (Modify phase)
        memcpy(s_cache_buffer + offset, buffer + i * kLogBlockSize, kLogBlockSize);
        s_cache_dirty = true;
    }
    
    // Сбрасываем кэш после всех операций (Write phase)
    if (s_cache_dirty) {
        return FlushCache();
    }
    
    return true;
}

bool SdmmcBlockDevice::WriteDirect(uint32_t lba, const uint8_t* buffer, uint32_t count) {
    // Прямая запись с bounce buffer (D-Cache безопасность)
    for (uint32_t i = 0; i < count; ++i) {
        memcpy(s_phys_buffer, buffer + i * kLogBlockSize, kLogBlockSize);
        
        if (HAL_SD_WriteBlocks(&hsd_, s_phys_buffer, lba + i, 1, 
                               config_.rw_timeout_ms) != HAL_OK) {
            return false;
        }
        
        if (!WaitReady(config_.rw_timeout_ms)) {
            return false;
        }
    }
    
    return true;
}

bool SdmmcBlockDevice::FlushCache() {
    if (!s_cache_dirty || s_cached_phys_lba == UINT32_MAX) {
        return true;
    }
    
    // Записываем кэшированный физический блок
    if (HAL_SD_WriteBlocks(&hsd_, s_cache_buffer, s_cached_phys_lba, 1, 
                           config_.rw_timeout_ms) != HAL_OK) {
        return false;
    }
    
    if (!WaitReady(config_.rw_timeout_ms)) {
        return false;
    }
    
    s_cache_dirty = false;
    return true;
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
