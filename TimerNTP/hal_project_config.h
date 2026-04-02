#pragma once

/**
 * @file hal_project_config.h
 * @brief JaszczurHAL module configuration for the TimerNTP project.
 *
 * This file is automatically picked up by hal_config.h via __has_include.
 * Define HAL_DISABLE_* flags here to exclude unused HAL modules from the
 * build.  Dependency propagation (e.g. EEPROM → KV) is handled by
 * hal_config.h — you only need to disable the base module.
 */

/* ── Modules not used by TimerNTP ──────────────────────────────────────── */

#define HAL_DISABLE_GPS             /* GPS / NMEA receiver                */
#define HAL_DISABLE_THERMOCOUPLE    /* MCP9600 / MAX6675                  */
#define HAL_DISABLE_UART            /* Hardware UART (SerialUART)         */
#define HAL_DISABLE_SWSERIAL        /* SoftwareSerial                     */
#define HAL_DISABLE_TFT             /* no TFT driver                  */
#define HAL_DISABLE_RGB_LED         /* no RGB LED strip                    */
#define HAL_DISABLE_EXTERNAL_ADC     /* no external ADC (e.g. ADS1115)      */
#define HAL_DISABLE_CAN             /* no CAN bus                          */
#define HAL_ENABLE_CJSON            /* enable cJSON from JaszczurHAL/utils */
#define HAL_DISABLE_UNITY           /* no Unity test framework              */
