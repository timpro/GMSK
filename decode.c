
#include <stdio.h>
#include <stdint.h>

#define PUNCTURE

int main()
{
	uint32_t i, j, lsb, lfsr, outbyte;
	char *instring = "$$UBSEDS22G,00:00:00,0.0000,0.0000 $UBSEDS22G,00:00:00,0.0000,0.0000 $UBSEDS22G,00:00:00,0.0000,0.0000";
	uint8_t  scrambled[260];
	uint8_t  conflicts[260];
	uint16_t interleaved[260];

	printf("%s\n", instring);

	// apply scrambler
	lfsr = 0;
	for (i = 0; instring[i] > 0; i++) {
		outbyte = 0;
		for (j = 0; j < 8; j++ ) {
			lsb = (instring[i] >> j);
			lsb ^= (lfsr >> 12);
			lsb ^= (lfsr >> 17);
			lsb &= 0b1;
			lfsr <<= 1;
			lfsr |= lsb;
			outbyte |= lsb << j;
		}
		scrambled[i] = outbyte;
	}

	// replace 1x bytes of data with 2x bytes of FEC and interleave
	lfsr = 0;
	for (i = 0; i < 256; i++) {
		uint16_t g1, g2, w[4], outword;
		outword = 0;
		for (j = 0; j < 8; j++ ) {
			lfsr <<= 1;
			lfsr |= (scrambled[i] >> j) & 0b1;
			g1 = 0b1 & ( lfsr ^ (lfsr >> 3) ^ (lfsr >> 4) );
			g2 = 0b1 & ( lfsr ^ (lfsr >> 1) ^ (lfsr >> 2)  ^ (lfsr >> 4) );
			outword |= g1 << (j + j);
			outword |= g2 << (j + j + 1);
		}
		for (j = 0; j < 4; j++ ) {
			w[j]  = 0x1 & (outword >> (0 + j - 0));
			w[j] |= 0x2 & (outword >> (4 + j - 1));
			w[j] |= 0x4 & (outword >> (8 + j - 2));
			w[j] |= 0x8 & (outword >> (12+ j - 3));
		}
		outword = w[0] | (w[1] << 4) | (w[2] << 8) | (w[3] << 12);
#ifdef PUNCTURE
		outword ^= 1 << ((5*i) & 15);
#endif
		interleaved[i] = outword;
	}

	// interleaving is self-inverse
	for (i = 0; i < 256; i++) {
		uint16_t w[4], outword;
		outword = interleaved[i];
		for (j = 0; j < 4; j++ ) {
			w[j]  = 0x1 & (outword >> (0 + j - 0));
			w[j] |= 0x2 & (outword >> (4 + j - 1));
			w[j] |= 0x4 & (outword >> (8 + j - 2));
			w[j] |= 0x8 & (outword >> (12+ j - 3));
		}
		interleaved[i] = w[0] | (w[1] << 4) | (w[2] << 8) | (w[3] << 12);
	}

	// First pass, with 12 bits of random
	uint32_t best, best2, least, test, errcount;
	lfsr = errcount = 0;
	for (i = 0; i < 256; i++) {
		least = 8;
		for (j = 0; least && (j < 4096); j++ ) {
			uint16_t k, g1, g2, outword;
			outword = 0;
			for (k = 8; k < 12; k++ ) {
				lfsr <<= 1;
				lfsr |= (j >> k) & 0b1;
			}
			for (k = 0; k < 8; k++ ) {
				lfsr <<= 1;
				lfsr |= (j >> k) & 0b1;
				g1 = 0b1 & ( lfsr ^ (lfsr >> 3) ^ (lfsr >> 4) );
				g2 = 0b1 & ( lfsr ^ (lfsr >> 1) ^ (lfsr >> 2)  ^ (lfsr >> 4) );
				outword |= g1 << (k + k);
				outword |= g2 << (k + k + 1);
			}
			test = interleaved[i] ^ outword;
			for (k = 0; test; k++ )
				test &= test - 1;
			if (k == least) {
				if (least == 1) // single bit error resolve conflict
					best2 = j;
			} else if (k < least) {
				least = k;
				best = best2 = j;
			}
		}
		errcount += least;
		scrambled[i] = best & 0xff;
		conflicts[i] = best2 & 0xff;
	}
	printf("Error count:%d\n", errcount);
#if 1
	// Resolve single bit error conflicts
	uint32_t result;
	for (i = 0; i < 256; i++) {
		uint32_t k, m, g1, g2, outdword;
		best = 16;
		for (j = 0; j < 4; j++) {
			lfsr = 0;
			m =  (i == 0) ? 0 : scrambled[i-1];
			m |= ((j & 0b01) ? conflicts[i] : scrambled[i]) << 8;
			m |= ((j & 0b10) ? conflicts[i+1] : scrambled[i+1]) << 16;
			outdword = 0;
			m >>= 4;
			for (k = 0; k < 4; k++ ) {
				lfsr <<= 1;
				lfsr |= m & 0b1;
				m >>= 1;
			}
			for (k = 0; k < 12; k++) {
				lfsr <<= 1;
				lfsr |= (m >> k) & 0b1;
				g1 = 0b1 & ( lfsr ^ (lfsr >> 3) ^ (lfsr >> 4) );
				g2 = 0b1 & ( lfsr ^ (lfsr >> 1) ^ (lfsr >> 2)  ^ (lfsr >> 4) );
				outdword |= g1 << (k + k);
				outdword |= g2 << (k + k + 1);
			}
			test = outdword ^ (interleaved[i] | (interleaved[i + 1] << 16));
			for (k = 0; test; k++ )
				test &= test - 1;
			if (k < best) {
				best = k;
				result = j;
			}
		}
		if (result & 0b01)
			scrambled[i] = conflicts[i];
	}
#endif
	// reverse scrambler
	lfsr = 0;
	for (i = 0; i < 256; i++) {
		outbyte = 0;
		for (j = 0; j < 8; j++ ) {
			lsb = (scrambled[i] >> j);
			lsb ^= (lfsr >> 12);
			lsb ^= (lfsr >> 17);
			lsb &= 0b1;
			lfsr <<= 1;
			lfsr |= (scrambled[i] >> j) & 0b1;

			outbyte |= lsb << j;
		}
		scrambled[i] = outbyte;
	}
	for (i = 0; instring[i] > 0; i++) {
		if (scrambled[i] < 32 || scrambled[i] > 126)
			scrambled[i] = '.'; // ASCII sanity check
	}
	scrambled[i] = 0;
	printf("%s\n", scrambled);

	return 0;
}
