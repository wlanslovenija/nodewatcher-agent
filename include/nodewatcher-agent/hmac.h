#ifndef NODEWATCHER_AGENT_HMAC_H
#define NODEWATCHER_AGENT_HMAC_H

#include <nodewatcher-agent/sha256.h>

#define SHA256_HASH_SIZE 32
#define SHA256_BLOCK_SIZE 64

void sha256(const BYTE *text, BYTE *buf);
void hmac(BYTE *key, int key_len, const BYTE *data, int data_len, BYTE *hmac_out);

#endif   // NODEWATCHER_AGENT_HMAC_H