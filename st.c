/* See LICENSE for license details. */

#include <errno.h>
#include <locale.h>
#include <pwd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "st.h"
#include "arg.h"

static void usage(void);

/* Font selection priority:
 * 1. -f option
 * 2. defaultfont variable */
char *defaultfont = "monospace:pixelsize=32:antialias=true:autohint=true";

/* Terminal client priority:
 * 1. -e option
 * 2. SHELL environment variable
 * 3. user shell in /etc/passwd
 * 4. defaultshell variable */
char *defaultshell = "/bin/sh";

/* Terminal size priority:
 * 1. whatever the user/window manager forces
 * 2. -g option
 * 3. defaultcols and defaultrows variables*/
uint defaultcols = 80;
uint defaultrows = 24;

double minlatency = 8;     /* Minimum time the window is dirty in ms. */
double maxlatency = 33;    /* Maximum time the window is dirty in ms. */
double blinktimeout = 800; /* Half blink period in ms; 0 to disable blink.*/

char *termname = "st-256color";

/* Window title priority:
 * 1. whatever is set by the terminal client
 * 2. defaulttitle variable */
char *defaulttitle = "st";

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

void
usage(void)
{
	die("usage: %s [-iv] [-c class] [-f font] [-g geometry] [-n name] [-o file]\n"
	    "          [-t title] [-w windowid] [[-e] command [args ...]]\n"
	    "       %s [-iv] [-c class] [-f font] [-g geometry] [-n name] [-o file]\n"
	    "          [-t title] [-w windowid] -l line [stty_args ...]\n",
		argv0, argv0);
}

char *
resolveshell(void)
{
	char *sh;
	struct passwd *pw;

	/* 1. Check $SHELL */
	if (sh = getenv("SHELL"))
		return sh;

	/* 2. Check /etc/passwd */
	errno = 0;
	pw = getpwuid(getuid());
	if (pw == NULL)
		die("getpwuid failed: %s\n",
				errno ? strerror(errno) : "password file entry not found");
	if (pw->pw_shell[0])
		return pw->pw_shell;

	/* 3. Fallback to default */
	return defaultshell;
}

void
run(char *shell)
{
	int xfd, tfd;
	int dirty, drawing;
	fd_set rfd;
	double timeout;
	struct timespec tv, *tvp, now, trigger, lastblink;
	XEvent ev;

	xfd = xstart();
	tfd = tstart(shell);

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
		if (blinktimeout > 0 && tattrset(ATTR_BLINK)) {
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
	uint cols = defaultcols;
	uint rows = defaultrows;
	char *font = defaultfont;
	char *shell = resolveshell();
	char *title = defaulttitle;

	ARGBEGIN {
	case 'c':
		opt_class = EARGF(usage());
		break;
	case 'e':
		if (argc > 0)
			--argc, ++argv;
		goto run;
	case 'f':
		font = EARGF(usage());
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
		title = EARGF(usage());
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

	setlocale(LC_CTYPE, "");
	XSetLocaleModifiers(""); /* TODO: move to X land */

	/* TODO: data flow with cols/rows is weird... and broken! the geometry
	 * string could change size, but tinit doesn't know that */
	tinit(cols, rows);
	xinit(cols, rows, title, font);
	run(shell);

	return 0;
}
