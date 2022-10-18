#include <stdio.h>      // fprintf(), stdout, setlinebuf()
#include <stdlib.h>     // EXIT_SUCCESS, EXIT_FAILURE, rand()
#include <stdint.h>     // uint8_t, uint16_t, ...
#include <inttypes.h>   // PRIu8, PRIu16, ...
#include <unistd.h>     // getopt(), STDOUT_FILENO
#include <math.h>       // ceil()
#include <time.h>       // time(), nanosleep(), struct timespec
#include <signal.h>     // sigaction(), struct sigaction
#include <termios.h>    // struct winsize, struct termios, tcgetattr(), ...
#include <sys/ioctl.h>  // ioctl(), TIOCGWINSZ

// program information

#define PROGRAM_NAME "fakesteak"
#define PROGRAM_URL  "https://github.com/domsson/fakesteak"

#define PROGRAM_VER_MAJOR 0
#define PROGRAM_VER_MINOR 2
#define PROGRAM_VER_PATCH 4

// colors, adjust to your liking
// https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit

#define COLOR_BG   "\x1b[48;5;0m"   // background color, if to be used
#define COLOR_FG_0 "\x1b[38;5;231m" // color for the drop
#define COLOR_FG_1 "\x1b[38;5;48m"  // color for first tail cell
#define COLOR_FG_2 "\x1b[38;5;41m"  // ...
#define COLOR_FG_3 "\x1b[38;5;35m"  // ...
#define COLOR_FG_4 "\x1b[38;5;29m"  // ...
#define COLOR_FG_5 "\x1b[38;5;238m" // color for the last tail cell

// these can be tweaked if need be

#define ERROR_BASE_VALUE 0.01
#define ERROR_FACTOR_MIN 1
#define ERROR_FACTOR_MAX 100
#define ERROR_FACTOR_DEF 2

#define DROPS_BASE_VALUE 0.001
#define DROPS_FACTOR_MIN 1
#define DROPS_FACTOR_MAX 100
#define DROPS_FACTOR_DEF 10

#define SPEED_BASE_VALUE 1.00 
#define SPEED_FACTOR_MIN 1
#define SPEED_FACTOR_MAX 100
#define SPEED_FACTOR_DEF 10

// do not change these 

#define ANSI_FONT_RESET "\x1b[0m"
#define ANSI_FONT_BOLD  "\x1b[1m"
#define ANSI_FONT_FAINT "\x1b[2m"

#define ANSI_HIDE_CURSOR "\x1b[?25l"
#define ANSI_SHOW_CURSOR "\x1b[?25h"

#define ANSI_CLEAR_SCREEN "\x1b[2J"
#define ANSI_CURSOR_RESET "\x1b[H"

#define BITMASK_ASCII 0x00FF
#define BITMASK_STATE 0x0300
#define BITMASK_TSIZE 0xFC00

#define STATE_NONE 0
#define STATE_DROP 1
#define STATE_TAIL 2

#define TSIZE_MIN 8
#define TSIZE_MAX 63

#define ASCII_MIN 32
#define ASCII_MAX 126

#define NS_PER_SEC 1000000000

// for easy access of colors later on

static char *colors[] =
{
	COLOR_FG_0,
	COLOR_FG_1,
	COLOR_FG_2,
	COLOR_FG_3,
	COLOR_FG_4, 
	COLOR_FG_5
};

#define NUM_COLORS sizeof(colors) / sizeof(colors[0])

// these are flags used for signal handling

static volatile int resized;   // window resize event received
static volatile int running;   // controls running of the main loop 

//
//  the matrix' data represents a 2D array of size cols * rows.
//  every data element is a 16 bit int which stores information
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
	uint16_t *data;     // matrix data
	uint16_t  cols;     // number of columns
	uint16_t  rows;     // number of rows
	size_t drop_count;  // current number of drops
	float  drop_ratio;  // desired ratio of drops
}
matrix_s;

typedef struct options
{
	uint8_t speed;         // speed factor
	uint8_t drops;         // drops ratio / factor
	uint8_t error;         // error ratio / factor
	time_t  rands;         // seed for rand()
	uint8_t bg : 1;        // use background color
	uint8_t help : 1;      // show help and exit
	uint8_t version : 1;   // show version and exit
}
options_s;

