#include "OTAUpdates.h"

#include <Credentials.h>
#include "CredentialValues.h"

#include <hal/hal.h>
#include <tools.h>

#include <stdio.h>

namespace {
constexpr uint32_t OTA_INIT_RETRY_MS = 5000;
constexpr const char *OTA_FALLBACK_HOSTNAME = "timerntp";

void ota_on_start(hal_ota_command_t command, void *user) {
  (void)user;
  const char *type = "sketch";
  if (command == HAL_OTA_COMMAND_FILESYSTEM) {
    type = "filesystem";
  }
  deb("OTA start: %s", type);
}

void ota_on_end(void *user) {
  (void)user;
  deb("OTA end: success");
}

void ota_on_progress(uint32_t progress, uint32_t total, void *user) {
  (void)user;
  if (total == 0u) {
    return;
  }
  deb("OTA progress: %u%%", (unsigned)((progress * 100u) / total));
}

void ota_on_error(hal_ota_error_t error, void *user) {
  (void)user;
  switch (error) {
    case HAL_OTA_ERROR_AUTH:
      derr("OTA error: auth");
      break;
    case HAL_OTA_ERROR_BEGIN:
      derr("OTA error: begin");
      break;
    case HAL_OTA_ERROR_CONNECT:
      derr("OTA error: connect");
      break;
    case HAL_OTA_ERROR_RECEIVE:
      derr("OTA error: receive");
      break;
    case HAL_OTA_ERROR_END:
      derr("OTA error: end");
      break;
    default:
      derr("OTA error: unknown=%u", (unsigned)error);
      break;
  }
}
}

void OTAUpdates::configureIfNeeded(const char *hostname) {
  if (active) {
    return;
  }

  const uint32_t now = hal_millis();
  if ((int32_t)(now - retryAtMs) < 0) {
    return;
  }

  const auto scheduleRetry = [this, now]() {
    active = false;
    retryAtMs = now + OTA_INIT_RETRY_MS;
  };

  if (!hal_littlefs_begin()) {
    if (!littlefsRecoveryAttempted) {
      littlefsRecoveryAttempted = true;
      deb("LittleFS mount failed; formatting filesystem");
      if (hal_littlefs_format() && hal_littlefs_begin()) {
        deb("LittleFS recovery succeeded");
      } else {
        derr("LittleFS recovery failed. OTA retry in %lu ms",
             (unsigned long)OTA_INIT_RETRY_MS);
        scheduleRetry();
        return;
      }
    } else {
      derr("LittleFS mount failed. OTA retry in %lu ms",
           (unsigned long)OTA_INIT_RETRY_MS);
      scheduleRetry();
      return;
    }
  }

  if (!hal_ota_on_start(ota_on_start, this) ||
      !hal_ota_on_end(ota_on_end, this) ||
      !hal_ota_on_progress(ota_on_progress, this) ||
      !hal_ota_on_error(ota_on_error, this)) {
    derr("OTA callback setup failed. OTA retry in %lu ms", (unsigned long)OTA_INIT_RETRY_MS);
    scheduleRetry();
    return;
  }

  const int otaPort = credentialIntValue(CR_OTA_UPDATE_PORT);
  if (!hal_ota_set_port(otaPort)) {
    derr("OTA set port failed. OTA retry in %lu ms", (unsigned long)OTA_INIT_RETRY_MS);
    scheduleRetry();
    return;
  }

  char hostname_ascii[64] = {0};
  const char *source_hostname = hostname;
  if (!source_hostname || source_hostname[0] == '\0') {
    source_hostname = OTA_FALLBACK_HOSTNAME;
  }
  remove_non_ascii(source_hostname, hostname_ascii, sizeof(hostname_ascii));
  if (hostname_ascii[0] == '\0') {
    snprintf(hostname_ascii, sizeof(hostname_ascii), "%s", OTA_FALLBACK_HOSTNAME);
  }

  if (!hal_ota_set_hostname(hostname_ascii)) {
    derr("OTA set hostname failed. OTA retry in %lu ms", (unsigned long)OTA_INIT_RETRY_MS);
    scheduleRetry();
    return;
  }

  if (!hal_ota_set_password(credentialValue(CR_OTA_UPDATE_PASSWORD))) {
    derr("OTA set password failed. OTA retry in %lu ms", (unsigned long)OTA_INIT_RETRY_MS);
    scheduleRetry();
    return;
  }

  if (!hal_ota_begin()) {
    derr("OTA begin failed. OTA retry in %lu ms", (unsigned long)OTA_INIT_RETRY_MS);
    scheduleRetry();
    return;
  }

  active = true;
  retryAtMs = 0;
  deb("OTA ready: host=%s port=%d", hostname_ascii, otaPort);
}

void OTAUpdates::handle(bool wifiConnected, const char *hostname) {
  if (!wifiConnected) {
    active = false;
    return;
  }

  if (!active) {
    configureIfNeeded(hostname);
    return;
  }

  hal_ota_handle();
}
