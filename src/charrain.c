#include <stdio.h>      // fprintf(), stdout, setlinebuf()
#include <stdlib.h>     // EXIT_SUCCESS, EXIT_FAILURE, rand()
#include <unistd.h>     // sleep()
#include <signal.h>     // sigaction(), struct sigaction
#include <termios.h>    // struct winsize 
#include <sys/ioctl.h>  // ioctl(), TIOCGWINSZ

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define ANSI_HIDE_CURSOR "\e[?25l"
#define ANSI_SHOW_CURSOR "\e[?25h"

static volatile int resize;
static volatile int running;   // controls running of the main loop 
static volatile int handled;   // last signal that has been handled 

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

char rand_ascii()
{
	int r = rand() % 126;
	//return r < 32 ? r + 32 : r;
	return r < 32 ? 32 : r;

}

int main(int argc, char **argv)
{
	// https://man7.org/linux/man-pages/man4/tty_ioctl.4.html
	
	struct sigaction sa = { .sa_handler = &on_signal };
	sigaction(SIGKILL,  &sa, NULL);
	sigaction(SIGQUIT,  &sa, NULL);
	sigaction(SIGTERM,  &sa, NULL);
	sigaction(SIGWINCH, &sa, NULL);

	struct winsize ws = { 0 };
	ioctl(0, TIOCGWINSZ, &ws);

	setlinebuf(stdout);

	fprintf(stdout, ANSI_HIDE_CURSOR);
	fprintf(stdout, ANSI_COLOR_GREEN);

	running = 1;
	while(running)
	{
		if (resize)
		{
			ioctl(0, TIOCGWINSZ, &ws);
			resize = 0;
			printf("\033[2J");             // clear screen
			printf("\033[%dA", ws.ws_row); // move up ws.ws_row lines
		}

		for (int i = 0; i < (ws.ws_col - 1); ++i)
		{
			fprintf(stdout, "%c", rand_ascii()); 
		}
		fprintf(stdout, "\n");

		sleep(1);
	}
	fprintf(stdout, ANSI_COLOR_RESET);
	fprintf(stdout, ANSI_SHOW_CURSOR);
	return EXIT_SUCCESS;
}
