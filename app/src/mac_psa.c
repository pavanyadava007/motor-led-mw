#include "mac_psa.h"

#include <psa/crypto.h>
#include <zephyr/sys/printk.h>

/* Persistent key id in TF-M Internal Trusted Storage. The non-secure side only
 * ever holds this id. DEMO ONLY: on first boot the key is imported from the
 * constant below; a product would provision it at end of line instead. */
#define CMD_KEY_ID ((psa_key_id_t)0x00001001)
#define CMD_ALG PSA_ALG_TRUNCATED_MAC(PSA_ALG_CMAC, 8)

static const uint8_t demo_key[16] = "mlmw-demo-key-01";

int mac_psa_init(void)
{
	psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
	psa_status_t st = psa_crypto_init();

	if (st != PSA_SUCCESS) {
		return (int)st;
	}
	st = psa_get_key_attributes(CMD_KEY_ID, &attr);
	psa_reset_key_attributes(&attr);
	if (st == PSA_SUCCESS) {
		printk("mac: key 0x%x already in secure storage\n", (unsigned)CMD_KEY_ID);
		return 0;
	}
	psa_key_id_t id;

	psa_set_key_id(&attr, CMD_KEY_ID);
	psa_set_key_lifetime(&attr, PSA_KEY_LIFETIME_PERSISTENT);
	psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_VERIFY_MESSAGE);
	psa_set_key_algorithm(&attr, CMD_ALG);
	psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&attr, 128);
	st = psa_import_key(&attr, demo_key, sizeof(demo_key), &id);
	printk("mac: imported key 0x%x into secure storage, status %d\n", (unsigned)id, (int)st);
	return st == PSA_SUCCESS ? 0 : (int)st;
}

int mac_psa_verify(void *user, const uint8_t *msg, size_t len, const uint8_t tag[8])
{
	(void)user;
	return psa_mac_verify(CMD_KEY_ID, CMD_ALG, msg, len, tag, 8) == PSA_SUCCESS ? 0 : -1;
}
