/*
 * waitidle.c -- find optimal [Waitidle] parameters
 *
 * This program runs the "Browser test", measures times and notes them.
 * Running it in a loop it tunes the parameters by itself.
 * For meaningful numbers it must be started along with hildon-home,
 * whose waitidle function must be disabled.
 */

/* Include files */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include <sys/time.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <X11/extensions/XTest.h>

/* Standard definitions */
#define FNAME		"/home/bg"
#define TIMEOUT		60

/* Genetic algorithm parameters */
#define POPSIZE		30
#define SURVIVORS	 0.1
#define MUTATION	 0.44
#define CROSSOVER	 0.8

/* Type definitions */
struct state_st
{
	struct genome_st
	{
		unsigned window;
		double threshold;
		unsigned score;
		int done;
	} pop[POPSIZE];

	unsigned idx;
};

/* Private variables */
static Display *Dpy;
static GMainLoop *Loop;
static DBusConnection *DBus;

/* Program code */
/* Block until the CPU is idling at least @threshold ratio of the time
 * over @window seconds, but return after @timeout in any case. */
static int waitidle(unsigned window, double threshold, unsigned timeout)
{
	FILE *st;
	unsigned nidles, idlep;
	double idles[window];
	long long prev_total, prev_idle;

	/* Where we can get the time spent in idle. */
	assert((st = fopen("/proc/stat", "r")) != NULL);

	/* Don't buffer, we'll reread the file periodically. */
	setvbuf(st, NULL, _IONBF, 0);

	fputs("idles:", stdout);
	prev_total = prev_idle = nidles = idlep = 0;
	for (; timeout > 0; timeout--)
	{
		double idlef;
		long long total, usr, nic, sys, idle, iowait, irq, softirq, steal;

		/* Read the jiffies. */
		assert(fscanf(st, "cpu  "
			"%lld %lld %lld %lld %lld %lld %lld %lld",
			&usr, &nic, &sys, &idle, &iowait, &irq,
			&softirq, &steal) >= 8);
		total = usr + nic + sys + idle + iowait + irq + softirq + steal;

		/* We need two consecutive samples to calculate idle%. */
		if (!prev_total)
			goto next;

		/* Calculate the ratio spent in idle. */
		if (!(idlef = total-prev_total))
			idlef = idle - prev_idle;
		else
			idlef = (double)(idle-prev_idle) / idlef;

		/* Add it to the window. */
		idles[idlep++] = idlef;
		if (nidles < window)
			nidles++;

		/* If the window is full see if the average
		 * has reached @threshold. */
		if (nidles >= window)
		{
			unsigned i;

			idlep %= nidles;
			for (idlef = i = 0; i < nidles; i++)
				idlef += idles[i];
			idlef /= nidles;
			if (idlef >= threshold)
				break;
		}

		printf(" %f", idlef);
next:		/* Prepare for the next turn. */
		fseek(st, 0, SEEK_SET);
		prev_total = total;
		prev_idle  = idle;
		sleep(1);
	}

	fputc('\n', stdout);
	fclose(st);
	return timeout > 0;
} /* waitidle */

/* Simulate a click at (@x, @y). */
static void click(unsigned x, unsigned y)
{
	XTestFakeMotionEvent(Dpy, DefaultScreen(Dpy), x, y, 0);
	XTestFakeButtonEvent(Dpy, Button1, True, 0);
	XTestFakeButtonEvent(Dpy, Button1, False, 250);
	XFlush(Dpy);
} /* click */

/* D-BUS filter to set *@successp and quit the main loop as soon as
 * it sees a LaunchApplication request on the session bus. */
static DBusHandlerResult filter(DBusConnection *con, DBusMessage *msg,
	void *successp)
{
	char const *str;

	if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_CALL
		&& (str = dbus_message_get_destination(msg)) != NULL
		&& !strcmp(str, "com.nokia.HildonDesktop.AppMgr")
		&& (str = dbus_message_get_interface(msg)) != NULL
		&& !strcmp(str, "com.nokia.HildonDesktop.AppMgr")
		&& (str = dbus_message_get_member(msg)) != NULL
		&& !strcmp(str, "LaunchApplication"))
	{
		*(int *)successp = 1;
		g_main_loop_quit(Loop);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
} /* filter */