/*
 * Parse command line args into the provided options_s struct.
 */
static void
parse_args(int argc, char **argv, options_s *opts)
{
	opterr = 0;
	int o;
	while ((o = getopt(argc, argv, "bd:e:hr:s:V")) != -1)
	{
		switch (o)
		{
			case 'b':
				opts->bg = 1;
				break;
			case 'd':
				opts->drops = atoi(optarg);
				break;
			case 'e':
				opts->error = atoi(optarg);
				break;
			case 'h':
				opts->help = 1;
				break;
			case 'r':
				opts->rands = atol(optarg);
				break;
			case 's':
				opts->speed = atoi(optarg);
				break;
			case 'V':
				opts->version = 1;
				break;
		}
	}
}

/*
 * Print usage information.
 */
static void
help(const char *invocation, FILE *where)
{
	fprintf(where, "USAGE\n");
	fprintf(where, "\t%s [OPTIONS...]\n\n", invocation);
	fprintf(where, "OPTIONS\n");
	fprintf(where, "\t-b\tuse black background color\n");
	fprintf(where, "\t-d\tdrops ratio (%"PRIu8" .. %"PRIu8", default: %"PRIu8")\n",
		       	DROPS_FACTOR_MIN, DROPS_FACTOR_MAX, DROPS_FACTOR_DEF);
	fprintf(where, "\t-e\terror ratio (%"PRIu8" .. %"PRIu8", default: %"PRIu8")\n", 
			ERROR_FACTOR_MIN, ERROR_FACTOR_MAX, ERROR_FACTOR_DEF);
	fprintf(where, "\t-h\tprint this help text and exit\n");
	fprintf(where, "\t-r\tseed for the random number generator\n");
	fprintf(where, "\t-s\tspeed factor (%"PRIu8" .. %"PRIu8", default: %"PRIu8")\n", 
			SPEED_FACTOR_MIN, SPEED_FACTOR_MAX, SPEED_FACTOR_DEF);
	fprintf(where, "\t-V\tprint version information and exit\n");
}

/*
 * Print version information.
 */
static void
version(FILE *where)
{
	fprintf(where, "%s %d.%d.%d\n%s\n", PROGRAM_NAME,
			PROGRAM_VER_MAJOR, PROGRAM_VER_MINOR, PROGRAM_VER_PATCH,
			PROGRAM_URL);
}

/*
 * Signal handler.
 */
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
}

/*
 * Make sure `val` is within the range [min, max].
 */
static void
clamp_uint8(uint8_t *val, uint8_t min, uint8_t max)
{
	if (*val < min) { *val = min; return; }
	if (*val > max) { *val = max; return; }
}

/*
 * Return a pseudo-random int in the range [min, max].
 */
static int
rand_int(int min, int max)
{
	return min + rand() % ((max + 1) - min);
}

/*
 * Return a psuedo-random int in the range [min, max], where any value smaller 
 * than min will be turned to min, hence giving a bias towards that number.
 */
static int
rand_int_mincap(int min, int max)
{
	int r = rand() % max;
	return r < min ? min : r;
}

/*
 * Return a pseudo-random ASCII character, where there is a somewhat 
 * greater chance of getting a space than any other char.
 */
static uint8_t 
rand_ascii()
{
	return rand_int_mincap(ASCII_MIN, ASCII_MAX);
}

//
// Functions to manipulate individual matrix cell values
//

/*
 * Create a 16 bit matrix value from the given 8 bit values representing 
 * a ASCII char, the cell state and the tail size (or color index).
 */
static uint16_t
val_new(uint8_t ascii, uint8_t state, uint8_t tsize)
{
	return (BITMASK_TSIZE & (tsize << 10)) | (BITMASK_STATE & (state << 8)) | ascii;
}

/*
 * Extract the 8 bit ASCII char from the given 16 bit matrix value.
 */
static uint8_t
val_get_ascii(uint16_t value)
{
	return value & BITMASK_ASCII;
}

/*
 * Extract the 2 bit cell state from the given 16 bit matrix value.
 */
