#include <stdio.h>      // fprintf(), stdout, setlinebuf()
#include <stdlib.h>     // EXIT_SUCCESS, EXIT_FAILURE, rand()
#include <stdint.h>     // uintmax_t
#include <time.h>       // nanosleep(), struct timespec
#include <signal.h>     // sigaction(), struct sigaction
#include <termios.h>    // struct winsize 
#include <sys/ioctl.h>  // ioctl(), TIOCGWINSZ

#define ANSI_FONT_RESET "\x1b[0m"
#define ANSI_FONT_BOLD  "\x1b[1m"
#define ANSI_FONT_FAINT "\x1b[2m"

#define ANSI_FONT_FG_WHITE1 "\x1b[38;5;194m"
#define ANSI_FONT_FG_WHITE2 "\x1b[38;5;157m"
#define ANSI_FONT_FG_WHITE3 "\x1b[38;5;120m"
#define ANSI_FONT_FG_GREEN1 "\x1b[38;5;46m"
#define ANSI_FONT_FG_GREEN2 "\x1b[38;5;40m"
#define ANSI_FONT_FG_GREEN3 "\x1b[38;5;34m"
#define ANSI_FONT_FG_GREEN4 "\x1b[38;5;28m"
#define ANSI_FONT_FG_GREEN5 "\x1b[38;5;22m"

#define ANSI_HIDE_CURSOR  "\e[?25l"
#define ANSI_SHOW_CURSOR  "\e[?25h"

#define BITMASK_ASCII 0x00FF
#define BITMASK_STATE 0x0300
#define BITMASK_TSIZE 0xFC00

#define STATE_NONE 0
#define STATE_DROP 1
#define STATE_TAIL 2

#define DEBUG_ASCII 1
#define DEBUG_STATE 2
#define DEBUG_TSIZE 3

#define TSIZE_MIN 8
#define TSIZE_MAX 252

#define GLITCH_RATIO 0.02
#define DROP_RATIO   0.015

static volatile int resized;
static volatile int running;   // controls running of the main loop 
static volatile int handled;   // last signal that has been handled 

enum colors {
	COLOR_FG_WHITE1 = 195,
	COLOR_FG_WHITE2 = 157,
	COLOR_FG_WHITE3 = 120,
	COLOR_FG_GREEN1 = 46,
	COLOR_FG_GREEN2 = 40,
	COLOR_FG_GREEN3 = 34,
	COLOR_FG_GREEN4 = 28,
	COLOR_FG_GREEN5 = 22
};

//char *colors[] = { ANSI_FONT_FG_GREEN1, ANSI_FONT_FG_GREEN2, ANSI_FONT_FG_GREEN3, ANSI_FONT_FG_GREEN4, ANSI_FONT_FG_GREEN5 };

uint8_t greens[] = { 48, 41, 35, 29, 22 };

//
//  The matrix' data represents a 2D array of size cols * rows.
//  Every data element is a 16 bit int which stores information
//  about that matrix cell as follows:
//
//  128 64  32  16   8   4   2   1  128 64  32  16   8   4   2   1
//   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
//   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0
//  '---------------------' '-----' '-----------------------------'
//          TSIZE            STATE               ASCII
//
//  ASCII: the char code to display (values 32 through 126)
//  STATE: 0 for NONE, 1 for DROP or 2 for TAIL
//  TSIZE: length of tail (for DROP) or color intensity (for TAIL)
//

typedef struct matrix
{
	uint16_t *data;
	int cols;
	int rows;
}
matrix_s;

static void
on_signal(int sig)
{
	switch (sig)
	{
		case SIGWINCH:
			resized = 1;
			break;
		case SIGKILL:
		case SIGQUIT:
		case SIGTERM:
			running = 0;
			break;
	}
	handled = sig;
}

static int
rand_int(int min, int max)
{
	return min + rand() % ((max + 1) - min);
}

static float
rand_float()
{
	return (float) rand() / (float) RAND_MAX;
}

