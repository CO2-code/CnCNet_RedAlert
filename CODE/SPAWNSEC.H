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
 * SPAWNSEC.H -- Hashing and packet obfuscation primitives used by the CnCNet Spawner.         *
 *                                                                                             *
 * These routines are intentionally free of any game dependencies so that they can be built    *
 * and tested on their own. They reproduce, bit for bit, the helpers used by the original      *
 * Spawner binary patch (SHA-256, the reflected CRC-32 and the repeating XOR key stream).      *
 *                                                                                             *
 * All 32 bit quantities use 'unsigned int' (32 bits on every supported compiler) rather than *
 * 'unsigned long' so that the code behaves identically on 32 and 64 bit hosts.                *
 *---------------------------------------------------------------------------------------------*/

#ifndef SPAWNSEC_H
#define SPAWNSEC_H

#define	SPAWNSEC_SHA256_DIGEST_SIZE	32

/*
**	The net key is the first digest followed by nine chained digests.
*/
#define	SPAWNSEC_NET_KEY_SIZE			(SPAWNSEC_SHA256_DIGEST_SIZE * 10)


/*
**	Incremental SHA-256.
*/
class Sha256Class {
	public:
		Sha256Class(void) {Reset();}

		void Reset(void);
		void Update(void const * data, unsigned long length);
		void Final(unsigned char * digest);

	private:
		void Transform(unsigned char const * block);

		unsigned int State[8];
		unsigned char Buffer[64];
		unsigned int BufferLength;
		unsigned int LengthLow;			// Total message length in bytes (low 32 bits).
		unsigned int LengthHigh;		// Total message length in bytes (high 32 bits).
};


/*
**	Standard reflected CRC-32 (polynomial 0xEDB88320). 'seed' is the running value to continue
**	from, so passing zero produces the usual CRC-32 of the data.
*/
unsigned int Spawner_Crc32(unsigned int seed, void const * data, unsigned long length);

/*
**	Copies 'length' bytes from 'source' to 'dest' XORing them with 'key', which repeats as
**	necessary. 'dest' and 'source' may be the same buffer.
*/
void Spawner_Xor_Key_Stream(void * dest, void const * source, unsigned long length, unsigned char const * key, unsigned long key_length);

/*
**	Expands the first digest into the full net key. Entry zero is 'first_digest'; each following
**	32 byte block is the SHA-256 of the previous block followed by 'salt' (little endian).
**	'key' must have room for SPAWNSEC_NET_KEY_SIZE bytes.
*/
void Spawner_Expand_Net_Key(unsigned char const * first_digest, unsigned int salt, unsigned char * key);

#endif
