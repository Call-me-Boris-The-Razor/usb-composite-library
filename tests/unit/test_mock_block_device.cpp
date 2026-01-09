/**
 * @file test_mock_block_device.cpp
 * @brief Unit тесты для MockBlockDevice
 */

#include <unity.h>
#include "mock/MockBlockDevice.hpp"

using usb::mock::MockBlockDevice;

void setUp() {
    // Вызывается перед каждым тестом
}

void tearDown() {
    // Вызывается после каждого теста
}

void test_mock_device_is_ready_by_default() {
    MockBlockDevice device;
    TEST_ASSERT_TRUE(device.IsReady());
}

void test_mock_device_returns_correct_block_count() {
    MockBlockDevice device(2048, 512);
    TEST_ASSERT_EQUAL_UINT32(2048, device.GetBlockCount());
}

void test_mock_device_returns_correct_block_size() {
    MockBlockDevice device(1024, 512);
    TEST_ASSERT_EQUAL_UINT32(512, device.GetBlockSize());
}

void test_mock_device_read_writes_correctly() {
    MockBlockDevice device(16, 512);
    
    // Записываем данные
    uint8_t write_buf[512];
    for (int i = 0; i < 512; i++) {
        write_buf[i] = static_cast<uint8_t>(i & 0xFF);
    }
    
    TEST_ASSERT_TRUE(device.Write(0, write_buf, 1));
    TEST_ASSERT_EQUAL_UINT32(1, device.GetWriteCount());
    TEST_ASSERT_EQUAL_UINT32(0, device.GetLastWriteLba());
    
    // Читаем данные
    uint8_t read_buf[512] = {0};
    TEST_ASSERT_TRUE(device.Read(0, read_buf, 1));
    TEST_ASSERT_EQUAL_UINT32(1, device.GetReadCount());
    
    // Проверяем что данные совпадают
    TEST_ASSERT_EQUAL_UINT8_ARRAY(write_buf, read_buf, 512);
}

void test_mock_device_fails_on_out_of_bounds_read() {
    MockBlockDevice device(16, 512);
    uint8_t buf[512];
    
    // Чтение за пределами должно провалиться
    TEST_ASSERT_FALSE(device.Read(20, buf, 1));
}

void test_mock_device_fails_when_not_ready() {
    MockBlockDevice device;
    device.SetReady(false);
    
    uint8_t buf[512];
    TEST_ASSERT_FALSE(device.Read(0, buf, 1));
    TEST_ASSERT_FALSE(device.Write(0, buf, 1));
}

void test_mock_device_sync_increments_counter() {
    MockBlockDevice device;
    
    TEST_ASSERT_EQUAL_UINT32(0, device.GetSyncCount());
    TEST_ASSERT_TRUE(device.Sync());
    TEST_ASSERT_EQUAL_UINT32(1, device.GetSyncCount());
}

int main() {
    UNITY_BEGIN();
    
    RUN_TEST(test_mock_device_is_ready_by_default);
    RUN_TEST(test_mock_device_returns_correct_block_count);
    RUN_TEST(test_mock_device_returns_correct_block_size);
    RUN_TEST(test_mock_device_read_writes_correctly);
    RUN_TEST(test_mock_device_fails_on_out_of_bounds_read);
    RUN_TEST(test_mock_device_fails_when_not_ready);
    RUN_TEST(test_mock_device_sync_increments_counter);
    
    return UNITY_END();
}
