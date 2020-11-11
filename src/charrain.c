#include <stdio.h>      // fprintf(), stdout, setlinebuf()
#include <stdlib.h>     // EXIT_SUCCESS, EXIT_FAILURE, rand()
#include <stdint.h>     // uint8_t, uint16_t
#include <time.h>       // time(), nanosleep(), struct timespec
#include <signal.h>     // sigaction(), struct sigaction
#include <termios.h>    // struct winsize 
#include <sys/ioctl.h>  // ioctl(), TIOCGWINSZ

#define ANSI_FONT_RESET "\x1b[0m"
#define ANSI_FONT_BOLD  "\x1b[1m"
#define ANSI_FONT_FAINT "\x1b[2m"

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

static volatile int resized;   // window resize event received
static volatile int running;   // controls running of the main loop 
static volatile int handled;   // last signal that has been handled 

// https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit
enum colors {
	COLOR_FG_WHITE1 = 158,
	COLOR_FG_GREEN1 = 48,
	COLOR_FG_GREEN2 = 41,
	COLOR_FG_GREEN3 = 35,
	COLOR_FG_GREEN4 = 29,
	COLOR_FG_GREEN5 = 23
};

uint8_t colors[] = { 158, 48, 41, 35, 29, 238 };

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
		case SIGINT:
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

//
// Functions to manipulate individual matrix cell values
//

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

//
// Functions to access / set matrix values
//

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

//
// Functions to create, manipulate and print a matrix
//

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
mat_print(matrix_s *mat)
{
	uint16_t value = 0;
	uint8_t  state = STATE_NONE;
	uint8_t  tsize = 0;
	uint8_t  color = 0;
	uint8_t  last  = 0;

	for (int r = 0; r < mat->rows; ++r)
	{
		last = r == mat->rows - 1;
		for (int c = 0; c < mat->cols; ++c)
		{
			value = mat_get_value(mat, r, c);
			state = val_get_state(value);
			tsize = val_get_tsize(value);
			color = state == STATE_DROP ? 0 : colors[tsize];

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
		if (!last)
		{
			fprintf(stdout, "\n");
		}
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
mat_drop(matrix_s *mat, int max_tries)
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
mat_rain(matrix_s *mat, float ratio)
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

/*
 * Works its way up from a drop, adding TAIL cells and coloring them.
 *
 * col  : the column to work on
 * row  : row to start in (should be the first tail cell above the drop)
 * tsize: tail length (should have been extracted from the drop)
 */
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
			color = 5 * intensity; // TODO hardcoded, bad
			mat_set_state(mat, row, col, STATE_TAIL);
			mat_set_tsize(mat, row, col, color + 1);
		}
	}
}

/*
 * Works its way up from the bottom of a column, setting the color of each 
 * TAIL cell to that of the TAIL cell above and deletes the top-most TAIL 
 * cell of that bottom-most continuous trace.
 */
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
mat_update(matrix_s *mat)
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
					mat_drop(mat, mat->cols);
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
mat_free(matrix_s *mat)
{
	free(mat->data);
}

void
cli_clear(int rows)
{
	//printf("\033[%dA", rows); // cursor up 
	//printf("\033[2J"); // clear screen
	//printf("\033[H");  // cursor back to top, left
	//printf("\033[%dT", rows); // scroll down
	//printf("\033[%dN", rows); // scroll up
}

void
cli_setup()
{
	fprintf(stdout, ANSI_HIDE_CURSOR);
	fprintf(stdout, ANSI_FONT_BOLD);
	printf("\033[2J"); // clear screen
	printf("\033[H");  // cursor back to top, left
	//printf("\n");
}

void
cli_reset()
{
	fprintf(stdout, ANSI_FONT_RESET);
	fprintf(stdout, ANSI_SHOW_CURSOR);
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
	
	// set signal handlers for the usual susspects plus window resize
	struct sigaction sa = { .sa_handler = &on_signal };
	sigaction(SIGINT,   &sa, NULL);
	sigaction(SIGKILL,  &sa, NULL);
	sigaction(SIGQUIT,  &sa, NULL);
	sigaction(SIGTERM,  &sa, NULL);
	sigaction(SIGWINCH, &sa, NULL);

	// get the terminal dimensions, maybe (this ain't portable)
	struct winsize ws = { 0 };
	ioctl(0, TIOCGWINSZ, &ws);

	// seed the random number generator with the current unix time
	srand(time(NULL));

	// this will determine the speed of the entire thing
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };

	// set the buffering to fully buffered, we're adult and flush ourselves
	setvbuf(stdout, NULL, _IOFBF, 0);

	// initialize the matrix
	matrix_s mat = { 0 }; 
	mat_init(&mat, ws.ws_row, ws.ws_col);
	mat_fill(&mat, STATE_NONE);
	mat_rain(&mat, DROP_RATIO);

	// prepare the terminal for our shenanigans
	cli_setup();

	running = 1;
	while(running)
	{
		if (resized)
		{
			// get the new terminal dimensions
			ioctl(0, TIOCGWINSZ, &ws);
			// reinitialize the matrix
			mat_init(&mat, ws.ws_row, ws.ws_col);
			mat_fill(&mat, STATE_NONE);
			mat_rain(&mat, DROP_RATIO);
			resized = 0;
		}

		mat_glitch(&mat, GLITCH_RATIO); // apply random defects
		mat_update(&mat);               // move all drops down one row
		cli_clear(ws.ws_row);
		mat_print(&mat);                // print to the terminal
		//mat_debug(&mat, DEBUG_TSIZE);
		nanosleep(&ts, NULL);           // zzZzZZzz
	}

	// make sure all is back to normal before we exit
	mat_free(&mat);	
	cli_reset();
	return EXIT_SUCCESS;
}
