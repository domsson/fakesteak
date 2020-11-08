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

void on_signal(int sig)
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

void get_size()
{
	struct winsize ws = { 0 };
	ioctl(0, TIOCGWINSZ, &ws);

	fprintf(stdout, "size: %d x %d characters\n", ws.ws_col, ws.ws_row);
}

byte rand_ascii()
{
	int r = rand() % 126;
	//return r < 32 ? r + 32 : r;
	return r < 32 ? 32 : r;

}

byte get_value(byte ascii, byte state)
{
	byte value = (ascii & BITMASK_ASCII);
	return state ? value | BITMASK_STATE : value;
}

byte get_ascii(byte value)
{
	return value & BITMASK_ASCII;
}

byte get_state(byte value)
{
	return value & BITMASK_STATE;
}

int mat_idx(matrix_s *mat, int row, int col)
{
	return row * mat->cols + col;
}

byte mat_get_value(matrix_s *mat, int row, int col)
{
	return mat->data[mat_idx(mat, row, col)];
}

byte mat_get_ascii(matrix_s *mat, int row, int col)
{
	return get_ascii(mat_get_value(mat, row, col));
}

byte mat_get_state(matrix_s *mat, int row, int col)
{
	return get_state(mat_get_value(mat, row, col));
}

byte mat_set_value(matrix_s *mat, int row, int col, byte val)
{
	return mat->data[mat_idx(mat, row, col)] = val;
}

byte mat_set_ascii(matrix_s *mat, int row, int col, byte ascii)
{
	byte value = mat_get_value(mat, row, col);
	byte state = get_state(value);
	return mat_set_value(mat, row, col, get_value(ascii, state));
}

byte mat_set_state(matrix_s *mat, int row, int col, byte state)
{
	byte value = mat_get_value(mat, row, col);
	byte ascii = get_ascii(value);
	return mat_set_value(mat, row, col, get_value(ascii, state));
}

void mat_fill(matrix_s *mat)
{
	for (int r = 0; r < mat->rows; ++r)
	{
		for (int c = 0; c < mat->cols; ++c)
		{
			mat_set_state(mat, r, c, 0);
			mat_set_ascii(mat, r, c, rand_ascii());
		}
	}
}

void mat_show(matrix_s *mat)
{
	for (int r = 0; r < mat->rows; ++r)
	{
		for (int c = 0; c < mat->cols; ++c)
		{
			fprintf(stdout, "%c", mat_get_ascii(mat, r, c));
		}
		fprintf(stdout, "\n");
	}
}

/*
 * Creates or recreates (resizes) the given matrix.
 * Returns -1 on error (out of memory), 0 on success.
 */
int mat_init(matrix_s *mat, int rows, int cols)
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

int main(int argc, char **argv)
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
	mat_fill(&mat);

	fprintf(stdout, ANSI_HIDE_CURSOR);
	fprintf(stdout, ANSI_COLOR_GREEN);

	int line = 0;
	running = 1;
	while(running)
	{
		if (resize)
		{
			ioctl(0, TIOCGWINSZ, &ws);
			resize = 0;
			/*
			printf("\033[2J");             // clear screen
			printf("\033[%dA", ws.ws_row); // move up ws.ws_row lines
			*/
			mat_init(&mat, ws.ws_row, ws.ws_col);
			mat_fill(&mat);
		}

		/*
		for (int i = 0; i < (ws.ws_col - 1); ++i)
		{
			if (sin(i) > 0.5)
			{
				fprintf(stdout, "%c", rand_ascii());
				//fprintf(stdout, "sin(%3d) = %f\n", i, sin(i));
			}
			else
			{
				fprintf(stdout, " ");
			}
		}
		fprintf(stdout, "\n");
		*/
		mat_show(&mat);
		printf("\033[%dA", ws.ws_row); // move up ws.ws_row lines (back to start)

		line = (line + 1) % ws.ws_row;

		//sleep(1);
		nanosleep(&ts, NULL);
	}

	fprintf(stdout, ANSI_COLOR_RESET);
	fprintf(stdout, ANSI_SHOW_CURSOR);
	return EXIT_SUCCESS;
}
