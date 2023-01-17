/*
 * devcrypto - Wiimote device-side crypto implementation
 * Copyright (C) 2008-2009 Héctor Martín <hector@marcansoft.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR MIT
 *
 */

#ifndef _DEVCRYPTO_H_
#define _DEVCRYPTO_H_

typedef unsigned char u8;

typedef struct {
	u8 ft[8];
	u8 sb[8];
} wiimote_key;

void wiimote_gen_key(wiimote_key *key, u8 *keydata);
void wiimote_encrypt(wiimote_key *key, int len, u8 *data, int addr);
void wiimote_decrypt(wiimote_key *key, int len, u8 *data, int addr);

#endif /* _DEVCRYPTO_H_ */

