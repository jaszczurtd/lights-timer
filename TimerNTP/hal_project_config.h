#pragma once

/**
 * @file hal_project_config.h
 * @brief JaszczurHAL module configuration for TimerNTP (OPT-IN policy).
 *
 * New policy: enable only modules that are used by the project.
 * Optional modules are OFF by default, so this file should contain only
 * HAL_ENABLE_* flags (plus optional HAL_DISABLE_ASSERTS).
 *
 * Dependency propagation is automatic in hal_config.h, e.g.:
 * - HAL_ENABLE_TIME      -> HAL_ENABLE_WIFI
 * - HAL_ENABLE_KV        -> HAL_ENABLE_EEPROM
 * - HAL_ENABLE_DS18B20   -> HAL_ENABLE_ONEWIRE
 * - HAL_ENABLE_SSD1306   -> HAL_ENABLE_DISPLAY
 */

/* ── Connectivity ──────────────────────────────────────────────────────── */
#define HAL_ENABLE_TIME             /* NTP/system time -> WiFi             */
#define HAL_ENABLE_MQTT             /* MQTT (PubSubClient) -> WiFi         */
#define HAL_ENABLE_UDP              /* UDP wrapper (WiFiUDP) -> WiFi       */
#define HAL_ENABLE_OTA              /* ArduinoOTA wrapper -> WiFi          */
#define HAL_ENABLE_WIREGUARD        /* WireGuard wrapper -> WiFi           */

/* ── Storage ───────────────────────────────────────────────────────────── */
#define HAL_ENABLE_KV               /* KV store -> EEPROM                  */
#define HAL_ENABLE_LITTLEFS         /* LittleFS helpers                    */

/* ── Buses ─────────────────────────────────────────────────────────────── */
#define HAL_ENABLE_I2C              /* required by hal_i2c_init() usage    */

/* ── Sensors ───────────────────────────────────────────────────────────── */
#define HAL_ENABLE_DS18B20          /* DS18B20 support -> ONEWIRE          */

/* ── Display ───────────────────────────────────────────────────────────── */
#define HAL_ENABLE_SSD1306          /* SSD1306 backend -> DISPLAY          */

/* ── Bundled libs ──────────────────────────────────────────────────────── */
#define HAL_ENABLE_CJSON            /* bundled cJSON / cJSON_Utils         */

/* ── Optional debug/config flags ───────────────────────────────────────── */
// #define HAL_DISABLE_ASSERTS       /* opt-out asserts (release only)      */
// #define HAL_CONFIG_VERBOSE        /* print active HAL_ENABLE_* flags     */
