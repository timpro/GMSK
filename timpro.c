#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define IQSIZE 1024

void exit_usage()
{
	fprintf(stderr,
		"Usage:\n"
		"\ttimpro < s16le_IQ.wav\n"
		"\nReads 48 kHz gmsk recordings.\n\n");
	exit(-1);
}

int main(int argc, char *argv[])
{
	FILE *fin = stdin;
	FILE *fout = stdout;
	int c;

	while((c = getopt(argc, argv, "h")) != -1)
	{
		switch(c)
		{
		case 'h':
		case '?': exit_usage();
		}
	}

	uint8_t result[IQSIZE];
	int16_t	*iqbuff = malloc(IQSIZE * 2 * 2);
	int32_t Ilast, Ithis, Inext;
	int32_t Qlast, Qthis, Qnext;
	int32_t x, y;
	size_t i, j, k;

	k = 0;
	Ilast = Qthis = Inext = IQSIZE;
	Qlast = Ithis = Qnext = -IQSIZE;
	while (IQSIZE == fread(iqbuff, 4, IQSIZE, fin)) {
		k++;
		for(i = 0; i < IQSIZE; i++) {
			x = Ithis * (Qlast - Qnext);
			y = Qthis * (Ilast - Inext);
			result[i] = (y > x) ? 1 : 0;
			Ilast = Ithis;
			Qlast = Qthis;
			Ithis = Inext;
			Qthis = Qnext;
			Inext = iqbuff[i*2];
			Qnext = iqbuff[i*2 + 1];
		}
		for(i = 0; i < IQSIZE; i+=4) {
			result[i >> 2] = 65 + (result[i] | (result[i + 1] << 1)
					| (result[i + 2] << 2) | (result[i + 3] << 3) );
			// printf("%c", result[i >> 2]);
		}
		printf("\n");
		fwrite(result, 1, i >> 2, fout);
	}

	if(fin != stdin)
		fclose(fin);
	if(fout != stdout)
		fclose(fout);
	free( iqbuff );
	printf("\n Read %zu packets\n", k);

	return(0);
}