/* Blocks until a LaunchApplication request is seen on the session bus,
 * or when @timeout is reached. */
static int waitdbus(unsigned timeout)
{
	int success;

	success = 0;
	dbus_connection_add_filter(DBus, filter, &success, NULL);
	g_timeout_add_seconds(timeout, (GSourceFunc)g_main_loop_quit, Loop);
	g_main_loop_run(Loop);

	return success;
} /* waitdbus */

/* GA functions */
/* Initialize @state with random genomes. */
static void initpop(struct state_st *state)
{
	unsigned i;

	fputs("init\n", stdout);
	state->idx = 0;
	for (i = 0; i < G_N_ELEMENTS(state->pop); i++)
	{
		struct genome_st *g = &state->pop[i];
		g->window = 3 + (rand() % (TIMEOUT-3));
		g->threshold = (double)rand() / RAND_MAX;
	}
} /* initpop */

/* Returns the difference between @now and @prev in microseconds. */
static unsigned diffit(struct timeval const *now, struct timeval const *prev)
{
        return (now->tv_sec-prev->tv_sec) * 1000000
		+ (now->tv_usec-prev->tv_usec);
} /* diffit */

/* Evaluate a genome and set its score. */
static void eval(struct genome_st *g)
{
	int to1, to2;
	unsigned d1, d2;
	struct timeval t0, t1, t2, t3;

        /* Measure how much time we waited until the idle system
         * and the time it take the application loading animation
         * to be commenced after the click. */
	gettimeofday(&t0, NULL);
	to1 = waitidle(g->window, g->threshold, TIMEOUT);
	gettimeofday(&t1, NULL);
	click(150, 200);
	gettimeofday(&t2, NULL);
	to2 = waitdbus(30);
	gettimeofday(&t3, NULL);

        /* Calculate the score and note the results. */
	d1 = diffit(&t1, &t0);
	d2 = diffit(&t3, &t2);
	g->score = d1 + d2;
	printf( "window=%u, threshold=%f, t1=%u%s, t2=%u%s, score=%u\n",
		g->window, g->threshold,
		d1, to1 ? "" : " [timeout]",
		d2, to2 ? "" : " [timeout]",
		g->score);
} /* eval */

/* Compare two genomes by score. */
static int cmpgenome(struct genome_st const *lhs, struct genome_st const *rhs)
{
        /* The lighter is the winner. */
	if (lhs->score < rhs->score)
		return -1;
	else if (lhs->score > rhs->score)
		return +1;
	else
		return  0;
} /* cmpgenome */

/* Returns a random !done genome from @state->pop.
 * There has to be at least one unprocessed genome in the population. */
static struct genome_st *pick(struct state_st *state)
{
	unsigned i;

	for (i = rand(); ; i++)
	{
		struct genome_st *g;

		g = &state->pop[i % state->idx];
		if (!g->done)
		{
			g->done = 1;
			return g;
		}
	}
} /* pick */

/* Evolve the population. */
static void generation(struct state_st *state)
{
	unsigned n, i;

        /* Sort the population, so the best genomes will be
         * the first ones. */
	fputs("generation\n", stdout);
	qsort(state->pop, state->idx, sizeof(state->pop[0]),
		(void *)cmpgenome);

        /* Mutate random genomes.  Skip the best ones. */
	n = state->idx * SURVIVORS;
	for (i = n; i < state->idx; i++)
	{
		struct genome_st *g = &state->pop[i];

		if (rand() < MUTATION*RAND_MAX)
		{	/* Mutate $g. */
			int w, t, r;

                        /* What to mutate: window or threshold or both. */
			w = t = 1;
			r = rand() % 5;
			if (r < 2)
				t = 0;
			else if (r < 4)
				w = 0;

			if (w)
			{       /* Mutate the window by 1s. */
				if (g->window <= 3)
					g->window++;
				else if (g->window >= TIMEOUT)
					g->window--;
				else if (rand() % 2)
					g->window++;
				else
					g->window--;
			}

			if (t)
			{       /* Mutate the threshold by 0.1. */
				if (g->threshold <= 0.1)
					g->threshold += 0.1;
				else if (g->threshold >= 0.9)
					g->threshold -= 0.1;
				else if (rand() % 2)
					g->threshold += 0.1;
				else
					g->threshold -= 0.1;
			}

			g->done = 1;
		} else  /* Not mutated, may be crossed over. */
			g->done = 0;
	}

        /* Select a random group of genomes to be crossed over.
         * Skip those that have been deal with. */
	for (i = n, n = 0; i < state->idx; i++)
	{
		if (!state->pop[i].done && !(rand() < CROSSOVER*RAND_MAX))
			state->pop[i].done = 1;
		if (!state->pop[i].done)
			n++;
	}

        /* Cross over the selected genomes. */
	for (n /= 2; n > 0; n--)
	{
		unsigned tmp;
		struct genome_st *g1, *g2;

                /* Swap the genes of two randomly chosen genomes. */
		g1 = pick(state);
		g2 = pick(state);

		/* Crossover $g1 with $g2. */
		tmp = g1->window;
		g1->window = g2->window;
		g2->window = tmp;
	}

        /* Restart the evaluation of the population. */
	state->idx = 0;
} /* generation */

