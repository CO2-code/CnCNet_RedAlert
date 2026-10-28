/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/***********************************************************************************************
 * SPAWNSEC.CPP -- Hashing and packet obfuscation primitives used by the CnCNet Spawner.       *
 *                                                                                             *
 * This file deliberately does not include the game headers. See SPAWNSEC.H.                   *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   Sha256Class::Reset -- Restarts the hash.                                                  *
 *   Sha256Class::Update -- Adds data to the hash.                                             *
 *   Sha256Class::Final -- Finishes the hash and returns the digest.                           *
 *   Spawner_Crc32 -- Reflected CRC-32 with a caller supplied starting value.                  *
 *   Spawner_Xor_Key_Stream -- Repeating-key XOR.                                              *
 *   Spawner_Expand_Net_Key -- Builds the chained net key from the first digest.               *
 *=============================================================================================*/

#include	<string.h>
#include	"spawnsec.h"


/*
**	SHA-256 round constants (FIPS 180-4).
*/
static unsigned int const Sha256K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define	ROTR32(x, n)	(((x) >> (n)) | ((x) << (32 - (n))))


/***********************************************************************************************
 * Sha256Class::Reset -- Restarts the hash.                                                    *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
void Sha256Class::Reset(void)
{
	State[0] = 0x6a09e667;
	State[1] = 0xbb67ae85;
	State[2] = 0x3c6ef372;
	State[3] = 0xa54ff53a;
	State[4] = 0x510e527f;
	State[5] = 0x9b05688c;
	State[6] = 0x1f83d9ab;
	State[7] = 0x5be0cd19;
	BufferLength = 0;
	LengthLow = 0;
	LengthHigh = 0;
}


/***********************************************************************************************
 * Sha256Class::Transform -- Processes one 64 byte block.                                      *
 *                                                                                             *
 * INPUT:   block -- Pointer to 64 bytes of message data.                                      *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
void Sha256Class::Transform(unsigned char const * block)
{
	unsigned int w[64];
	unsigned int a, b, c, d, e, f, g, h;
	int i;

	for (i = 0; i < 16; i++) {
		w[i] = ((unsigned int)block[i * 4] << 24) |
				 ((unsigned int)block[i * 4 + 1] << 16) |
				 ((unsigned int)block[i * 4 + 2] << 8) |
				 ((unsigned int)block[i * 4 + 3]);
	}
	for (i = 16; i < 64; i++) {
		unsigned int s0 = ROTR32(w[i - 15], 7) ^ ROTR32(w[i - 15], 18) ^ (w[i - 15] >> 3);
		unsigned int s1 = ROTR32(w[i - 2], 17) ^ ROTR32(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = (w[i - 16] + s0 + w[i - 7] + s1) & 0xFFFFFFFF;
	}

	a = State[0];
	b = State[1];
	c = State[2];
	d = State[3];
	e = State[4];
	f = State[5];
	g = State[6];
	h = State[7];

	for (i = 0; i < 64; i++) {
		unsigned int s1 = ROTR32(e, 6) ^ ROTR32(e, 11) ^ ROTR32(e, 25);
		unsigned int ch = (e & f) ^ ((~e) & g);
		unsigned int t1 = (h + s1 + ch + Sha256K[i] + w[i]) & 0xFFFFFFFF;
		unsigned int s0 = ROTR32(a, 2) ^ ROTR32(a, 13) ^ ROTR32(a, 22);
		unsigned int maj = (a & b) ^ (a & c) ^ (b & c);
		unsigned int t2 = (s0 + maj) & 0xFFFFFFFF;

		h = g;
		g = f;
		f = e;
		e = (d + t1) & 0xFFFFFFFF;
		d = c;
		c = b;
		b = a;
		a = (t1 + t2) & 0xFFFFFFFF;
	}

	State[0] = (State[0] + a) & 0xFFFFFFFF;
	State[1] = (State[1] + b) & 0xFFFFFFFF;
	State[2] = (State[2] + c) & 0xFFFFFFFF;
	State[3] = (State[3] + d) & 0xFFFFFFFF;
	State[4] = (State[4] + e) & 0xFFFFFFFF;
	State[5] = (State[5] + f) & 0xFFFFFFFF;
	State[6] = (State[6] + g) & 0xFFFFFFFF;
	State[7] = (State[7] + h) & 0xFFFFFFFF;
}


/***********************************************************************************************
 * Sha256Class::Update -- Adds data to the hash.                                               *
 *                                                                                             *
 * INPUT:   data   -- Pointer to the data.                                                     *
 *          length -- Number of bytes.                                                         *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
void Sha256Class::Update(void const * data, unsigned long length)
{
	unsigned char const * bytes = (unsigned char const *)data;

	/*
	**	Track the total length as a 64 bit byte count split over two 32 bit words.
	*/
	unsigned int old_low = LengthLow;
	LengthLow = (LengthLow + (unsigned int)(length & 0xFFFFFFFF)) & 0xFFFFFFFF;
	if (LengthLow < old_low) {
		LengthHigh++;
	}

	while (length > 0) {
		unsigned long space = 64 - BufferLength;
		unsigned long chunk = (length < space) ? length : space;

		memcpy(&Buffer[BufferLength], bytes, (size_t)chunk);
		BufferLength += (unsigned int)chunk;
		bytes += chunk;
		length -= chunk;

		if (BufferLength == 64) {
			Transform(Buffer);
			BufferLength = 0;
		}
	}
}


