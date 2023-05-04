#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#define sleep(x) Sleep(1000*x)
#else
#include <unistd.h>
#endif

int main(int argc, char **argv)
{
	bool    is_ignore_fullstop = false;
	bool    is_stall           = false;
	FILE   *outfile            = NULL;
	int     fullstop_state     = 0;
	int     i;
	int     c;

	if (argc == 1) {
		return 0;
	}

#ifdef _WIN32
	_setmode(_fileno(stdin), O_BINARY);
#endif

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (argv[i][1] == 'x') {
				i++;
				if (i == argc) { return 0; }
				return atoi(argv[i]);
			}
			if (argv[i][1] == 'i') {
				is_ignore_fullstop = true;
				continue;
			}
			if (argv[i][1] == 'o') {
				i++;
				if (i == argc) { return 0; }
				outfile = fopen(argv[i], "ab");
			}
			if (argv[i][1] == 's') {
				is_stall = true;
				continue;
			}
		}
	}

	if (setvbuf(stdin, NULL, _IONBF, 0) != 0) {
		fprintf(stderr, "Error setting stdin to no buffering\n");
		return 0;
	}

	while (1) {
		c = fgetc(stdin);
		if (feof(stdin)) { break; }
		if (outfile) {
			fputc(c, outfile);
		}
		if (!is_ignore_fullstop) {
			switch(fullstop_state) {
				case 0: if (c == '\r') { fullstop_state = 1; } break;
				case 1: if (c == '\n') { fullstop_state = 2; } else { fullstop_state = 0; } break;
				case 2: if (c == '.') { fullstop_state = 3; } else { fullstop_state = 0; } break;
				case 3: if (c == '\r') { fullstop_state = 4; } else { fullstop_state = 0; } break;
				case 4: if (c == '\n') { return 0; } else { fullstop_state = 0; } break;
			}
		}
		if (is_stall) {
			sleep(1);
		}
	}
	if (outfile) {
		fclose(outfile);
		outfile = NULL;
	}
	return 0;
}
