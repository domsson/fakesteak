#include <stdio.h>      // fprintf(), stdout, setlinebuf()
#include <stdlib.h>     // EXIT_SUCCESS, EXIT_FAILURE, rand()
//#include <unistd.h>     // sleep()
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

typedef unsigned char byte;

struct matrix
{
	byte *data;
	int   cols;
	int   rows;
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

static void
get_size()
{
	struct winsize ws = { 0 };
	ioctl(0, TIOCGWINSZ, &ws);

	fprintf(stdout, "size: %d x %d characters\n", ws.ws_col, ws.ws_row);
}

static byte
rand_ascii()
{
	int r = rand() % 126;
	//return r < 32 ? r + 32 : r;
	return r < 32 ? 32 : r;

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

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };

	setlinebuf(stdout);

	matrix_s mat = { 0 }; 
	mat_init(&mat, ws.ws_row, ws.ws_col);
	mat_fill(&mat, 1);

	fprintf(stdout, ANSI_HIDE_CURSOR);
	fprintf(stdout, ANSI_COLOR_GREEN);

	int i = 0;
	int row = 0;
	int state = 0;

	running = 1;
	while(running)
	{
		if (resize)
		{
			ioctl(0, TIOCGWINSZ, &ws);
			resize = 0;
			mat_init(&mat, ws.ws_row, ws.ws_col);
			mat_fill(&mat, 1);
		}

		/*
		for (int col = 0; col < (ws.ws_col - 1); ++col)
		{
			state = sin(col) > 0.5 && cos(i * row) > 0.3;
			mat_set_state(&mat, row, col, state);
		}
		*/

		mat_glitch(&mat, 0.01);
		mat_show(&mat);
		printf("\033[%dA", ws.ws_row); // move up ws.ws_row lines (back to start)

		row = (row + 1) % ws.ws_row;
		++i;

		nanosleep(&ts, NULL);
	}

	fprintf(stdout, ANSI_COLOR_RESET);
	fprintf(stdout, ANSI_SHOW_CURSOR);
	return EXIT_SUCCESS;
}