static int
rand_int_mincap(int min, int max)
{
	int r = rand() % max;
	return r < min ? min : r;
}

static uint8_t 
rand_ascii()
{
	return rand_int_mincap(32, 126);
}

static void
color_fg(uint8_t color)
{
	fprintf(stdout, "\x1b[38;5;%hhum", color);
}

static void
color_bg(uint8_t color)
{
	fprintf(stdout, "\x1b[48;5;%hhum", color);
}

static uint16_t
val_new(uint8_t ascii, uint8_t state, uint8_t tsize)
{
	return (BITMASK_TSIZE & (tsize << 10)) | (BITMASK_STATE & (state << 8)) | ascii;
}

static uint8_t
val_get_ascii(uint16_t value)
{
	return value & BITMASK_ASCII;
}

static uint8_t
val_get_state(uint16_t value)
{
	return (value & BITMASK_STATE) >> 8;
}

static uint8_t
val_get_tsize(uint16_t value)
{
	return (value & BITMASK_TSIZE) >> 10;
}

static int
mat_idx(matrix_s *mat, int row, int col)
{
	return row * mat->cols + col;
}

static uint16_t
mat_get_value(matrix_s *mat, int row, int col)
{
	if (row >= mat->rows) return 0;
	if (col >= mat->cols) return 0;
	return mat->data[mat_idx(mat, row, col)];
}

static uint8_t
mat_get_ascii(matrix_s *mat, int row, int col)
{
	return val_get_ascii(mat_get_value(mat, row, col));
}

static uint8_t
mat_get_state(matrix_s *mat, int row, int col)
{
	return val_get_state(mat_get_value(mat, row, col));
}

static uint8_t
mat_get_tsize(matrix_s *mat, int row, int col)
{
	return val_get_tsize(mat_get_value(mat, row, col));
}

static uint8_t
mat_set_value(matrix_s *mat, int row, int col, uint16_t value)
{
	if (row >= mat->rows) return 0;
	if (col >= mat->cols) return 0;
	return mat->data[mat_idx(mat, row, col)] = value;
}

static uint8_t
mat_set_ascii(matrix_s *mat, int row, int col, uint8_t ascii)
{
	uint16_t value = mat_get_value(mat, row, col);
	return mat_set_value(mat, row, col, 
			val_new(ascii, val_get_state(value), val_get_tsize(value)));
}

static uint8_t
mat_set_state(matrix_s *mat, int row, int col, uint8_t state)
{
	uint16_t value = mat_get_value(mat, row, col);
	uint8_t  tsize = state == STATE_NONE ? 0 : val_get_tsize(value);
	return mat_set_value(mat, row, col, 
			val_new(val_get_ascii(value), state, tsize));
}

static uint8_t
mat_set_tsize(matrix_s *mat, int row, int col, uint8_t tsize)
{
	uint16_t value = mat_get_value(mat, row, col);
	return mat_set_value(mat, row, col, 
			val_new(val_get_ascii(value), val_get_state(value), tsize));
}

static void
mat_glitch(matrix_s *mat, float fraction)
{
	int size = mat->rows * mat->cols;
	int num = fraction * size;

	int row = 0;
	int col = 0;

	for (int i = 0; i < num; ++i)
	{
		row = rand() % mat->rows;
		col = rand() % mat->cols;
		mat_set_ascii(mat, row, col, rand_ascii());
	}
}

static void
mat_show(matrix_s *mat)
{
	uint16_t value = 0;
	uint8_t  state = STATE_NONE;
	uint8_t  tsize = 0;
	
	for (int r = 0; r < mat->rows; ++r)
	{
		for (int c = 0; c < mat->cols; ++c)
		{
			value = mat_get_value(mat, r, c);
			state = val_get_state(value);
			tsize = val_get_tsize(value);
			uint8_t color = greens[tsize];

			switch (state)
			{
				case STATE_NONE:
					fprintf(stdout, " ");
					break;
				case STATE_DROP:
					color_fg(COLOR_FG_WHITE1);
					fprintf(stdout, "%c", val_get_ascii(value));
					break;
				case STATE_TAIL:
					color_fg(color);
					fprintf(stdout, "%c", val_get_ascii(value));
					break;
						
			}
		}
		fprintf(stdout, "\n");
	}
	// Depending on what type of buffering we use, flushing might be needed
	fflush(stdout);
}

