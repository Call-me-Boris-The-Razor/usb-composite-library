/**
 * @file MockBlockDevice.hpp
 * @brief Mock реализация IBlockDevice для тестов
 */

#pragma once

#include "ports/IBlockDevice.hpp"
#include <cstring>
#include <vector>

namespace usb::mock {

/**
 * @brief Mock блочное устройство для unit тестов
 */
class MockBlockDevice : public ports::IBlockDevice {
public:
    static constexpr uint32_t kDefaultBlockSize = 512;
    static constexpr uint32_t kDefaultBlockCount = 1024;  // 512KB
    
    explicit MockBlockDevice(uint32_t block_count = kDefaultBlockCount,
                             uint32_t block_size = kDefaultBlockSize)
        : block_count_(block_count)
        , block_size_(block_size)
        , data_(block_count * block_size, 0)
        , ready_(true) {}
    
    // IBlockDevice interface
    [[nodiscard]] bool IsReady() const override { return ready_; }
    [[nodiscard]] uint32_t GetBlockCount() const override { return block_count_; }
    [[nodiscard]] uint32_t GetBlockSize() const override { return block_size_; }
    
    bool Read(uint32_t lba, uint8_t* buffer, uint32_t count) override {
        if (!ready_ || lba + count > block_count_) {
            return false;
        }
        
        read_count_++;
        last_read_lba_ = lba;
        last_read_count_ = count;
        
        std::memcpy(buffer, &data_[lba * block_size_], count * block_size_);
        return true;
    }
    
    bool Write(uint32_t lba, const uint8_t* buffer, uint32_t count) override {
        if (!ready_ || lba + count > block_count_) {
            return false;
        }
        
        write_count_++;
        last_write_lba_ = lba;
        last_write_count_ = count;
        
        std::memcpy(&data_[lba * block_size_], buffer, count * block_size_);
        return true;
    }
    
    bool Sync() override {
        sync_count_++;
        return true;
    }
    
    // Test helpers
    void SetReady(bool ready) { ready_ = ready; }
    void Fill(uint8_t value) { std::fill(data_.begin(), data_.end(), value); }
    uint8_t* GetData() { return data_.data(); }
    
    // Счётчики для проверок в тестах
    uint32_t GetReadCount() const { return read_count_; }
    uint32_t GetWriteCount() const { return write_count_; }
    uint32_t GetSyncCount() const { return sync_count_; }
    uint32_t GetLastReadLba() const { return last_read_lba_; }
    uint32_t GetLastWriteLba() const { return last_write_lba_; }
    
    void ResetCounters() {
        read_count_ = write_count_ = sync_count_ = 0;
        last_read_lba_ = last_write_lba_ = 0;
        last_read_count_ = last_write_count_ = 0;
    }
    
private:
    uint32_t block_count_;
    uint32_t block_size_;
    std::vector<uint8_t> data_;
    bool ready_;
    
    // Счётчики
    uint32_t read_count_ = 0;
    uint32_t write_count_ = 0;
    uint32_t sync_count_ = 0;
    uint32_t last_read_lba_ = 0;
    uint32_t last_write_lba_ = 0;
    uint32_t last_read_count_ = 0;
    uint32_t last_write_count_ = 0;
};

}  // namespace usb::mock
