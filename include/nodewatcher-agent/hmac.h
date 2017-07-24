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
#ifndef NODEWATCHER_AGENT_HMAC_H
#define NODEWATCHER_AGENT_HMAC_H

#include <nodewatcher-agent/sha256.h>

#define SHA256_HASH_SIZE 32
#define SHA256_BLOCK_SIZE 64

void sha256(const BYTE *text, BYTE *buf);
void hmac_sha256(BYTE *key, int key_len, const BYTE *data, int data_len, BYTE *hmac_out);

#endif   // NODEWATCHER_AGENT_HMAC_H
