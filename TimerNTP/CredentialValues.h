#pragma once

#include <Credentials.h>

// Credentials strings are decoded once and retained for the firmware lifetime.
// This gives HAL APIs stable pointers without repeatedly allocating buffers.
const char *credentialValue(Cred credential);
int credentialIntValue(Cred credential);