static void
mat_debug(matrix_s *mat, int what)
{
	uint16_t value = 0;
	for (int r = 0; r < mat->rows; ++r)
	{
		for (int c = 0; c < mat->cols; ++c)
		{
			value = mat_get_value(mat, r, c);
			if (what == DEBUG_STATE)
			{
				fprintf(stdout, "%hhu", val_get_state(value));
			}
			else if (what == DEBUG_ASCII)
			{
				fprintf(stdout, "%c", val_get_ascii(value));
			}
			else if (what == DEBUG_TSIZE)
			{
				fprintf(stdout, "%hhu", val_get_tsize(value));
			}
		}
		fprintf(stdout, "\n");
	}
}

static void
new_drop(matrix_s *mat, int max_tries)
{
	// TODO this could turn into an endless loop if there isn't any space
	//      so we need to make sure the tail lengths are short enough in
	//      relation to the terminal height to _always_ allow for a new
	//      drop to be placed _somewhere_ at the top at least. Even then 
	//      this isn't exactly an elegant approach... but oh well.

	int maxlen = TSIZE_MAX >= mat->rows ? mat->rows - 1 : TSIZE_MAX;

	while (max_tries--)
	{
		int c = rand_int(0, mat->cols - 1);
		if (mat_get_state(mat, 0, c) == STATE_NONE && mat_get_state(mat, 1, c) == STATE_NONE)
		{
			mat_set_state(mat, 0, c, STATE_DROP);
			mat_set_tsize(mat, 0, c, rand_int(TSIZE_MIN, maxlen));
			break;
		}
	}
}

static void
mat_drop(matrix_s *mat, float ratio)
{
	int total = mat->cols * mat->rows;
	int drops = (int) ((float) total * ratio);

	int maxlen = TSIZE_MAX >= mat->rows ? mat->rows - 1 : TSIZE_MAX;

	for (int d = 0; d < drops; ++d)
	{
		int r = rand_int(0, mat->rows - 1);
		int c = rand_int(0, mat->cols - 1);
		mat_set_state(mat, r, c, STATE_DROP);
		mat_set_tsize(mat, r, c, rand_int(TSIZE_MIN, maxlen));
	}
}

static void
col_trace(matrix_s *mat, int col, int row, int tsize)
{
	int top = row - tsize > 0 ? row - tsize : 0;
	float intensity = 1;
	int color = 0;
	for (int i = 0; row >= top; --row, ++i)
	{
		if (row == top)
		{
			mat_set_state(mat, row, col, STATE_NONE);
		}
		else
		{	
			intensity = ((float)i / (float)tsize);
			color = 4 * intensity;
			mat_set_state(mat, row, col, STATE_TAIL);
			//mat_set_tsize(mat, row, col, tsize - i);
			mat_set_tsize(mat, row, col, color);
		}
	}
}

static void
col_clean(matrix_s *mat, int col)
{
	int state = STATE_NONE;
	for (int row = mat->rows - 1; row >= 0; --row)
	{
		state = mat_get_state(mat, row, col);
		if (state == STATE_NONE)
		{
			mat_set_state(mat, row+1, col, STATE_NONE);
			break;
		}
		else
		{
			mat_set_tsize(mat, row+1, col, mat_get_tsize(mat, row, col));
		}
	}
}

