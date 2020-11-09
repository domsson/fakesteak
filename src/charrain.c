#include <stdio.h>      // fprintf(), stdout, setlinebuf()
#include <stdlib.h>     // EXIT_SUCCESS, EXIT_FAILURE, rand()
#include <stdint.h>     // uintmax_t
#include <time.h>       // nanosleep(), struct timespec
#include <signal.h>     // sigaction(), struct sigaction
#include <math.h>       // sin()
#include <termios.h>    // struct winsize 
#include <sys/ioctl.h>  // ioctl(), TIOCGWINSZ

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define ANSI_HIDE_CURSOR   "\e[?25l"
#define ANSI_SHOW_CURSOR   "\e[?25h"

static volatile int resize;
static volatile int running;   // controls running of the main loop 
static volatile int handled;   // last signal that has been handled 

#define BITMASK_ASCII 0x00FF
#define BITMASK_STATE 0x0300
#define BITMASK_SIZE  0xFC00

#define STATE_NONE 0
#define STATE_DROP 1
#define STATE_TAIL 2

#define DEBUG_STATE 1
#define DEBUG_ASCII 2

#define MAX_DROP_LENGTH 252

#define GLITCH_RATIO 0.02
#define DROP_RATIO   0.01
#define DROP_LENGTH  20

//  128 64  32  16   8   4   2   1  128 64  32  16   8   4   2   1
//   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
//   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0
//   '---------- STATE ----------'   '---------- ASCII ----------'
//   '------ SIZE -------'   '---'

struct matrix
{
	uint16_t *data;
	int cols;
	int rows;
};

typedef struct matrix matrix_s;

