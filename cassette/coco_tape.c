/* 
 * Copyright (c) 2023, Philip Kufeldt
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * This program takes a WAV encoded file of a TRS-80 Color Computer
 * Cassette recording and decodes it.
 *
 * WAV FILE INFORMATION
 * This program only supports decoding WAV files formatted as 16-bit 
 * 1-channel PCM at a frequency of 44100. 
 *
 * ENCODING INFORMATION 
 * The cassette format chosen uses a sinewave of 2400 or 1200 Hertz 
 * to yield a Baud rate of approximately 1500 Baud. In this format, 
 * a 0 (or logic low) is represented by one cycle of 1200 Hertz, and 
 * a 1 (or logic high) is represented by one cycle of 2400 Hertz. A 
 * typical program tape would consist of a leader of alternating 1's 
 * and O's, followed by one or more blocks of data. A block of data 
 * is composed of 0 to 255 bytes of data with a checksum, sync byte, 
 * and the block length. 
 * 
 * DETAILED TAPE FORMAT INFORMATION 
 * The standard TRS-80 Color Computer tape is composed of the following 
 * items: 
 *	1. A leader consisting of 128 bytes of Hex 55 
 *	2. A Namefile block 
 *	3. A blank section of tape approximately equal to 0.5 seconds 
 * 	   in length; this allows BASIC time to evaluate the Namefile. 
 *	4. A second leader of 128 bytes of Hex 55 
 *	5. One or more Data blocks 
 *	6. An End of File block 
 *
 * The block format for Data blocks, Namefile blocks, or an End of File 
 * block is as follows: 
 *	1. One leader byte — 
 * 		55H = Leader byte
 *	2. One sync byte — 
 *		3CH = Sync byte
 *	3. One block type byte — 
 *		00H = Namefile 
 *		01H = Data 
 *		FFH = End of File 
 *	4. One block length byte — 
 *		00H to FFH 
 *	5. Data — 0 to 255 bytes 
 *	6. One checksum byte — 
 *		the sum of all the data plus block type and block length 
 *	7. One leader byte — 
 * 		55H = Leader byte
 *
 * The End of File block is a standard block with a length of 0 and 
 * the block type equal to FFH. 
 *
 * The Namefile block is a standard block with a length of 15 bytes (0FH) 
 * and the block type equals 00H. The 15 bytes of data provide information 
 * to BASIC and are employed as described below: 
 *	1. Eight bytes for the program name 
 *	2. One file type byte — 
 *		00H = BASIC 
 *		01H = Data 
 *		02H = Machine Language 
 *	3. One ASCII flag byte — 
 *		00H = Binary 
 *		FFH = ASCII 
 *	4. One Gap flag byte — 
 *		01H = Continuous 
 *		FFH = Gaps 
 *	5. Two bytes for the start address of a machine language program 
 *	6. Two bytes for the load address of a machine language program 
 * 
 * BASIC DATA BLOCK FORMAT
 * A BASIC data block is a byte array with up to 255 bytes. The format has
 * been reverse engineered and it appears to be line oriented with 
 * a null termination. Lines contain some metadata and are encoded with
 * CoCo BASIC tokens. The null termination may not be the correct way to
 * define a line as the meta data does seem to point at the beginning 
 * of the next line. However there are unexplained issue preventing its use
 * see below. 
 *
 * LINE FORMAT
 *	Offset:		Type:	Value:
 *	0		byte	Next Line Data Block Number (NLDBN)
 * 	1		byte	Next Line Offset in NLDBN (NLO)
 *	2:3		word	BASIC Program line number
 *	4-[NLDBN:NLO]	byte[]	Encoded BASIC Program line
 *
 * Block numbers seem to start at 30 (0x1e). The NLDBN can point to the next
 * data block allowing lines to span data blocks. NLO is always relative to
 * the beginning of NLDBN. 
 * 
 * NLDBN:NLO ISSUES
 * NLDBN:NLO seems to be inconsistent wrt to where the next line lies.
 * When NLDBN is the first data block NLO seems to point to +1 into the
 * next line. Then when NLDBN is the second data block NLO points to the 
 * the actual next line. Then when NLDBN is the third data block
 * NLO points to -1 of the next line and so on.  Currently this code 
 * does not use NLDBN:NLO because of this. It just searches for the 
 * next null, to terminate the line. 
 *
 * DECODING INFORMATION
 * This program takes a simple approach to decoding the data in the WAV file.
 * Since bits are encoded as a sine wave with a freq of either a 1200Hz (0)
 * or 2400Hz (1) AND since the WAV file format samples at a fixed rate of
 * 44100Hz, the number of data data points per sine wave cycle can identify 
 * the frequency. That is an encoded 1 will have 44100/2400=18.375 data 
 * points per cycle and an encoded 0 will have 44100/1200=36.75 data 
 * points per cycle. To find a cycle this code looks for a falling zero
 * crossing in the WAV data then starts counting the data points until the 
 * next falling zero crossing. 
 *
 * However, the CoCos 6 bit A/D converter made the accurancy of a 1200/2400
 * fequency very suspect, plus the variability in recordings and noise, means 
 * that the WAV data is not that precise, so data counts can not be fixed 
 * numbers but rather ranges.
 * 
 * During testing it was determined that a 1 (high) was defined by the range
 * of 18-31, and a 0 (low) was 31-inf. These may prove to be slightly different
 * on a per recording basis, so params are provided to define them at runtime.
 */
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define PRINT_ERROR(a, args...) printf("ERROR %s() %s Line %d: " a "\n", \
				       __FUNCTION__, __FILE__, __LINE__, ##args);

/* Holds wav file data */
typedef struct {
	uint32_t samples;
	int16_t *data;
} sound_t;

enum blocktype {
	BT_NAME 	= 0x00,		/* Name block */
	BT_DATA 	= 0x01,		/* Data block */
	BT_EOF		= 0xFF,		/* End of file block */
};

/* Name blocks define what is in subsequent data blocks */
enum filetype {
	FT_BASIC	= 0x00,		/* CoCo encoded BASIC */
	FT_DATA		= 0x01,		/* raw data?? */
	FT_ML		= 0x02,		/* machine language code */
};

/* Defines if data is binary or ascii, so far only seen binary */
enum asciiflag {
	AF_BINARY	= 0x00,
	AF_ASCII	= 0xFF,
};

/* 
 * Not sure what gapflag is for - service manual only defines 
 * continuous and gaps but have only seen 0x00.
 */
enum gapflag {
	GF_UNKNOWN	= 0x00,
	GF_CONT		= 0x01,
	GF_GAPS		= 0xFF,
};

#define SYNCBYTE	0x3C
#define LEADERBYTE	0x55

#define PROGNAMELEN 	8
#define MLSTARTLEN 	2
#define MLLOADLEN 	2
#define NAMEBLOCKLEN    15

/* State machine states for reading in data */ 
enum block_state {
	BS_NEED_LEADBYTE,
	BS_NEED_SYNCBYTE,
	BS_NEED_BLOCKTYPE,
	BS_NEED_LENGTH,
	BS_NEED_DATA,
	BS_NEED_NAME,
	BS_NEED_FILETYPE,
	BS_NEED_ASCIIFLAG,
	BS_NEED_GAPFLAG,
	BS_NEED_STARTADDR,
	BS_NEED_LOADADDR,
	BS_NEED_CKSUM,
	BS_DONE,
};

struct block {
	enum block_state b_state;	/* State machine value for decoding */
	struct block	*b_next;	/* Block list ptr */

	enum blocktype	b_type;
	uint8_t		b_length;
	uint8_t		b_cksum;
	uint8_t		*b_data;

	/* Data specific to b_type == FT_NAME */
	char		b_progname[PROGNAMELEN+1];
	enum filetype	b_filetype;
	enum asciiflag	b_asciiflag;
	enum gapflag	b_gapflag;
	uint8_t		b_mlstart[MLSTARTLEN];
	uint8_t		b_mlload[MLLOADLEN];

	/* Decoding scratch data */
	uint8_t 	b_byte;
	uint8_t 	b_nbit;
	uint8_t		b_data_i;
	uint8_t		b_progname_i;
	uint8_t		b_mlstart_i;
	uint8_t		b_mlload_i;
};

/* 
 * For binary encoded CoCo BASIC Programs, 
 * might be called tokenized BASIC Format 
*/
char token[][15] = {
	/* Operator tokens */
	
	/* 0x80 */ "FOR", "GO", "REM", "'",
	/* 0x84 */ "ELSE", "IF", "DATA", "PRINT",
	/* 0x88 */ "ON", "INPUT", "END", "NEXT",
	/* 0x8b */ "DIM", "READ", "RUN", "RESTORE",
	/* 0x90	*/ "RETURN", "STOP", "POKE", "CONT",
	/* 0x94	*/ "LIST", "CLEAR","NEW", "CLOAD",
	/* 0x98	*/ "CSAVE", "OPEN", "CLOSE", "LLIST",
	/* 0x9b	*/ "SET", "RESET", "CLS", "MOTOR",
	/* 0xa0	*/ "SOUND", "AUDIO", "EXEC", "SKIPF",
	/* 0xa4	*/ "TAB(", "TO", "SUB", "THEN",
	/* 0xa8	*/ "NOT", "STEP", "OFF", "+",
	/* 0xab	*/ "-", "*", "/", "^",
	/* 0xb0	*/ "AND", "OR", ">", "=",
	/* 0xb4	*/ "<", "DEL", "EDIT", "TRON",
	/* 0xb8	*/ "TROFF", "DEF", "LET", "LINE",
	/* 0xbb	*/ "PCLS", "PSET", "PRESET", "SCREEN",
	/* 0xc0	*/ "PCLEAR", "COLOR", "CIRCLE", "PAINT",
	/* 0xc4	*/ "GET", "PUT", "DRAW", "PCOPY",
	/* 0xc8	*/ "PMODE", "PLAY", "DLOAD", "RENUM",
	/* 0xcb */ "FN", "USING",

	/* RSDOS adds these .. (from Dragon User 12/84) */
	/* 0xce	*/ "DIR", "DRIVE",
	/* 0xd0	*/ "FIELD", "FILES", "KILL", "LOAD",
	/* 0xd4 */ "LSET", "MERGE", "RENAME", "RSET",
	/* 0xd8	*/ "SAVE", "WRITE", "VERIFY", "UNLOAD",
	/* 0xdb */ "DSKINI", "BACKUP", "COPY", "DSKI$",
	/* 0xe0	*/ "DSKO$",
};

char ftoken[][15] = {
	/* Function tokens - proceeded by 0xff to differentiate from operators */

	/* 0x80 */ "SGN", "INT", "ABS", "USR",
	/* 0x84 */ "RND", "SIN", "PEEK", "LEN", 
	/* 0x88 */ "STR$", "VAL", "ASC", "CHR$",
	/* 0x8b */ "EOF", "JOYSTK", "LEFT$", "RIGHT$", 
	/* 0x90 */ "MID$", "POINT", "INKEY$", "MEM",
	/* 0x94 */ "ATN", "COS", "TAN", "EXP", 
	/* 0x98 */ "FIX", "LOG", "POS", "SQR",
	/* 0x9b */ "HEX$", "VARPTR", "INSTR", "TIMER", 
	/* 0xa0 */ "PPOINT", "STRING$",

	/* RSDOS adds these .. (from Dragon User 12/84) */
	/* 0xa2 */ "CVN", "FREE",
	/* 0xa4 */ "LOC", "LOF", "MKN$",
};

#define ZL 31
#define ZH 1000
#define OL 18
#define OH 31

char *progname;
int d_debug = 0;
int z_zero_low	= ZL;
int Z_zero_high	= ZH;
int o_one_low	= OL;
int O_one_high	= OH;
int v_verbose = 0;

bool load_wav(const char *filename, sound_t *sound);
int  process_bit(struct block *cb);
int  print_prog(struct block *cb);
void hexdump(const void* data, size_t size);


void
usage()
{
        fprintf(stderr, "Usage: %s [OPTIONS] FILENAME\n", progname);

	char msg[] = "\n\
Where, OPTIONS are [default]:\n\
	-d           Turn on debugging output\n\
	-z           Low num of data points that correspond to a zero [32]\n\
	-Z           High num of data points that correspond to a zero [inf]\n\
	-o           Low num of data points that correspond to a one [18]\n\
	-O           High num of data points that correspond to a one [31]\n\
	-v           Turn on verbose output\n\
	-?           Help\n\
\n\
Where, FILENAME is a 16-bit 1-channel PCM .WAV encoded file containing\n\
a Color Computer Cassette audio recording.\n\
";

	fprintf(stderr, "%s", msg);
	exit(1);
}


int
main(int argc, char *argv[])
{


	extern char     *optarg;
        extern int	optind, opterr, optopt;
        char		c, *cp, *filename=NULL;
	int32_t		count = 0, nblocks=0;
	sound_t 	wav;
	struct block 	*blocks = NULL;		/* Root block list ptr */
	struct block 	*cb = NULL;		/* Current block ptr */
	struct block 	*pb = NULL;		/* Previous block ptr */

	progname = argv[0];
	
        while ((c = getopt(argc, argv, "doOzZvh?")) != (char)EOF) {
                switch (c) {
		case 'd':
			d_debug = 1;
			break;

		case 'o':
		case 'O':
		case 'z':
		case 'Z':
			if (optarg[0] == '-') {
				fprintf(stderr, "*** Negative value %s\n",
				       optarg);
				usage();
				return(-1);
			}

			count = strtol(optarg, &cp, 0);
			if (!cp || *cp != '\0') {
				fprintf(stderr, "**** Invalid Value %s\n",
				       optarg);
				usage();
				return(-1);
			}
			if (count > 10000) {
				fprintf(stderr, "**** Value too large %s\n",
				       optarg);
				usage();
				return(-1);
			}

			if (c == 'o') o_one_low   = count;
			if (c == 'O') O_one_high  = count;
			if (c == 'z') z_zero_low  = count;
			if (c == 'Z') Z_zero_high = count;
			count = 0;
			break;
			
		case 'v':
			v_verbose = 1;
			break;

		case 'h':
                case '?':
                default:
                        usage();
			return(-1);
		}
        }

	// Check for the key and value parms
	if (argc - optind == 1) {
		filename = argv[optind];
	} else if (argc - optind < 1) {
		fprintf(stderr, "**** Missing FILENAME\n");
		usage();
	} else {
		fprintf(stderr, "**** Too many arguments\n");
		usage();
	}

	if(!load_wav(filename, &wav)) {
		PRINT_ERROR("Failed to load .wav");
		return -1;
	}

	if (v_verbose) printf ("Samples:  %d\n", wav.samples);
	
	for(int j = 1; j < wav.samples; j++) {
		//		printf("WAV: %d\n",wav.data[j]); 
		if (!cb) {
			/* need to allocate a block */
			cb = (struct block *)malloc(sizeof(struct block));
			if (!cb) {
				PRINT_ERROR("Failed to malloc CB");
				return -1;
			}

			memset(cb, 0, sizeof(struct block));
			cb->b_state = BS_NEED_SYNCBYTE;

			if (!blocks) blocks = cb;
			if (pb) pb->b_next = cb;
			pb = cb;
			nblocks++;
		}

		/* Use falling zero crossings to determine a cycle */
		if ((wav.data[j] < 0) &&
		    (wav.data[j-1] >= 0)) {
			/* Falling zero crossing */ 
			if (d_debug && cb->b_state == BS_NEED_LENGTH)
				printf("count: %d\n", count);

			if ((count >= o_one_low) &&
			    (count <= O_one_high)) {
				/* Found a 1 */
				cb->b_byte = (cb->b_byte >> 1) | 0x80;
			} else if ((count >= z_zero_low) &&
				 (count <= Z_zero_high)) {
				/* Found a 0 */
				cb->b_byte = (cb->b_byte >> 1);
			} else {
				if (d_debug) {
					printf("Not 1200/2400Hz waveform: %d\n",
					       count);
					for(int k=j-50; k<j+50; k++)
						if (cb->b_state == BS_NEED_DATA)
							printf("WAV: %d\n",
							       wav.data[k]);
				}
			}
			//printf("Curr Byte: 0x%02x\n", cb->b_byte);
			
			if (process_bit(cb)) {
				exit(1);
			}
			if (cb->b_state == BS_DONE) { 
				if (cb->b_type == BT_EOF) {
					/* Completed a prog */
					print_prog(blocks);

					/* Free up the blocks */
					pb = blocks;
					while (pb) {
						cb = pb->b_next;
						free(pb->b_data);
						free(pb);
						pb = cb;
					}
					blocks = pb = NULL;
				}
				/* Time to start another block */
				cb = NULL;
			}

			/* reset the data point count, to start next cycle */
			count = 0;
		}
		count++;
	}

	print_prog(blocks);

	if (v_verbose) {
		printf("Decoded %d blocks\n", nblocks);
		for (cb=blocks; cb->b_next; cb=cb->b_next) {
			switch (cb->b_type) {
			case BT_NAME:
				printf("Name Block\n");
				break;
			case BT_DATA:
				printf("DATA Block (%d)\n", cb->b_length);
				break;
			case BT_EOF:
				printf("EOF Block\n");
				break;
			default:
				printf("Bad block type %d\n", cb->b_type);
				break;
			}
		}
	}

	exit(0);
}

/*
* print a buffer as an ascii string but where chars are unprintable replace
* then with a "\HH" notation where H is an ascii hexdigit.
* ex "O\x01--\x7F\xFF"
*/
void
asciidump(const void* data, size_t size)
{
	size_t i;
	
	for (i = 0; i < size; ++i) {
		if (isprint((int)(((unsigned char*)data)[i]))) {
			printf("%c", ((unsigned char*)data)[i]);
		} else {
			if ((((unsigned char *)data)[i] > 0x7f) &&
			    (((unsigned char *)data)[i] < 0xe0)) {
				printf("%s", token[((unsigned char *)data)[i]-0x80]);
			} else if (((unsigned char *)data)[i] == 0xff) {
				i++;
				printf("%s", ftoken[((unsigned char *)data)[i]-0x80]);				} else {
				if (((unsigned char*)data)[i])
					printf("\\x%02X", ((unsigned char*)data)[i]);
			}
		}
	}
}

int
print_prog(struct block *cb)
{
#define LINELEN 4096
	
	int i, j, llen  ;
	uint16_t lineno;
	uint8_t eol, blkn, nl, line[LINELEN];
	
	if (cb && (cb->b_state == BS_DONE) && (cb->b_type == BT_NAME)) {
		printf("Program: %8s\n", cb->b_progname);
	}
		       
	while (cb && (cb->b_type != BT_DATA))
		cb =cb->b_next;

	if (!cb) return(0);
	
	blkn = cb->b_data[0];
	if (d_debug) printf("Block %d\n", blkn);
	
	i=0;
	while(cb) {
		/* Three trailing nulls seem to terminate the data */
		/* Careful - this might span data blocks - checked not handled */
		if (((cb->b_length - i) == 2) &&
		    (cb->b_data[i]   == 0) &&
		    (cb->b_data[i+1] == 0) &&
		    (cb->b_data[i+2] == 0)) {
			/* We're done here */
			return(0);
		}
		
		if ((cb->b_data[i] != blkn) && (cb->b_data[i] != blkn+1))  {
			printf("bad start of line 0x%02x != 0x%02x 0x%02x\n",
			       cb->b_data[i], blkn, i);
			hexdump(cb->b_data, cb->b_length);
			exit(1);
		}

		/* next byte - remember it might span data blocks */
		i++;
		if (i == cb->b_length) {
			/* time to jump */
			i = 0;
			cb = cb->b_next;
			blkn++;
		}
		
		/* Ignoring next line offset byte - using null to term a line */
		/* nl = cb->b_data[i] */
		
		/* next byte - remember it might span data blocks */
		i++;
		if (i == cb->b_length) {
			/* time to jump */
			i = 0;
			cb = cb->b_next;
			blkn++;
		}


		lineno = (uint16_t)cb->b_data[i] << 8;

		/* next byte - remember it might span data blocks */
		i++;
		if (i == cb->b_length) {
			/* time to jump */
			i = 0;
			cb = cb->b_next;
			blkn++;
		}

		lineno = lineno | (uint16_t)cb->b_data[i];

		/* next byte - remember it might span data blocks */
		i++;
		if (i == cb->b_length) {
			/* time to jump */
			i = 0;
			cb = cb->b_next;
			blkn++;
		}

		/* Copy the line - copy because it may span blocks - 
		 * assumes lines never longer than line buffer 
		 */
		j=0; llen=0;
		while (cb->b_data[i] != 0x00) {
			line[j++] = cb->b_data[i];
			llen++;

			/* next byte - remember it might span data blocks */
			i++;
			if (i == cb->b_length) {
				/* time to span */
				i = 0;
				cb = cb->b_next;
				blkn++;
			}

			if (j>=LINELEN) {
				printf("Line too big for buffer (%d>=%d)\n",
				       j, LINELEN);
				exit(1);
			}
		}

		/* next byte - remember it might span data blocks */
		i++;
		if (i == cb->b_length) {
			/* time to jump */
			i = 0;
			cb = cb->b_next;
			blkn++;
		}
				
		//printf("%d:%d [%3d] %3d ", blkn, nl, i,  lineno);
		printf("%5d ", lineno);
		asciidump(line, llen);
		memset(line, 0, LINELEN);
		printf("\n");
	}
		
}


int
process_bit(struct block *cb)
{
	switch (cb->b_state) {
	case BS_NEED_SYNCBYTE:
		if (cb->b_byte == SYNCBYTE) {
			/* Found header */
			if (d_debug)
				printf("Found header byte: 0x%02x\n",
				       cb->b_byte);
			cb->b_byte = 0;
			cb->b_nbit = 1;
			cb->b_state = BS_NEED_BLOCKTYPE;
		}
		break;
		
	case BS_NEED_BLOCKTYPE:
		if (cb->b_nbit == 8) {
			if (d_debug)
				printf("Found BLOCK TYPE: 0x%02x\n", cb->b_byte);
			if ((cb->b_byte == BT_NAME) ||
			    (cb->b_byte == BT_DATA) ||
			    (cb->b_byte == BT_EOF))  {
				    cb->b_type = cb->b_byte;
				    cb->b_cksum = cb->b_byte;
				    cb->b_byte = 0;
				    cb->b_nbit = 0;
				    cb->b_state = BS_NEED_LENGTH;
			    } else {
				    cb->b_byte = 0;
				    cb->b_nbit = 0;
				    cb->b_state = BS_NEED_SYNCBYTE;
				    if (d_debug)
					    printf("Found bad block type, resetting\n");
			    }
				    
		}
		cb->b_nbit++;
		break;
		
	case BS_NEED_LENGTH:
		if (cb->b_nbit == 8) {
			if (d_debug)
				printf("Found LENGTH: 0x%02x\n", cb->b_byte);
			cb->b_length = cb->b_byte;
			cb->b_cksum += cb->b_byte;
			cb->b_byte = 0;
			cb->b_nbit = 0;
			if (cb->b_type == BT_NAME)  {
				if (cb->b_length != NAMEBLOCKLEN) {
				    cb->b_byte = 0;
				    cb->b_nbit = 0;
				    cb->b_state = BS_NEED_SYNCBYTE;
				    printf("TYPE: 0x%02x\n", cb->b_type);
				    printf("Found bad block len, resetting\n");
				} else 
					cb->b_state = BS_NEED_NAME;
			} else if (cb->b_type == BT_EOF) {
				if (cb->b_length != 0) {
				    cb->b_byte = 0;
				    cb->b_nbit = 0;
				    cb->b_state = BS_NEED_SYNCBYTE;
				    printf("TYPE: 0x%02x\n", cb->b_type);
				    printf("Found bad block len, resetting\n");
				} else
					cb->b_state = BS_NEED_CKSUM;
			} else {
				cb->b_state = BS_NEED_DATA;
				cb->b_data = (uint8_t *)malloc(cb->b_length+1);
				if (!cb->b_data) {
					PRINT_ERROR("Data malloc failed\n");
					return(1);
				}
				memset(cb->b_data, 0, cb->b_length+1);
			}
		}
		cb->b_nbit++;
		break;
		
	case BS_NEED_NAME:
		if (cb->b_nbit == 8) {
			if (d_debug)
				printf("Found NAME BYTE: 0x%02x\n", cb->b_byte);
			cb->b_progname[cb->b_progname_i++] = cb->b_byte;
			cb->b_cksum += cb->b_byte;
			if (cb->b_progname_i == PROGNAMELEN) {
				if (d_debug)
					printf("Name: %s\n", cb->b_progname);
				cb->b_state = BS_NEED_FILETYPE;
			}
			
			cb->b_byte = 0;
			cb->b_nbit = 0;
		}
		cb->b_nbit++;
		break;
		
	case BS_NEED_FILETYPE:
		if (cb->b_nbit == 8) {
			if (d_debug)
				printf("Found FILETYPE: 0x%02x\n", cb->b_byte);
			cb->b_filetype = cb->b_byte;
			cb->b_cksum += cb->b_byte;
			cb->b_state = BS_NEED_ASCIIFLAG;
			
			cb->b_byte = 0;
			cb->b_nbit = 0;
		}
		cb->b_nbit++;
		break;
		
	case BS_NEED_ASCIIFLAG:
		if (cb->b_nbit == 8) {
			if (d_debug)
				printf("Found ASCIIFLAG: 0x%02x\n", cb->b_byte);
			cb->b_asciiflag = cb->b_byte;
			cb->b_cksum += cb->b_byte;
			cb->b_state = BS_NEED_GAPFLAG;
			
			cb->b_byte = 0;
			cb->b_nbit = 0;
		}
		cb->b_nbit++;
		break;
		
	case BS_NEED_GAPFLAG:
		if (cb->b_nbit == 8) {
			if (d_debug)
				printf("Found GAPFLAG: 0x%02x\n", cb->b_byte);
			cb->b_gapflag = cb->b_byte;
			cb->b_cksum += cb->b_byte;
			cb->b_state = BS_NEED_STARTADDR;
			
			cb->b_byte = 0;
			cb->b_nbit = 0;
		}
		cb->b_nbit++;
		break;
		
	case BS_NEED_STARTADDR:
		if (cb->b_nbit == 8) {
			if (d_debug)
				printf("Found START ADDR BYTE: 0x%02x\n",
				       cb->b_byte);
			cb->b_mlstart[cb->b_mlstart_i++] = cb->b_byte;
			cb->b_cksum += cb->b_byte;
			if (cb->b_mlstart_i == MLSTARTLEN) {
				if (d_debug)
					printf("Machine Language Start: 0x%04x\n",
				       *(uint16_t *)cb->b_mlstart);
				cb->b_state = BS_NEED_LOADADDR;
			}
			
			cb->b_byte = 0;
			cb->b_nbit = 0;
		}
		cb->b_nbit++;
		break;
		
	case BS_NEED_LOADADDR:
		if (cb->b_nbit == 8) {
			if (d_debug)
				printf("Found LOAD ADDR BYTE: 0x%02x\n",
				       cb->b_byte);
			cb->b_mlload[cb->b_mlload_i++] = cb->b_byte;
			cb->b_cksum += cb->b_byte;
			cb->b_length--;
			if (cb->b_mlload_i == MLLOADLEN) {
				if (d_debug)
					printf("Machine Language Load: 0x%04x\n",
					       *(uint16_t *)cb->b_mlload);
				cb->b_state = BS_NEED_CKSUM;
			}
			cb->b_byte = 0;
			cb->b_nbit = 0;
		}
		cb->b_nbit++;
		break;
		
	case BS_NEED_DATA:
		if (cb->b_nbit == 8) {
			if (d_debug)
				printf("Found DATA: 0x%02x\n", cb->b_byte);
			cb->b_data[cb->b_data_i++] = cb->b_byte;
			cb->b_cksum += cb->b_byte;
			if (cb->b_length == cb->b_data_i) {
				if (d_debug) {
					printf("Length: 0x%02x\n", cb->b_data_i);
					hexdump(cb->b_data, cb->b_data_i);
				}
				cb->b_state = BS_NEED_CKSUM;
			}
			
			cb->b_byte = 0;
			cb->b_nbit = 0;
		}
		cb->b_nbit++;
		
		break;
		
	case BS_NEED_CKSUM:
		if (cb->b_nbit == 8) {
			if (d_debug) {
				printf("Found CKSUM: 0x%02x\n", cb->b_byte);
				printf("Checksum: 0x%02x\n", cb->b_cksum);
			}
			if (cb->b_byte != cb->b_cksum) {
				PRINT_ERROR("Decode Error: chksum\n");
				return(1);
			}
			cb->b_state = BS_NEED_LEADBYTE;
			
			cb->b_byte = 0;
			cb->b_nbit = 0;
		}
		cb->b_nbit++;
		break;
		
	case BS_NEED_LEADBYTE:
		if (cb->b_nbit == 8) {
			if (d_debug)
				printf("Found LEADBYTE: 0x%02x\n", cb->b_byte);
			cb->b_byte = 0;
			cb->b_nbit = 0;
			cb->b_state = BS_DONE;
		}
		cb->b_nbit++;
		break;
		
	default:
		PRINT_ERROR("Bad Block state\n");
		return(1);
	}

	return(0);
}

/* 
 * Loads ONLY 16-bit 1-channel PCM .WAV files. 
 * Allocates sound->data and fills with the pcm data. 
 * Fills sound->samples with the number of ELEMENTS in sound->data. 
 * EG for 2-bytes per sample single channel, sound->samples = HALF 
 * of the number of bytes in sound->data.
 */
bool load_wav(const char *filename, sound_t *sound) {
	bool return_value = true;
	FILE *file;
	char magic[4];
	int32_t filesize;
	int32_t format_length;		// 16
	int16_t format_type;		// 1 = PCM
	int16_t num_channels;		// 1
	int32_t sample_rate;		// 44100
	int32_t bytes_per_second;	// sample_rate * num_chans * bits_per_sample / 8
	int16_t block_align;		// num_channels * bits_per_sample / 8
	int16_t bits_per_sample;	// 16
	int32_t data_size;

	file = fopen(filename, "rb");
	if(file == NULL) {
		PRINT_ERROR("%s: Failed to open file", filename);
		return false;
	}

	fread(magic, 1, 4, file);
	if(magic[0] != 'R' || magic[1] != 'I' || magic[2] != 'F' || magic[3] != 'F') {
		PRINT_ERROR("%s First 4 bytes should be \"RIFF\", are \"%4s\"", filename, magic);
		return_value = false;
		goto CLOSE_FILE;
	}

	fread(&filesize, 4, 1, file);

	fread(magic, 1, 4, file);
	if(magic[0] != 'W' || magic[1] != 'A' || magic[2] != 'V' || magic[3] != 'E') {
		PRINT_ERROR("%s 4 bytes should be \"WAVE\", are \"%4s\"", filename, magic);
		return_value = false;
		goto CLOSE_FILE;
	}

	fread(magic, 1, 4, file);
	if(magic[0] != 'f' || magic[1] != 'm' || magic[2] != 't' || magic[3] != ' ') {
		PRINT_ERROR("%s 4 bytes should be \"fmt/0\", are \"%4s\"", filename, magic);
		return_value = false;
		goto CLOSE_FILE;
	}

	fread(&format_length, 4, 1, file);
	fread(&format_type, 2, 1, file);
	if(format_type != 1) {
		PRINT_ERROR("%s format type should be 1, is %d", filename, format_type);
		return_value = false;
		goto CLOSE_FILE;
	}

	fread(&num_channels, 2, 1, file);
	if(num_channels != 1) {
		PRINT_ERROR("%s Number of channels should be 1, is %d", filename, num_channels);
		return_value = false;
		goto CLOSE_FILE;
	}

	fread(&sample_rate, 4, 1, file);
	if(sample_rate != 44100) {
		PRINT_ERROR("%s Sample rate should be 44100, is %d", filename, sample_rate);
		return_value = false;
		goto CLOSE_FILE;
	}

	fread(&bytes_per_second, 4, 1, file);
	fread(&block_align, 2, 1, file);
	fread(&bits_per_sample, 2, 1, file);
	if(bits_per_sample != 16) {
		PRINT_ERROR("%s bits per sample should be 16, is %d", filename, bits_per_sample);
		return_value = false;
		goto CLOSE_FILE;
	}

	fread(magic, 1, 4, file);
	if(magic[0] != 'd' || magic[1] != 'a' || magic[2] != 't' || magic[3] != 'a') {
		PRINT_ERROR("%s 4 bytes should be \"data\", are \"%4s\"", filename, magic);
		return_value = false;
		goto CLOSE_FILE;
	}

	fread(&data_size, 4, 1, file);
	sound->data = malloc(data_size);
	if(sound->data == NULL) {
		PRINT_ERROR("%s Failed to allocate %d bytes for data", filename, data_size);
		return_value = false;
		goto CLOSE_FILE;
	}

	if(fread(sound->data, 1, data_size, file) != data_size) {
		PRINT_ERROR("%s Failed to read data bytes", filename);
		return_value = false;
		free(sound->data);
		goto CLOSE_FILE;
	}

	sound->samples = data_size / 2;

	CLOSE_FILE:
	fclose(file);

	return return_value;
}


void
hexdump(const void* data, size_t size)
{
	char offset[10];	/* Byte Offset string */
	char line[81];		/* Current line */
	char lline[81];		/* Last line for comparison */
#define SEPSTR " |  "
	char sep[81];		/* Built separator string */
	char hexch[5];		/* Formatted hex character */
	size_t lines, l, i, j, bpl=16, lb, bsl=3, lo, repeat = 0;

	/*
	 * dump the output line by line. This way if lines are
	 * repeated they can be detected and not printed again.
	 * The hex portion is a little inefficient as it is O(x^2)
	 * but is simple to read.
	 *
	 * bpl is Bytes per Line
	 * lb is Line Bytes, mostly lb = bpl except for last line
	 * bsl is Byte String Length, length of single byte in ascii + " "
	 * lo is Line Offset
	 */
	lines = (size / bpl) + ((size % bpl)?1:0);
	lline[0] = '\0';
	lb = bpl;
	for (l=0,i=0; l<lines; i+=lb, l++) {

		/* Last line update bpl if necessary */
		if (l == (lines - 1))
			if (size % bpl)
				lb = size % bpl;

		/* Offset */
		sprintf(offset, "%08x ", (unsigned int)(l * bpl));

		/* Hex dump */
		line[0] = '\0';
		for (j = 0; j < lb; ++j) {
			sprintf(hexch, "%02X ", ((unsigned char*)data)[i+j]);
			strcat(line, hexch);
		}

		/* Add Hex Ascii Separator, variable width */
		sprintf(sep, "%*s",
			(int)(((bpl - lb) * bsl) + strlen(SEPSTR)), SEPSTR);
		strcat(line, sep);

		/* Ascii dump */
		lo = strlen(line);
		for (j = 0; j < lb; ++j) {
			if (isprint(((unsigned char *)data)[i+j]))
				line[lo] = ((char *)data)[i+j];
			else
				line[lo] = '.';
			lo++;
		}
		line[lo] = '\0';

		/* check if repeated */
		if (strcmp(line, lline)) {
			/* Non repeated line, print it */
			if (repeat)
				printf("    Last line repeated %ld time(s)\n",
				       repeat);

			printf("%s%s\n", offset, line);

			/* Save last line */
			strcpy(lline, line);
			repeat = 0;
		} else {
			/* Repeated, don't print just count */
			repeat++;
		}
	}

	if (repeat)
		printf("Line repeated %ld time(s)\n", repeat);
}


