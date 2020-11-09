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

#define BITMASK_STATE 0x80
#define BITMASK_ASCII 0x7F 

#define STATE_ON  1
#define STATE_OFF 0

typedef unsigned char byte;

struct matrix
{
	byte *data;
	int   cols;
	int   rows;
};

typedef struct matrix matrix_s;

struct drop
{
	int col;
	int row;
	int length;
};

typedef struct drop drop_s;

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

static byte
rand_ascii()
{
	return rand_int_mincap(32, 126);
}

static byte
get_value(byte ascii, byte state)
{
	byte value = (ascii & BITMASK_ASCII);
	return state ? value | BITMASK_STATE : value;
}

static byte
get_ascii(byte value)
{
	return value & BITMASK_ASCII;
}

static byte
get_state(byte value)
{
	return value & BITMASK_STATE;
}

static int
mat_idx(matrix_s *mat, int row, int col)
{
	return row * mat->cols + col;
}

static byte
mat_get_value(matrix_s *mat, int row, int col)
{
	if (row >= mat->rows) return 0;
	if (col >= mat->cols) return 0;
	return mat->data[mat_idx(mat, row, col)];
}

static byte
mat_get_ascii(matrix_s *mat, int row, int col)
{
	return get_ascii(mat_get_value(mat, row, col));
}

static byte
mat_get_state(matrix_s *mat, int row, int col)
{
	return get_state(mat_get_value(mat, row, col));
}

static byte
mat_set_value(matrix_s *mat, int row, int col, byte val)
{
	if (row >= mat->rows) return 0;
	if (col >= mat->cols) return 0;
	return mat->data[mat_idx(mat, row, col)] = val;
}

static byte
mat_set_ascii(matrix_s *mat, int row, int col, byte ascii)
{
	byte value = mat_get_value(mat, row, col);
	byte state = get_state(value);
	return mat_set_value(mat, row, col, get_value(ascii, state));
}

static byte
mat_set_state(matrix_s *mat, int row, int col, byte state)
{
	byte value = mat_get_value(mat, row, col);
	byte ascii = get_ascii(value);
	return mat_set_value(mat, row, col, get_value(ascii, state));
}

static void
mat_fill(matrix_s *mat, byte state)
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
	byte value = 0;

	for (int r = 0; r < mat->rows; ++r)
	{
		for (int c = 0; c < mat->cols; ++c)
		{
			value = mat_get_value(mat, r, c);
			fprintf(stdout, "%c", get_state(value) ? get_ascii(value) : ' ');
		}
		fprintf(stdout, "\n");
	}
}

static void
mat_tick(matrix_s *mat, drop_s *drops, size_t num_drops)
{
	int distance = 0;
	for (size_t d = 0; d < num_drops; ++d)
	{
		for (int r = 0; r < mat->rows; ++r)
		{
			distance = abs(drops[d].row - r);
			mat_set_state(mat, r, drops[d].col, distance < drops[d].length);
		}
		drops[d].row = (drops[d].row + 1 >= mat->rows) ? 0 : drops[d].row + 1;
	}
}

/*
 * Creates or recreates (resizes) the given matrix.
 * Returns -1 on error (out of memory), 0 on success.
 */
static int
mat_init(matrix_s *mat, int rows, int cols)
{
	mat->data = realloc(mat->data, sizeof(char) * rows * cols);
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
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 250000000 };

	setlinebuf(stdout);

	matrix_s mat = { 0 }; 
	mat_init(&mat, ws.ws_row, ws.ws_col);
	mat_fill(&mat, STATE_OFF);

	fprintf(stdout, ANSI_HIDE_CURSOR);
	fprintf(stdout, ANSI_COLOR_GREEN);

	uintmax_t tick = 0;
	int state = 0;

	// create rain drops
	int num_drops = 100;
	drop_s *drops = malloc(sizeof(drop_s) * num_drops);
	if (drops == NULL) return EXIT_FAILURE;

	for (int i = 0; i < num_drops; ++i)
	{
		int r = rand_int(0, mat.rows - 1);
		int c = rand_int(0, mat.cols - 1);
		drops[i].row = r;
		drops[i].col = c;
		drops[i].length = rand_int(0, mat.rows - 1);
	}

	running = 1;
	while(running)
	{
		if (resize)
		{
			ioctl(0, TIOCGWINSZ, &ws);
			mat_init(&mat, ws.ws_row, ws.ws_col);
			mat_fill(&mat, STATE_ON);
			resize = 0;
		}

		/*
		for (int row = 0; row < mat.rows; ++row)
		{
			for (int col = 0; col < mat.cols; ++col)
			{
			}
		}
		*/

		mat_glitch(&mat, 0.01);
		mat_tick(&mat, drops, num_drops);
		mat_show(&mat);

		printf("\033[%dA", ws.ws_row); // move up ws.ws_row lines (back to start)

		++tick;

		nanosleep(&ts, NULL);
	}

	fprintf(stdout, ANSI_COLOR_RESET);
	fprintf(stdout, ANSI_SHOW_CURSOR);
	return EXIT_SUCCESS;
}