/***********************************************************************************************
 * Sha256Class::Final -- Finishes the hash and returns the digest.                             *
 *                                                                                             *
 * INPUT:   digest -- Buffer that receives the 32 byte digest.                                 *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   The object must be Reset() before it is used again.                             *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
void Sha256Class::Final(unsigned char * digest)
{
	/*
	**	Message length in bits, as a 64 bit big endian value.
	*/
	unsigned int bits_high = ((LengthHigh << 3) | (LengthLow >> 29)) & 0xFFFFFFFF;
	unsigned int bits_low = (LengthLow << 3) & 0xFFFFFFFF;
	unsigned char length_bytes[8];
	unsigned char pad = 0x80;
	unsigned char zero = 0;
	int i;

	length_bytes[0] = (unsigned char)(bits_high >> 24);
	length_bytes[1] = (unsigned char)(bits_high >> 16);
	length_bytes[2] = (unsigned char)(bits_high >> 8);
	length_bytes[3] = (unsigned char)(bits_high);
	length_bytes[4] = (unsigned char)(bits_low >> 24);
	length_bytes[5] = (unsigned char)(bits_low >> 16);
	length_bytes[6] = (unsigned char)(bits_low >> 8);
	length_bytes[7] = (unsigned char)(bits_low);

	Update(&pad, 1);
	while (BufferLength != 56) {
		Update(&zero, 1);
	}
	Update(length_bytes, 8);

	for (i = 0; i < 8; i++) {
		digest[i * 4] = (unsigned char)(State[i] >> 24);
		digest[i * 4 + 1] = (unsigned char)(State[i] >> 16);
		digest[i * 4 + 2] = (unsigned char)(State[i] >> 8);
		digest[i * 4 + 3] = (unsigned char)(State[i]);
	}
}


/***********************************************************************************************
 * Spawner_Crc32 -- Reflected CRC-32 with a caller supplied starting value.                    *
 *                                                                                             *
 * INPUT:   seed   -- The running CRC to continue from (zero for a fresh CRC).                 *
 *          data   -- Pointer to the data.                                                     *
 *          length -- Number of bytes.                                                         *
 *                                                                                             *
 * OUTPUT:  The CRC of the data.                                                               *
 *                                                                                             *
 * WARNINGS:   This is the same function as the public domain Crc32_ComputeBuf used by the     *
 *             original Spawner patch, including treating 'seed' as a finished CRC value.      *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
unsigned int Spawner_Crc32(unsigned int seed, void const * data, unsigned long length)
{
	static unsigned int table[256];
	static bool table_ready = false;

	if (!table_ready) {
		for (unsigned int n = 0; n < 256; n++) {
			unsigned int c = n;
			for (int k = 0; k < 8; k++) {
				if (c & 1) {
					c = 0xEDB88320 ^ (c >> 1);
				} else {
					c = c >> 1;
				}
			}
			table[n] = c;
		}
		table_ready = true;
	}

	unsigned char const * bytes = (unsigned char const *)data;
	unsigned int crc = seed ^ 0xFFFFFFFF;
	for (unsigned long i = 0; i < length; i++) {
		crc = (crc >> 8) ^ table[(crc ^ bytes[i]) & 0xFF];
	}
	return(crc ^ 0xFFFFFFFF);
}


/***********************************************************************************************
 * Spawner_Xor_Key_Stream -- Repeating-key XOR.                                                *
 *                                                                                             *
 * INPUT:   dest       -- Output buffer.                                                       *
 *          source     -- Input buffer (may equal dest).                                       *
 *          length     -- Number of bytes to process.                                          *
 *          key        -- The key bytes.                                                       *
 *          key_length -- Number of key bytes (must be non-zero).                              *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
void Spawner_Xor_Key_Stream(void * dest, void const * source, unsigned long length, unsigned char const * key, unsigned long key_length)
{
	unsigned char * d = (unsigned char *)dest;
	unsigned char const * s = (unsigned char const *)source;

	if (key_length == 0) {
		if (d != s) memcpy(d, s, (size_t)length);
		return;
	}

	for (unsigned long i = 0; i < length; i++) {
		d[i] = (unsigned char)(s[i] ^ key[i % key_length]);
	}
}


/***********************************************************************************************
 * Spawner_Expand_Net_Key -- Builds the chained net key from the first digest.                 *
 *                                                                                             *
 * INPUT:   first_digest -- The 32 byte digest of the match specific data.                     *
 *          salt         -- Value mixed into every chained block.                              *
 *          key          -- Receives SPAWNSEC_NET_KEY_SIZE bytes.                              *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
void Spawner_Expand_Net_Key(unsigned char const * first_digest, unsigned int salt, unsigned char * key)
{
	unsigned char salt_bytes[4];
	salt_bytes[0] = (unsigned char)(salt);
	salt_bytes[1] = (unsigned char)(salt >> 8);
	salt_bytes[2] = (unsigned char)(salt >> 16);
	salt_bytes[3] = (unsigned char)(salt >> 24);

	memcpy(key, first_digest, SPAWNSEC_SHA256_DIGEST_SIZE);

	for (int i = 0; i < 9; i++) {
		Sha256Class sha;
		sha.Update(key + (i * SPAWNSEC_SHA256_DIGEST_SIZE), SPAWNSEC_SHA256_DIGEST_SIZE);
		sha.Update(salt_bytes, 4);
		sha.Final(key + ((i + 1) * SPAWNSEC_SHA256_DIGEST_SIZE));
	}
}