static void
on_signal(int sig)
{
	switch (sig)
	{
		case SIGWINCH:
			resize = 1;
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

static uint16_t
get_value(uint8_t ascii, uint8_t state, uint8_t size)
{
	return (BITMASK_SIZE & (size << 10)) | (BITMASK_STATE & (state << 8)) | ascii;
}

static uint8_t
get_ascii(uint16_t value)
{
	return value & BITMASK_ASCII;
}

static uint8_t
get_state(uint16_t value)
{
	return (value & BITMASK_STATE) >> 8;
}

static uint8_t
get_size(uint16_t value)
{
	return (value & BITMASK_SIZE) >> 12;
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
	return get_ascii(mat_get_value(mat, row, col));
}

static uint8_t
mat_get_state(matrix_s *mat, int row, int col)
{
	return get_state(mat_get_value(mat, row, col));
}

static uint8_t
mat_get_size(matrix_s *mat, int row, int col)
{
	return get_size(mat_get_value(mat, row, col));
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
	uint8_t state = mat_get_state(mat, row, col);
	uint8_t size  = mat_get_size(mat, row, col);
	return mat_set_value(mat, row, col, get_value(ascii, state, size));
}

static uint8_t
mat_set_state(matrix_s *mat, int row, int col, uint8_t state)
{
	uint8_t ascii = mat_get_ascii(mat, row, col);
	uint8_t size  = mat_get_size(mat, row, col);
	return mat_set_value(mat, row, col, get_value(ascii, state, size));
}

static uint8_t
mat_set_size(matrix_s *mat, int row, int col, uint8_t size)
{
	uint8_t ascii = mat_get_ascii(mat, row, col);
	uint8_t state = mat_get_state(mat, row, col);
	return mat_set_value(mat, row, col, get_value(ascii, state, size));
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

static void
mat_glitch(matrix_s *mat, double fraction)
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

	for (int r = 0; r < mat->rows; ++r)
	{
		for (int c = 0; c < mat->cols; ++c)
		{
			value = mat_get_value(mat, r, c);
			state = get_state(value);

			switch (state)
			{
				case STATE_NONE:
					fprintf(stdout, " ");
					break;
				case STATE_DROP:
					fprintf(stdout, ANSI_COLOR_RESET);
					fprintf(stdout, "%c", get_ascii(value));
					fprintf(stdout, ANSI_COLOR_GREEN);
					break;
				case STATE_TAIL:
					fprintf(stdout, "%c", get_ascii(value));
					break;
						
			}
			//fprintf(stdout, "%c", get_state(value) ? get_ascii(value) : ' ');
			//fprintf(stdout, "%c", mat_get_state(mat, r, c) ? mat_get_ascii(mat, r, c) : ' ');
		}
		fprintf(stdout, "\n");
	}
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
				fprintf(stdout, "%hhu", get_state(value));
			}
			else
			{
				fprintf(stdout, "%c", get_ascii(value));
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
	while (max_tries--)
	{
		int c = rand_int(0, mat->cols - 1);
		if (mat_get_state(mat, 0, c) == STATE_NONE && mat_get_state(mat, 1, c) == STATE_NONE)
		{
			mat_set_state(mat, 0, c, STATE_DROP);
			break;
		}
	}
}

static void
mat_drop(matrix_s *mat, float ratio)
{
	int total = mat->cols * mat->rows;
	int drops = (int) ((float) total * ratio);

	for (int d = 0; d < drops; ++d)
	{
		int r = rand_int(0, mat->rows - 1);
		int c = rand_int(0, mat->cols - 1);
		mat_set_state(mat, r, c, STATE_DROP);
	}
}

static void
col_trace(matrix_s *mat, int col, int row, int length)
{
	int top = row - length > 0 ? row - length : 0;
	for (; row >= top; --row)
	{
		mat_set_state(mat, row, col, row == top ? STATE_NONE : STATE_TAIL);
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
	}
}

static void
mat_tick(matrix_s *mat)
{
	// find the drops
	int state = STATE_NONE;
	for (int c = 0; c < mat->cols; ++c)
	{
		for (int r = mat->rows - 1; r >= 0; --r)
		{
			state = mat_get_state(mat, r, c);
			if (r == mat->rows - 1 && state == STATE_TAIL)
			{
				col_clean(mat, c);
			}
			else if (r == mat->rows - 1 && state == STATE_DROP)
			{
				mat_set_state(mat, r, c, STATE_TAIL);
				//mat_set_state(mat, 0, c, STATE_DROP);
				new_drop(mat, mat->cols);
			}
			else if (state == STATE_DROP)
			{
				mat_set_state(mat, r,   c, STATE_NONE);
				mat_set_state(mat, r+1, c, STATE_DROP);
				col_trace(mat, c, r, DROP_LENGTH);
			}
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

int
main(int argc, char **argv)
{
	// https://youtu.be/MvEXkd3O2ow?t=26
	// https://matrix.logic-wire.de/

	// https://man7.org/linux/man-pages/man4/tty_ioctl.4.html
	// https://en.wikipedia.org/wiki/ANSI_escape_code
	// https://gist.github.com/XVilka/8346728
	// https://stackoverflow.com/a/33206814/3316645 
	
	struct sigaction sa = { .sa_handler = &on_signal };
	sigaction(SIGKILL,  &sa, NULL);
	sigaction(SIGQUIT,  &sa, NULL);
	sigaction(SIGTERM,  &sa, NULL);
	sigaction(SIGWINCH, &sa, NULL);

	struct winsize ws = { 0 };
	ioctl(0, TIOCGWINSZ, &ws);

	//struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };

	setlinebuf(stdout);

	matrix_s mat = { 0 }; 
	mat_init(&mat, ws.ws_row, ws.ws_col);
	mat_fill(&mat, STATE_NONE);
	mat_drop(&mat, DROP_RATIO);

	fprintf(stdout, ANSI_HIDE_CURSOR);
	fprintf(stdout, ANSI_COLOR_GREEN);

	uintmax_t tick = 0;
	int state = 0;

	running = 1;
	while(running)
	{
		if (resize)
		{
			ioctl(0, TIOCGWINSZ, &ws);
			mat_init(&mat, ws.ws_row, ws.ws_col);
			mat_fill(&mat, STATE_NONE);
			mat_drop(&mat, DROP_RATIO);
			resize = 0;
		}

		mat_glitch(&mat, GLITCH_RATIO);
		mat_tick(&mat);
		mat_show(&mat);
		//mat_debug(&mat, DEBUG_STATE);

		printf("\033[%dA", ws.ws_row); // move up ws.ws_row lines (back to start)

		++tick;

		nanosleep(&ts, NULL);
	}

	fprintf(stdout, ANSI_COLOR_RESET);
	fprintf(stdout, ANSI_SHOW_CURSOR);
	return EXIT_SUCCESS;
}