/* Dump the whole population in human readable format. */
static void dumpop(struct state_st *state)
{
	unsigned i;

	for (i = 0; i < G_N_ELEMENTS(state->pop); i++)
	{
		struct genome_st *g = &state->pop[i];
		printf("%u\t%f\n", g->window, g->threshold);
	}
} /* dumpop */

/* Restore the GA state from a file. */
static int loadstate(struct state_st *state)
{
	int fd;

	if ((fd = open(FNAME".state", O_RDONLY)) < 0)
		return 0;
	assert(read(fd, state, sizeof(*state)) == sizeof(*state));
	close(fd);
	return 1;
} /* loadstate */

/* Save the GA state in a file. */
static void savestate(struct state_st const *state)
{
	int fd;

	fd = open(FNAME".state", O_CREAT|O_TRUNC|O_WRONLY, 0666);
	assert(fd >= 0);
	assert(write(fd, state, sizeof(*state)) == sizeof(*state));
	fsync(fd);
	close(fd);

	fflush(stdout);
	fsync(fileno(stdout));
} /* savestate */

/* The main function */
int main(int argc, char const *argv[])
{
	DBusError dbe;
	struct state_st state;

        /* Parse the command line. */
	if (argv[1] && argv[1][0] == 'd')
	{       /* Debug dump */
		assert(loadstate(&state));
		dumpop(&state);
		return 0;
	} else if (argv[1] && argv[1][0] == 'r')
	{       /* Repair the GA state. */
		unsigned i;

		assert(loadstate(&state));
		for (i = 0; i < G_N_ELEMENTS(state.pop); i++)
		{       /* Ensure that genes are bounded. */
			struct genome_st *g = &state.pop[i];
			if (g->window > TIMEOUT)
				g->window = TIMEOUT;
			else if (g->window < 3)
				g->window = 3;
			if (g->threshold <= 0)
				g->threshold = 0.1;
			else if (g->threshold >= 1)
				g->threshold = 0.9;
		}
		savestate(&state);
		return 0;
	}

        /* Redirect stdio. */
	assert((stderr = fopen(FNAME".err", "a")) != NULL);
	assert((stdout = fopen(FNAME".log", "a")) != NULL);

        /* Initialize infrastructure for the test. */
	Loop = g_main_loop_new(0, 0);

	DBus = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	dbus_connection_setup_with_g_main(DBus, g_main_context_default());
	dbus_error_init(&dbe);
	dbus_bus_add_match(DBus, "type='method_call',interface='com.nokia.HildonDesktop.AppMgr',member='LaunchApplication'", &dbe);
	assert(!dbus_error_is_set(&dbe));

	Dpy = XOpenDisplay(NULL);
	assert(Dpy != NULL);

	if (!loadstate(&state))
		initpop(&state);

        /* Test */
	eval(&state.pop[state.idx++]);
	if (state.idx >= G_N_ELEMENTS(state.pop))
		generation(&state);

        /* Done and reboot. */
	savestate(&state);
	if (argv[1])
		system("echo reboot | sudo gainroot");

	return 0;
} /* main */

/* End of waitidle.c */