static uint8_t
val_get_state(uint16_t value)
{
	return (value & BITMASK_STATE) >> 8;
}

/*
 * Extract the 6 bit tail size/ color index from the given 16 bit matrix value.
 */
static uint8_t
val_get_tsize(uint16_t value)
{
	return (value & BITMASK_TSIZE) >> 10;
}

//
// Functions to access / set matrix values
//

/*
 * Get the matrix array index for the given row and column.
 */
static int
mat_idx(matrix_s *mat, int row, int col)
{
	return row * mat->cols + col;
}

/*
 * Get the 16 bit matrix value from the cell at the given row and column.
 */
static uint16_t
mat_get_value(matrix_s *mat, int row, int col)
{
	if (row >= mat->rows) return 0;
	if (col >= mat->cols) return 0;
	return mat->data[mat_idx(mat, row, col)];
}

/*
 * Get the 2 bit cell state from the cell at the given row and column.
 */
static uint8_t
mat_get_state(matrix_s *mat, int row, int col)
{
	return val_get_state(mat_get_value(mat, row, col));
}

/*
 * Set the 16 bit matrix value for the cell at the given row and column.
 */
static uint8_t
mat_set_value(matrix_s *mat, int row, int col, uint16_t value)
{
	if (row >= mat->rows) return 0;
	if (col >= mat->cols) return 0;
	return mat->data[mat_idx(mat, row, col)] = value;
}

/*
 * Set the 8 bit ASCII char for the cell at the given row and column.
 */
static uint8_t
mat_set_ascii(matrix_s *mat, int row, int col, uint8_t ascii)
{
	uint16_t value = mat_get_value(mat, row, col);
	return mat_set_value(mat, row, col, 
			val_new(ascii, val_get_state(value), val_get_tsize(value)));
}

/*
 * Set the 2 bit cell state for the cell at the given row and column.
 */
static uint8_t
mat_set_state(matrix_s *mat, int row, int col, uint8_t state)
{
	uint16_t value = mat_get_value(mat, row, col);
	uint8_t  tsize = state == STATE_NONE ? 0 : val_get_tsize(value);
	return mat_set_value(mat, row, col, 
			val_new(val_get_ascii(value), state, tsize));
}

/*
 * Set the 6 bit tail size for the cell at the given row and column.
 */
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

/*
 * Randomly change some characters in the matrix.
 */
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

/*
 * Print the matrix to stdout.
 */
