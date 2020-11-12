#include <stdio.h>      // fprintf(), stdout, setlinebuf()
#include <stdlib.h>     // EXIT_SUCCESS, EXIT_FAILURE, rand()
#include <stdint.h>     // uint8_t, uint16_t
#include <math.h>       // ceil()
#include <time.h>       // time(), nanosleep(), struct timespec
#include <string.h>     // memmove()
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
#define TSIZE_MAX 63

//#define TSIZE_MIN 2
//#define TSIZE_MAX 8

#define GLITCH_RATIO 0.02
#define DROP_RATIO   0.02

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
	size_t drop_count;
	float  drop_ratio;
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
					fputc(' ', stdout);
					break;
				case STATE_DROP:
					color_fg(COLOR_FG_WHITE1);
					fputc(val_get_ascii(value), stdout);
					break;
				case STATE_TAIL:
					color_fg(color);
					fputc(val_get_ascii(value), stdout);
					break;
			}
		}
		if (!last)
		{
			fputc('\n', stdout);
		}
	}
	// Depending on what type of buffering we use, flushing might be needed
	fflush(stdout);
}

static void
mat_debug(matrix_s *mat, int what)
{
	fprintf(stdout, "\x1b[0m");

	uint16_t value = 0;
	uint8_t  last  = 0;

	for (int r = 0; r < mat->rows; ++r)
	{
		last = r == mat->rows - 1;
		for (int c = 0; c < mat->cols; ++c)
		{
			value = mat_get_value(mat, r, c);
			switch (what)
			{
				case DEBUG_STATE:
					fprintf(stdout, "%hhu", val_get_state(value));
					break;
				case DEBUG_ASCII:
					fprintf(stdout, "%c",   val_get_ascii(value));
					break;
				case DEBUG_TSIZE:
					fprintf(stdout, "%hhu", val_get_tsize(value));
					break;
			}
		}
		if (!last)
		{
			fprintf(stdout, "\n");
		}
	}
	fflush(stdout);
}

/*
 * Turn the specified cell into a DROP cell.
 */
static void
mat_put_cell_drop(matrix_s *mat, int row, int col, int tsize)
{
	mat_set_state(mat, row, col, STATE_DROP);
	mat_set_tsize(mat, row, col, tsize);
}

/*
 * Turn the specified cell into a TAIL cell.
 */
static void
mat_put_cell_tail(matrix_s *mat, int row, int col, int tsize, int tnext)
{
	float intensity = (float) tnext / (float) tsize; // 1 for end of trace, 0.x for beginning
	int color = ceil(5 * intensity); // TODO hardcoded, bad
	mat_set_state(mat, row, col, STATE_TAIL);
//	mat_set_tsize(mat, row, col, tnext); // TODO color index instead!
	mat_set_tsize(mat, row, col, color);
}

/*
 * Add a DROP, including its TAIL cells, to the matrix, 
 * starting from the specified position.
 */
static void
mat_add_drop(matrix_s *mat, int row, int col, int tsize)
{
	for (int i = 0; i <= tsize; ++i, --row)
	{
		if (row < 0)          break;
		if (row >= mat->rows) continue;

		if (i == 0)
		{
			mat_put_cell_drop(mat, row, col, tsize);
			++mat->drop_count;
		}
		else
		{
			mat_put_cell_tail(mat, row, col, tsize, i);
		}
	}
}

/*
 * Make it rain by adding some DROPs to the matrix.
 */
static void
mat_rain(matrix_s *mat, float ratio)
{
	int num = (int) (mat->cols * mat->rows) * ratio;

	int c = 0;
	int r = 0;

	for (int i = 0; i < num; ++i)
	{
		c = rand_int(0, mat->cols - 1);
		r = rand_int(0, mat->rows - 1);
		mat_add_drop(mat, r, c, rand_int(TSIZE_MIN, TSIZE_MAX));
	}
}

/*
 * Move every cell down one row, potentially adding a new tail cell at the top.
 * Returns 1 if a DROP 'fell off the bottom', otherwise 0.
 */
static int
mat_mov_col(matrix_s *mat, int col)
{
	uint8_t tail_size = 0;
	uint8_t tail_seen = 0;

	uint16_t value = 0;
	uint8_t  state = STATE_NONE;
	uint8_t  tsize = 0;

	// manually check the bottom-most cell: is it a DROP?
	int dropped = mat_get_state(mat, mat->rows - 1, col) == STATE_DROP;

	// iterate all cells in this column, moving each down one cell
	for (int row = mat->rows - 1; row >= 0; --row)
	{
		// get the current cell's meta data
		value = mat_get_value(mat, row, col);
		state = val_get_state(value);
		tsize = val_get_tsize(value);

		// nothing to do if this cell is neither DROP nor TAIL
		if (state == STATE_NONE)
		{
			continue;
		}

		// move the cell one down
		mat_set_state(mat, row+1, col, state);
		mat_set_tsize(mat, row+1, col, tsize);

		// null the current cell
		mat_set_state(mat, row, col, STATE_NONE);
		mat_set_tsize(mat, row, col, 0);

		// keep track of the tail length of the last seen drop
		if (state == STATE_DROP)
		{
			// remember the tail size to draw for this drop
			tail_size = tsize;
			tail_seen = 0;
		}
		else if (state == STATE_TAIL && tail_size > 0)
		{
			// keep track of how many tail cells we've seen
			++tail_seen;
		}
	}

	// if the top-most cell wasn't empty, we might have to add a tail cell
	if (state != STATE_NONE && tail_seen < tail_size)
	{
		mat_put_cell_tail(mat, 0, col, tail_size, tail_seen + 1); 
	}

	return dropped;
}

static void 
mat_update(matrix_s *mat)
{
	// add new drops at the top, trying to get to the desired drop count

	size_t drops_desired = (mat->cols * mat->rows) * mat->drop_ratio;
	size_t drops_missing = drops_desired - mat->drop_count; 
	size_t drops_to_add  = (size_t) ((float) drops_missing / (float) mat->rows);

	int col = 0;

	for (size_t i = 0; i <= drops_to_add; ++i)
	{
		col = rand_int(0, mat->cols - 1);
		
		mat_add_drop(mat, 0, col, rand_int(TSIZE_MIN, TSIZE_MAX));
	}

	// move each column down one cell, possibly dropping some drops
	for (int col = 0; col < mat->cols; ++col)
	{
		mat->drop_count -= mat_mov_col(mat, col);
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
mat_init(matrix_s *mat, int rows, int cols, float drop_ratio)
{
	mat->data = realloc(mat->data, sizeof(mat->data) * rows * cols);
	if (mat->data == NULL)
	{
		return -1;
	}
	
	mat->rows = rows;
	mat->cols = cols;

	mat->drop_count = 0;
	mat->drop_ratio = drop_ratio;
	
	return 0;
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
	//struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };

	// set the buffering to fully buffered, we're adult and flush ourselves
	setvbuf(stdout, NULL, _IOFBF, 0);

	// initialize the matrix
	matrix_s mat = { 0 }; 
	mat_init(&mat, ws.ws_row, ws.ws_col, DROP_RATIO);
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
			mat_init(&mat, ws.ws_row, ws.ws_col, DROP_RATIO);
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
