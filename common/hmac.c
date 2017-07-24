/*
 * nodewatcher-agent - remote monitoring daemon
 *
 * Copyright (C) 2017 Anej Placer <anej.placer@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
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
