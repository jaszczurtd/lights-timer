
#include "ca_cert.h"

static const char* ca_cert =
  "-----BEGIN CERTIFICATE-----\n"
  "REPLACE_WITH_CA_CERTIFICATE_PEM_LINE_1\n"
  "REPLACE_WITH_CA_CERTIFICATE_PEM_LINE_2\n"
  "REPLACE_WITH_CA_CERTIFICATE_PEM_LINE_3\n"
  "...\n"
  "REPLACE_WITH_CA_CERTIFICATE_PEM_LAST_LINE\n"
  "-----END CERTIFICATE-----\n";

const char *getCertificate() {
  return ca_cert;
}