static void
mat_tick(matrix_s *mat)
{
	uint8_t state = STATE_NONE;
	uint8_t tsize = 0;

	// find the drops
	for (int c = 0; c < mat->cols; ++c)
	{
		for (int r = mat->rows - 1; r >= 0; --r)
		{
			state = mat_get_state(mat, r, c);
			tsize = mat_get_tsize(mat, r, c);

			if (state == STATE_DROP)
			{
				if (r == mat->rows - 1) // bottom row)
				{
					mat_set_state(mat, r, c, STATE_TAIL);
					new_drop(mat, mat->cols);
					col_trace(mat, c, r, tsize);
					col_clean(mat, c);
				}
				else
				{
					mat_set_state(mat, r,   c, STATE_NONE);
					mat_set_state(mat, r+1, c, STATE_DROP);
					mat_set_tsize(mat, r+1, c, tsize);
					col_trace(mat, c, r, tsize);
				}
			}
			else if (state == STATE_TAIL)
			{
				if (r == mat->rows - 1) // bottom row
				{
					col_clean(mat, c);
				}

			}
		}
	}
}

static void
mat_fill(matrix_s *mat, uint8_t state)
{
	for (int r = 0; r < mat->rows; ++r)
	{
		for (int c = 0; c < mat->cols; ++c)
		{
			mat_set_state(mat, r, c, state);
			mat_set_ascii(mat, r, c, rand_ascii());
		}
	}
}

/*
 * Creates or recreates (resizes) the given matrix.
 * Returns -1 on error (out of memory), 0 on success.
 */
static int
mat_init(matrix_s *mat, int rows, int cols)
{
	mat->data = realloc(mat->data, sizeof(uint16_t) * rows * cols);
	if (mat->data)
	{
		mat->rows = rows;
		mat->cols = cols;
		return 0;
	}
	return -1;
}

void
cli_clear(int rows)
{
	printf("\033[%dA", rows); // cursor up 
	//printf("\033[2J"); // clear screen
	//printf("\033[H");  // cursor back to top, left
	//printf("\033[%dT", rows); // scroll down
	//printf("\033[%dN", rows); // scroll up
}

int
main(int argc, char **argv)
{
	// https://youtu.be/MvEXkd3O2ow?t=26
	// https://matrix.logic-wire.de/

	// https://man7.org/linux/man-pages/man4/tty_ioctl.4.html
	// https://en.wikipedia.org/wiki/ANSI_escape_code
	// https://gist.github.com/XVilka/8346728
	// https://stackoverflow.com/a/33206814/3316645 
	// https://jdebp.eu/FGA/clearing-the-tui-screen.html#POSIX
	
	struct sigaction sa = { .sa_handler = &on_signal };
	sigaction(SIGKILL,  &sa, NULL);
	sigaction(SIGQUIT,  &sa, NULL);
	sigaction(SIGTERM,  &sa, NULL);
	sigaction(SIGWINCH, &sa, NULL);

	struct winsize ws = { 0 };
	ioctl(0, TIOCGWINSZ, &ws);

	//struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };

	//setlinebuf(stdout);
	setvbuf(stdout, NULL, _IOFBF, 0);

	matrix_s mat = { 0 }; 
	mat_init(&mat, ws.ws_row, ws.ws_col);
	mat_fill(&mat, STATE_NONE);
	mat_drop(&mat, DROP_RATIO);

	fprintf(stdout, ANSI_HIDE_CURSOR);
	fprintf(stdout, ANSI_FONT_BOLD);
	color_fg(COLOR_FG_GREEN1);

	uintmax_t tick = 0;

	running = 1;
	while(running)
	{
		if (resized)
		{
			ioctl(0, TIOCGWINSZ, &ws);
			mat_init(&mat, ws.ws_row, ws.ws_col);
			mat_fill(&mat, STATE_NONE);
			mat_drop(&mat, DROP_RATIO);
			resized = 0;
		}

		mat_glitch(&mat, GLITCH_RATIO);
		mat_tick(&mat);
		mat_show(&mat);
		//mat_debug(&mat, DEBUG_TSIZE);

		//cli_clear(ws.ws_row);

		++tick;
		nanosleep(&ts, NULL);
	}

	free(mat.data);
	fprintf(stdout, ANSI_FONT_RESET);
	fprintf(stdout, ANSI_SHOW_CURSOR);
	return EXIT_SUCCESS;
}
