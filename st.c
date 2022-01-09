/* See LICENSE for license details. */

#include <errno.h>
#include <locale.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "st.h"
#include "arg.h"

static void usage(void);

char *argv0;

char *opt_class = NULL;
char **opt_cmd  = NULL;
char *opt_embed = NULL;
char *opt_font  = NULL;
char *opt_geom  = NULL;
int opt_fixed   = 0;
char *opt_io    = NULL;
char *opt_line  = NULL;
char *opt_name  = NULL;
char *opt_title = NULL;

void
usage(void)
{
	die("usage: %s [-aiv] [-c class] [-f font] [-g geometry] [-n name] [-o file]\n"
	    "          [-T title] [-t title] [-w windowid] [[-e] command [args ...]]\n"
	    "       %s [-aiv] [-c class] [-f font] [-g geometry] [-n name] [-o file]\n"
	    "          [-T title] [-t title] [-w windowid] -l line [stty_args ...]\n",
		argv0, argv0);
}

void
run(void)
{
	int xfd, tfd;
	int dirty, drawing;
	fd_set rfd;
	double timeout;
	struct timespec tv, *tvp, now, trigger, lastblink;
	XEvent ev;

	xfd = xstart();
	tfd = tstart();

	timeout = -1;
	drawing = 0;
	lastblink = (struct timespec){0};
	for (;;) {
		/* Events already in Xlib's event queue won't set xfd. */
		if (XPending(xw.dpy))
			timeout = 0;
		/* Note there is no race condition between here and the call to
		 * pselect, because nothing is reading X events in to Xlib's event
		 * queue. */

		if (timeout >= 0) {
			tv.tv_sec = timeout / 1e3;
			tv.tv_nsec = 1e6 * (timeout - 1e3 * tv.tv_sec);
			tvp = &tv;
		} else {
			tvp = NULL;
		}

		FD_ZERO(&rfd);
		FD_SET(xfd, &rfd);
		FD_SET(tfd, &rfd);
		if (pselect(MAX(xfd, tfd)+1, &rfd, NULL, NULL, tvp, NULL) < 0) {
			if (errno == EINTR)
				continue;
			die("select failed: %s\n", strerror(errno));
		}

		clock_gettime(CLOCK_MONOTONIC, &now);

		if (FD_ISSET(tfd, &rfd)) {
			dirty = 1;
			ttyread();
		}

		/* TODO: temp; this X-specific stuff should be in win.c; run should
		 * just spin the event loop; all event handling should be elsewhere */
		while (XPending(xw.dpy)) {
			dirty = 1;
			XNextEvent(xw.dpy, &ev);
			if (XFilterEvent(&ev, None))
				continue;
			if (handler[ev.type])
				(handler[ev.type])(&ev);
		}

		/* To reduce flicker and tearing, when new content or event triggers
		 * drawing, we first wait a bit to ensure we got everything, and if
		 * nothing new arrives - we draw. We start with trying to wait
		 * minlatency ms. If more content arrives sooner, we retry with shorter
		 * and shorter periods, and eventually draw even without idle after
		 * maxlatency ms. Typically this results in low latency while
		 * interacting, maximum latency intervals during `cat huge.txt`, and
		 * perfect sync with periodic updates from animations/key-repeats/
		 * etc. */
		/* TODO: use global dirty flag that is only set when there definitely
		 * is something dirty; right now, dirty has false positives */
		if (dirty) {
			if (!drawing) {
				trigger = now;
				drawing = 1;
			}
			timeout = (maxlatency - TIMEDIFF(now, trigger)) /
				maxlatency * minlatency;
			if (timeout > 0)
				continue;  /* We have time; try to find idle. */
		}

		/* Idle detected or maxlatency exhausted, so draw. */
		timeout = -1;
		if (blinktimeout && tattrset(ATTR_BLINK)) {
			timeout = blinktimeout - TIMEDIFF(now, lastblink);
			if (timeout <= 0) {
				if (-timeout > blinktimeout) /* start visible */
					win.blink = 1;
				win.blink = !win.blink;
				tsetdirtattr(ATTR_BLINK);
				lastblink = now;
				timeout = blinktimeout;
			}
		}

		draw();
		XFlush(xw.dpy);
		drawing = 0;
	}
}

int
main(int argc, char **argv)
{
	uint cols, rows;

	ARGBEGIN {
	case 'a':
		allowaltscreen = 0;
		break;
	case 'c':
		opt_class = EARGF(usage());
		break;
	case 'e':
		if (argc > 0)
			--argc, ++argv;
		goto run;
	case 'f':
		opt_font = EARGF(usage());
		break;
	case 'g':
		opt_geom = EARGF(usage());
		break;
	case 'i':
		opt_fixed = 1;
		break;
	case 'o':
		opt_io = EARGF(usage());
		break;
	case 'l':
		opt_line = EARGF(usage());
		break;
	case 'n':
		opt_name = EARGF(usage());
		break;
	case 't':
	case 'T':
		opt_title = EARGF(usage());
		break;
	case 'w':
		opt_embed = EARGF(usage());
		break;
	case 'v':
		die("%s " VERSION "\n", argv0);
		break;
	default:
		usage();
	} ARGEND;

run:
	if (argc > 0) /* eat all remaining arguments */
		opt_cmd = argv;

	if (!opt_title)
		opt_title = (opt_line || !opt_cmd) ? "st" : opt_cmd[0];

	setlocale(LC_CTYPE, "");
	XSetLocaleModifiers(""); /* TODO: move to X land */
	/* TODO: data flow with cols/rows is weird */
	cols = MAX(defaultcols, 1);
	rows = MAX(defaultrows, 1);
	tinit(cols, rows);
	xinit(cols, rows);
	run();

	return 0;
}