static void
mat_print(matrix_s *mat)
{
	uint16_t value = 0;
	uint8_t  state = STATE_NONE;
	size_t   size  = mat->cols * mat->rows;

	for (int i = 0; i < size; ++i)
	{
		value = mat->data[i];
		state = val_get_state(value);

		// fputc() + fputs() is faster than one call to printf()
		// TODO investigate if the *_unlocked functions are faster;
		//      and also, if faster, are they safe to use here?

		switch (state)
		{
			case STATE_NONE:
				fputc(' ', stdout);
				//fputc_unlocked(' ', stdout);
				break;
			case STATE_DROP:
				fputs(colors[0], stdout);
				fputc(val_get_ascii(value), stdout);
				//fputc_unlocked(val_get_ascii(value), stdout);
				break;
			case STATE_TAIL:
				fputs(colors[val_get_tsize(value)], stdout);
				fputc(val_get_ascii(value), stdout);
				//fputc_unlocked(val_get_ascii(value), stdout);
				break;
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
	int color = ceil((NUM_COLORS-1) * intensity);
	mat_set_state(mat, row, col, STATE_TAIL);
	mat_set_tsize(mat, row, col, color);
}

/*
 * Add a DROP, including its TAIL cells, to the matrix, 
 * starting from the specified position.
 *
 * TODO make it so it can also draw partial traces, where
 *      the drop is past the bottom row, but some of the
 *      tail cells are still inside the matrix
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
			mat->drop_count += 1;
		}
		else
		{
			mat_put_cell_tail(mat, row, col, tsize, i);
		}
	}
}

/*
 * Make it rain by randomly adding DROPs to the matrix, based on the 
 * drop_ratio of the given matrix.
 *
 * TODO a nicer implementation would be to base the number of drops 
 *      to add on the drop_count field; however, we then also need to
 *      make sure that we reset this field to 0 on matrix resize and 
 *      before calling this function.
 */
static void
mat_rain(matrix_s *mat)
{
	int num = (int) (mat->cols * mat->rows) * mat->drop_ratio;

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

/*
 * Update the matrix by moving all drops down one cell and potentially 
 * adding new drops at the top of the matrix.
 */
static void 
mat_update(matrix_s *mat)
{
	// move each column down one cell, possibly dropping some drops
	for (int col = 0; col < mat->cols; ++col)
	{
		mat->drop_count -= mat_mov_col(mat, col);
	}
	
	// add new drops at the top, trying to get to the desired drop count
	int drops_desired = (mat->cols * mat->rows) * mat->drop_ratio;
	int drops_missing = drops_desired - mat->drop_count; 
	int drops_to_add  = ceil(drops_missing / (float) mat->rows);

	for (int i = 0; i <= drops_to_add; ++i)
	{
		mat_add_drop(mat, 0, rand_int(0, mat->cols - 1), 
				rand_int(TSIZE_MIN, TSIZE_MAX));
	}
}

/*
 * Fill the entire matrix with random characters, setting all cells to state
 * STATE_NONE in the process.
 */
static void
mat_fill(matrix_s *mat)
{
	for (int r = 0; r < mat->rows; ++r)
	{
		for (int c = 0; c < mat->cols; ++c)
		{
			mat_set_state(mat, r, c, STATE_NONE);
			mat_set_ascii(mat, r, c, rand_ascii());
		}
	}
}

/*
 * Creates or recreates (resizes) the given matrix.
 * Returns -1 on error (out of memory), 0 on success.
 */
static int
mat_init(matrix_s *mat, uint16_t rows, uint16_t cols, float drop_ratio)
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

/*
 * Free ALL the memory \o/
 */
void
mat_free(matrix_s *mat)
{
	free(mat->data);
}

/*
 * Try to figure out the terminal size, in character cells, and return that 
 * info in the given winsize structure. Returns 0 on succes, -1 on error.
 * However, you might still want to check if the ws_col and ws_row fields 
 * actually contain values other than 0. They should. But who knows.
 */
int
cli_wsize(struct winsize *ws)
{
#ifndef TIOCGWINSZ
	return -1;
#endif
	return ioctl(STDOUT_FILENO, TIOCGWINSZ, ws);
}

/*
 * Turn echoing of keyboard input on/off.
 */
static int
cli_echo(int on)
{
	struct termios ta;
	if (tcgetattr(STDIN_FILENO, &ta) != 0)
	{
		return -1;
	}
	ta.c_lflag = on ? ta.c_lflag | ECHO : ta.c_lflag & ~ECHO;
	return tcsetattr(STDIN_FILENO, TCSAFLUSH, &ta);
}

/*
 * Prepare the terminal for the next paint iteration.
 */
static void
cli_clear()
{
	fputs(ANSI_CURSOR_RESET, stdout);
}

/*
 * Prepare the terminal for our matrix shenanigans.
 */
static void
cli_setup(options_s *opts)
{
	fputs(ANSI_HIDE_CURSOR, stdout);
	fputs(ANSI_FONT_BOLD, stdout);

	if (opts->bg)
	{
		fputs(COLOR_BG, stdout);
	}

	fputs(ANSI_CLEAR_SCREEN, stdout); // clear screen
	fputs(ANSI_CURSOR_RESET, stdout); // cursor back to position 0,0
	cli_echo(0);                      // don't show keyboard input
	
	// set the buffering to fully buffered, we're adult and flush ourselves
	setvbuf(stdout, NULL, _IOFBF, 0);
}

/*
 * Make sure the terminal goes back to its normal state.
 */
static void
cli_reset()
{
	fputs(ANSI_FONT_RESET, stdout);   // resets font colors and effects
	fputs(ANSI_SHOW_CURSOR, stdout);  // show the cursor again
	fputs(ANSI_CLEAR_SCREEN, stdout); // clear screen
	fputs(ANSI_CURSOR_RESET, stdout); // cursor back to position 0,0
	cli_echo(1);                      // show keyboard input

	setvbuf(stdout, NULL, _IOLBF, 0);
}

/*
 * Some good resources that have helped me with this project:
 *
 * https://youtu.be/MvEXkd3O2ow?t=26
 * https://matrix.logic-wire.de/
 *
 * https://man7.org/linux/man-pages/man4/tty_ioctl.4.html
 * https://en.wikipedia.org/wiki/ANSI_escape_code
 * https://gist.github.com/XVilka/8346728
 * https://stackoverflow.com/a/33206814/3316645 
 * https://jdebp.eu/FGA/clearing-the-tui-screen.html#POSIX
 */
int
main(int argc, char **argv)
{
	// set signal handlers for the usual susspects plus window resize
	struct sigaction sa = { .sa_handler = &on_signal };
	sigaction(SIGINT,   &sa, NULL);
	sigaction(SIGQUIT,  &sa, NULL);
	sigaction(SIGTERM,  &sa, NULL);
	sigaction(SIGWINCH, &sa, NULL);

	// parse command line options
	options_s opts = { 0 };
	parse_args(argc, argv, &opts);

	if (opts.help)
	{
		help(argv[0], stdout);
		return EXIT_SUCCESS;
	}

	if (opts.version)
	{
		version(stdout);
		return EXIT_SUCCESS;
	}

	if (opts.speed == 0)
	{
		opts.speed = SPEED_FACTOR_DEF;
	}

	if (opts.drops == 0)
	{
		opts.drops = DROPS_FACTOR_DEF;
	}

	if (opts.error == 0)
	{
		opts.error = ERROR_FACTOR_DEF;
	}

	if (opts.rands == 0)
	{
		opts.rands = time(NULL);
	}
	
	// make sure the values are within expected/valid range
	clamp_uint8(&opts.speed, SPEED_FACTOR_MIN, SPEED_FACTOR_MAX);
	clamp_uint8(&opts.drops, DROPS_FACTOR_MIN, DROPS_FACTOR_MAX);
	clamp_uint8(&opts.error, ERROR_FACTOR_MIN, ERROR_FACTOR_MAX);

	// get the terminal dimensions
	struct winsize ws = { 0 };
	if (cli_wsize(&ws) == -1)
	{
		fprintf(stderr, "Failed to determine terminal size\n");
		return EXIT_FAILURE;
	}

	if (ws.ws_col == 0 || ws.ws_row == 0)
	{
		fprintf(stderr, "Terminal size not appropriate\n");
		return EXIT_FAILURE;
	}

	// calculate some spicy values from the options
	float wait = SPEED_BASE_VALUE / (float) opts.speed;
	float drops_ratio = DROPS_BASE_VALUE * opts.drops;
	float error_ratio = ERROR_BASE_VALUE * opts.error;

	// set up the nanosleep struct
	uint8_t  sec  = (int) wait;
	uint32_t nsec = (wait - sec) * NS_PER_SEC;
	struct timespec ts = { .tv_sec = sec, .tv_nsec = nsec };
	
	// seed the random number generator with the current unix time
	srand(opts.rands);

	// initialize the matrix
	matrix_s mat = { 0 }; 
	mat_init(&mat, ws.ws_row, ws.ws_col, drops_ratio);
	mat_fill(&mat);

	// prepare the terminal for our shenanigans
	cli_setup(&opts);

	running = 1;
	while(running)
	{
		if (resized)
		{
			// query the terminal size again
			cli_wsize(&ws);
			
			// reinitialize the matrix
			mat_init(&mat, ws.ws_row, ws.ws_col, drops_ratio);
			mat_fill(&mat);
			mat_rain(&mat); // TODO maybe this isn't desired?
			resized = 0;
		}

		cli_clear();
		mat_print(&mat);                // print to the terminal
		mat_glitch(&mat, error_ratio);  // apply random defects
		mat_update(&mat);               // move all drops down one row
		nanosleep(&ts, NULL);
	}

	// make sure all is back to normal before we exit
	mat_free(&mat);	
	cli_reset();
	return EXIT_SUCCESS;
}
