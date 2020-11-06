#include <stdio.h>      // fprintf(), stdout
#include <stdlib.h>     // EXIT_SUCCESS, EXIT_FAILURE, rand()
#include <unistd.h>     // sleep()
#include <signal.h>     // sigaction(), struct sigaction
#include <termios.h>    // struct winsize 
#include <sys/ioctl.h>  // ioctl(), TIOCGWINSZ

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
	return r < 32 ? r + 32 : r;

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
	
	char *rain = malloc(ws.ws_col);
	if (rain == NULL) return EXIT_FAILURE;

	running = 1;
	while(running)
	{
		if (resize)
		{
			ioctl(0, TIOCGWINSZ, &ws);
			rain = realloc(rain, ws.ws_col);
			resize = 0;
			printf("\033[2J");             // clear screen
			printf("\033[%dA", ws.ws_row); // move up ws.ws_row lines
		}

		for (int i = 0; i < (ws.ws_col - 1); ++i)
		{
			rain[i] = rand_ascii();
		}
		rain[ws.ws_col - 1] = '\0';

		fprintf(stdout, "%s\n", rain);

		sleep(1);
	}
	return EXIT_SUCCESS;
}
