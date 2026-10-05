#ifndef MAC_PSA_H
#define MAC_PSA_H

#include <stddef.h>
#include <stdint.h>

/* Opens (first boot: imports) the command key in TF-M and returns 0 on success. */
int mac_psa_init(void);
/* mlmw_mac_verify_fn: AES-128 CMAC truncated to 8 bytes, verified in the secure world. */
int mac_psa_verify(void *user, const uint8_t *msg, size_t len, const uint8_t tag[8]);

#endif
