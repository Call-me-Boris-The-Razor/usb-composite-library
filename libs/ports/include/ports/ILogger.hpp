/**
 * @file ILogger.hpp
 * @brief Интерфейс логирования
 * 
 * Абстракция для вывода диагностических сообщений.
 * Не содержит HAL/платформенных зависимостей.
 */

#pragma once

#include <cstdint>

namespace usb::ports {

/// Уровни логирования
enum class LogLevel : uint8_t {
    Error = 0,
    Warning = 1,
    Info = 2,
    Debug = 3,
    Trace = 4,
};

/**
 * @brief Интерфейс логгера
 */
struct ILogger {
    virtual ~ILogger() = default;
    
    /// Вывод сообщения
    virtual void Log(LogLevel level, const char* message) = 0;
    
    // Удобные методы
    void Error(const char* msg) { Log(LogLevel::Error, msg); }
    void Warning(const char* msg) { Log(LogLevel::Warning, msg); }
    void Info(const char* msg) { Log(LogLevel::Info, msg); }
    void Debug(const char* msg) { Log(LogLevel::Debug, msg); }
    
    // Запрет копирования
    ILogger(const ILogger&) = delete;
    ILogger& operator=(const ILogger&) = delete;
    
protected:
    ILogger() = default;
};

/// Null logger (ничего не делает)
struct NullLogger : ILogger {
    void Log(LogLevel, const char*) override {}
};

}  // namespace usb::ports
