
#include "ca_cert.h"

static const char* ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----

[your certificate here]

----END CERTIFICATE-----
)EOF";

const char *getCertificate() {
  return ca_cert;
}
