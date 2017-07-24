#include <nodewatcher-agent/hmac.h>

void sha256(const BYTE *text, BYTE *buf)
{
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, text, strlen((char*) text));
    sha256_final(&ctx, buf);
}

void hmac_sha256(BYTE *key, int key_len, const BYTE *data, int data_len, BYTE *hmac_out)
{
    BYTE key_buf[SHA256_BLOCK_SIZE] = {0};

    if (key_len > SHA256_BLOCK_SIZE) {
        sha256(key, key_buf); 
    }
    if (key_len < SHA256_BLOCK_SIZE) {
       memcpy(key_buf, key, key_len);
    }

    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
        key_buf[i] ^= 0x36;
    }

    size_t buf_size = SHA256_BLOCK_SIZE + data_len;
    BYTE *in_buf = (BYTE*) malloc(buf_size);
    memset(in_buf, 0x00, buf_size);
    memcpy(in_buf, key_buf, SHA256_BLOCK_SIZE);
    memcpy(in_buf + SHA256_BLOCK_SIZE, data, data_len);
    memset(hmac_out, 0x00, SHA256_HASH_SIZE);
    sha256(in_buf, hmac_out);

    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
        key_buf[i] ^= 0x36 ^ 0x5c;
    }

    buf_size = SHA256_BLOCK_SIZE + SHA256_HASH_SIZE + 1;
    in_buf = (BYTE*) realloc(in_buf, buf_size);
    memset(in_buf, 0x00, buf_size);
    memcpy(in_buf, key_buf, SHA256_BLOCK_SIZE);
    memcpy(in_buf + SHA256_BLOCK_SIZE, hmac_out, SHA256_HASH_SIZE);
    memset(hmac_out, 0x00, SHA256_HASH_SIZE);
    sha256(in_buf, hmac_out);

    free(in_buf);
}
