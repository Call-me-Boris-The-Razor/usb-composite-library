/**
 * @file tusb_config.h
 * @brief TinyUSB configuration — прокси для usb_composite_config.h
 * 
 * Этот файл позволяет TinyUSB найти конфигурацию автоматически.
 * Просто добавьте путь к include/ библиотеки в build_flags:
 *   -I lib/usb_composite/include
 */

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#include "usb_composite_config.h"

#endif // TUSB_CONFIG_H_
