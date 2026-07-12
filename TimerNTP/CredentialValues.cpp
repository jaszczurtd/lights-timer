#include "CredentialValues.h"

#include <stddef.h>

const char *credentialValue(Cred credential) {
  static char *values[CR_LAST] = {};

  if (credential < 0 || credential >= CR_LAST) {
    return "";
  }

  if (values[credential] == nullptr) {
    values[credential] = getCredential(credential);
  }
  return values[credential] != nullptr ? values[credential] : "";
}

int credentialIntValue(Cred credential) {
  return getCredentialInt(credential);
}
