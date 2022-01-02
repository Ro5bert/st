/* See LICENSE for license details. */

#include <locale.h>
#include <stdint.h>
#include <stdlib.h>
#include <X11/Xlib.h>

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

int
main(int argc, char *argv[])
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
	xmain();

	return 0;
}
