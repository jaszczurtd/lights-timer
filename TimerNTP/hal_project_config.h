#pragma once

/**
 * @file hal_project_config.h
 * @brief JaszczurHAL module configuration for TimerNTP.
 *
 * Unused HAL modules are disabled here to reduce build time and binary size.
 * Dependency propagation handled by hal_config.h:
 *   HAL_DISABLE_SWSERIAL  -> also disables GPS
 *
 * Modules USED by this project:
 *   hal_wifi, hal_time, hal_eeprom, hal_kv, hal_i2c, hal_display,
 *   hal_gpio, hal_system
 */

#define HAL_DISABLE_UART
#define HAL_DISABLE_SWSERIAL
#define HAL_DISABLE_EXTERNAL_ADC
#define HAL_DISABLE_PWM_FREQ
#define HAL_DISABLE_RGB_LED
#define HAL_DISABLE_CAN
#define HAL_DISABLE_THERMOCOUPLE
