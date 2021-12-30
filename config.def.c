/* See LICENSE file for license details. */
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include "util.h"
#include "config.h"
#include "st.h"
#include "win.h"

static void clipcopy(uint, Arg);
static void clippaste(uint, Arg);
static void selpaste(uint, Arg);
static void zoomrel(uint, Arg);
static void zoomrst(uint, Arg);
static void numlock(uint, Arg);
static void sendstr(uint, Arg);
static void sendcsi(uint, Arg);
static void printscreen(uint, Arg);
static void selprint(uint, Arg);
static void sendbreak(uint, Arg);
static void togprinter(uint, Arg);

/* See: http://freedesktop.org/software/fontconfig/fontconfig-user.html */
char *font = "Fira Mono:pixelsize=32:antialias=true:autohint=true";
int borderpx = 2;
/* Kerning / character bounding-box multipliers */
float cwscale = 1.0;
float chscale = 1.0;

/* What program is execed by st depends of these precedence rules:
 * 1: program passed with -e
 * 2: scroll and/or utmp
 * 3: SHELL environment variable
 * 4: user shell in /etc/passwd
 * 5: shell in config.c */
char *shell = "/bin/sh";
char *utmp = 0;
/* scroll program: to enable use a string like "scroll" */
char *scroll = 0;
char *stty_args = "stty raw pass8 nl -echo -iexten -cstopb 38400";

/* identification sequence returned in DA and DECID */
char *vtiden = "\033[?6c";

wchar_t *worddelimiters = L" "; /* E.g., L" `'\"()[]{}" */

/* selection timeouts (in milliseconds) */
uint doubleclicktimeout = 300;
uint tripleclicktimeout = 600;

int allowaltscreen = 1;

/* allow certain non-interactive (insecure) window operations, such as
   setting the clipboard text */
int allowwindowops = 0;

/* draw latency range in ms - from new content/keypress/etc until drawing.
 * within this range, st draws when content stops arriving (idle). mostly it's
 * near minlatency, but it waits longer for slow updates to avoid partial draw.
 * low minlatency will tear/flicker more, as it can "detect" idle too early. */
double minlatency = 8;
double maxlatency = 33;

/* blinking timeout (set to 0 to disable blinking) for the terminal blinking
 * attribute. */
uint blinktimeout = 800;

/* thickness of underline and bar cursors */
uint cursorthickness = 2;

/* bell volume. It must be a value between -100 and 100.
 * Use 0 to disable it. */
int bellvolume = 0;

char *termname = "st-256color";

/* spaces per tab
 * When you are changing this value, don't forget to adapt the »it« value in
 * the st.info and appropriately install the st.info in the environment where
 * you use this st version.
 * Secondly make sure your kernel is not expanding tabs. When running
 * `stty -a`, »tab0« should appear. You can tell the terminal to not expand
 * tabs by running `stty tabs`. */
uint tabspaces = 4;

/* Terminal colors (16 first used in escape sequence) */
const char *colorname[] = {
	/* 8 normal colors */
	"black",
	"red3",
	"green3",
	"yellow3",
	"blue2",
	"magenta3",
	"cyan3",
	"gray90",

	/* 8 bright colors */
	"gray50",
	"red",
	"green",
	"yellow",
	"#5c5cff",
	"magenta",
	"cyan",
	"white",

	[255] = 0,

	/* more colors can be added after 255 to use with defaultXX */
	"#f8f8f2",
	"#1c1c1c",
	"#cccccc",
	"#555555",
	0,
};

/* Default colors (colorname index)
 * foreground, background, cursor, reverse cursor */
uint defaultfg = 256;
uint defaultbg = 257;
uint defaultcs = 258;
uint defaultrcs = 259;

/* Default shape of cursor
 * 2: Block ("█")
 * 4: Underline ("_")
 * 6: Bar ("|")
 * 7: Snowman ("☃") */
uint cursorshape = 2;

uint cols = 80;
uint rows = 24;

/* Default colour and shape of the mouse cursor */
uint mouseshape = XC_xterm;
uint mousefg = 7;
uint mousebg = 0;

/* Color used to display font attributes when fontconfig selected a font which
 * doesn't match the ones requested. */
uint defaultattr = 11;

#define R RELS
#define S SHFT
#define C CTRL
#define A ALT
#define TERMMOD (CTRL|SHFT)
#define SENDSTR(s_)    sendstr, .arg = ARG_STR((s_))
#define SENDCSI(n,m,c) sendcsi, .arg = ARG_CSI((n),(m),(c))
#define SENDTILDE(n)    SENDCSI((n),0,'~')
#define SENDUNICODE(cp) SENDCSI((cp),S,'u')
#define ARG_DUMMY {.i = 0}

/* Beware that overloading Button1 will disable the selection. */
Btn btns[] = {
	{ Button2,  R,           0,  selpaste, ARG_DUMMY  },
	{ Button4,  S,  KEXCL(S)|R,  SENDSTR("\033[5;2~") },
	{ Button4,  0,           R,  SENDSTR("\031")      },
	{ Button5,  S,  KEXCL(S)|R,  SENDSTR("\033[6;2~") },
	{ Button5,  0,           R,  SENDSTR("\005")      },
	{ 0 },
};

/* TODO: add RELS to clr */
Key keys[] = {
	/* Shortcuts (must be first to get precedence) */
	{ XK_Home,      TERMMOD,  KEXCL(TERMMOD)|R,       zoomrst,  ARG_DUMMY },
	{ XK_Prior,     TERMMOD,  KEXCL(TERMMOD)|R,       zoomrel,  {.d = +1} },
	{ XK_Next,      TERMMOD,  KEXCL(TERMMOD)|R,       zoomrel,  {.d = -1} },
	{ XK_Print,           C,        KEXCL(C)|R,    togprinter,  ARG_DUMMY },
	{ XK_Print,           S,        KEXCL(S)|R,   printscreen,  ARG_DUMMY },
	{ XK_Print,           0,                 R,      selprint,  ARG_DUMMY },
	{ XK_Insert,          S,        KEXCL(S)|R,      selpaste,  ARG_DUMMY },
	{ XK_Break,           0,                 R,     sendbreak,  ARG_DUMMY },
	{ XK_Num_Lock,  TERMMOD,  KEXCL(TERMMOD)|R,       numlock,  ARG_DUMMY },
	{ XK_C,         TERMMOD,  KEXCL(TERMMOD)|R,      clipcopy,  ARG_DUMMY },
	{ XK_V,         TERMMOD,  KEXCL(TERMMOD)|R,     clippaste,  ARG_DUMMY },
	{ XK_Y,         TERMMOD,  KEXCL(TERMMOD)|R,      selpaste,  ARG_DUMMY },

	/* ASCII special cases (handlesym handles most cases already) */
	{ XK_space,          S,             R,  SENDCSI(' ',0,'u') },
	{ XK_space,          C,    KEXCL(C)|R,  SENDSTR("\0")      },
	{ XK_space,        C|A,  KEXCL(C|A)|R,  SENDSTR("\033\0")  },
	{ XK_O,              A,             R,  SENDUNICODE('O')   }, /* ESC O  is SS3 */
	{ XK_i,              C,             R,  SENDUNICODE('i')   }, /* 'i'-0x60 is tab */
	{ XK_m,              C,             R,  SENDUNICODE('m')   }, /* 'm'-0x60 is CR  */
	{ XK_bracketleft,    A,             R,  SENDUNICODE('[')   }, /* ESC [  is CSI */

	/* Misc */
	{ XK_BackSpace,     0,      KMOD|R,  SENDSTR("\177")     },
	{ XK_BackSpace,     A,  KEXCL(A)|R,  SENDSTR("\033\177") },
	{ XK_BackSpace,     0,           R,  SENDCSI(127,0,'u')  },
	{ XK_Tab,           0,      KMOD|R,  SENDSTR("\t")       },
	{ XK_Tab,           A,  KEXCL(A)|R,  SENDSTR("\033\t")   },
	{ XK_Tab,           S,           R,  SENDCSI(1,S,'Z')    },
	{ XK_Tab,           0,           R,  SENDCSI('\t',0,'u') },
	{ XK_Return,        0,      KMOD|R,  SENDSTR("\r")       },
	{ XK_Return,        A,  KEXCL(A)|R,  SENDSTR("\033\r")   },
	{ XK_Return,        0,           R,  SENDCSI('\r',0,'u') },
	{ XK_Escape,        0,      KMOD|R,  SENDSTR("\033")     },
	{ XK_Escape,        A,  KEXCL(A)|R,  SENDSTR("\033\033") },
	{ XK_Escape,        0,           R,  SENDCSI(27,0,'u')   },
	{ XK_Delete,        0,           R,  SENDTILDE(3)        },
	{ XK_Home,       CURS,      KMOD|R,  SENDSTR("\033OH")   },
	{ XK_Home,          0,           R,  SENDCSI(1,0,'H')    },
	{ XK_Left,       CURS,      KMOD|R,  SENDSTR("\033OD")   },
	{ XK_Left,          0,           R,  SENDCSI(1,0,'D')    },
	{ XK_Up,         CURS,      KMOD|R,  SENDSTR("\033OA")   },
	{ XK_Up,            0,           R,  SENDCSI(1,0,'A')    },
	{ XK_Right,      CURS,      KMOD|R,  SENDSTR("\033OC")   },
	{ XK_Right,         0,           R,  SENDCSI(1,0,'C')    },
	{ XK_Down,       CURS,      KMOD|R,  SENDSTR("\033OB")   },
	{ XK_Down,          0,           R,  SENDCSI(1,0,'B')    },
	{ XK_Prior,         0,           R,  SENDTILDE(5)        },
	{ XK_Next,          0,           R,  SENDTILDE(6)        },
	{ XK_End,        CURS,      KMOD|R,  SENDSTR("\033OF")   },
	{ XK_End,           0,           R,  SENDCSI(1,0,'F')    },
	{ XK_Begin,         0,           R,  SENDCSI(1,0,'E')    },
	{ XK_Select,        0,           R,  SENDTILDE(4)        },
	{ XK_Insert,        0,           R,  SENDTILDE(2)        },
	{ XK_Find,          0,           R,  SENDTILDE(1)        },

	/* Keypad */
	{ XK_KP_Enter,      KPAD,  NMLK|KMOD|R,  SENDSTR("\033OM")   },
	{ XK_KP_Enter,         0,       KMOD|R,  SENDSTR("\r")       },
	{ XK_KP_Enter,         A,   KEXCL(A)|R,  SENDSTR("\033\r")   },
	{ XK_KP_Enter,         0,            R,  SENDCSI('\r',0,'u') },
	{ XK_KP_F1,            0,       KMOD|R,  SENDSTR("\033OP")   },
	{ XK_KP_F1,            0,            R,  SENDCSI(1,0,'P')    },
	{ XK_KP_F2,            0,       KMOD|R,  SENDSTR("\033OQ")   },
	{ XK_KP_F2,            0,            R,  SENDCSI(1,0,'Q')    },
	{ XK_KP_F3,            0,       KMOD|R,  SENDSTR("\033OR")   },
	{ XK_KP_F3,            0,            R,  SENDCSI(1,0,'R')    },
	{ XK_KP_F4,            0,       KMOD|R,  SENDSTR("\033OS")   },
	{ XK_KP_F4,            0,            R,  SENDCSI(1,0,'S')    },
	{ XK_KP_Home,       CURS,       KMOD|R,  SENDSTR("\033OH")   },
	{ XK_KP_Home,          0,            R,  SENDCSI(1,0,'H')    },
	{ XK_KP_Left,       CURS,       KMOD|R,  SENDSTR("\033OD")   },
	{ XK_KP_Left,          0,            R,  SENDCSI(1,0,'D')    },
	{ XK_KP_Up,         CURS,       KMOD|R,  SENDSTR("\033OA")   },
	{ XK_KP_Up,            0,            R,  SENDCSI(1,0,'A')    },
	{ XK_KP_Right,      CURS,       KMOD|R,  SENDSTR("\033OC")   },
	{ XK_KP_Right,         0,            R,  SENDCSI(1,0,'C')    },
	{ XK_KP_Down,       CURS,       KMOD|R,  SENDSTR("\033OB")   },
	{ XK_KP_Down,          0,            R,  SENDCSI(1,0,'B')    },
	{ XK_KP_Prior,         0,            R,  SENDTILDE(5)        },
	{ XK_KP_Next,          0,            R,  SENDTILDE(6)        },
	{ XK_KP_End,        CURS,       KMOD|R,  SENDSTR("\033OF")   },
	{ XK_KP_End,           0,            R,  SENDCSI(1,0,'F')    },
	{ XK_KP_Begin,         0,            R,  SENDCSI(1,0,'E')    },
	{ XK_KP_Insert,        0,            R,  SENDTILDE(2)        },
	{ XK_KP_Delete,        0,            R,  SENDTILDE(3)        },
	{ XK_KP_Equal,      KPAD,  NMLK|KMOD|R,  SENDSTR("\033OX")   },
	{ XK_KP_Multiply,   KPAD,  NMLK|KMOD|R,  SENDSTR("\033Oj")   },
	{ XK_KP_Add,        KPAD,  NMLK|KMOD|R,  SENDSTR("\033Ok")   },
	{ XK_KP_Separator,  KPAD,  NMLK|KMOD|R,  SENDSTR("\033Ol")   },
	{ XK_KP_Subtract,   KPAD,  NMLK|KMOD|R,  SENDSTR("\033Om")   },
	{ XK_KP_Decimal,    KPAD,  NMLK|KMOD|R,  SENDSTR("\033On")   },
	{ XK_KP_Divide,     KPAD,  NMLK|KMOD|R,  SENDSTR("\033Oo")   },
	{ XK_KP_0,          KPAD,  NMLK|KMOD|R,  SENDSTR("\033Op")   },
	{ XK_KP_1,          KPAD,  NMLK|KMOD|R,  SENDSTR("\033Oq")   },
	{ XK_KP_2,          KPAD,  NMLK|KMOD|R,  SENDSTR("\033Or")   },
	{ XK_KP_3,          KPAD,  NMLK|KMOD|R,  SENDSTR("\033Os")   },
	{ XK_KP_4,          KPAD,  NMLK|KMOD|R,  SENDSTR("\033Ot")   },
	{ XK_KP_5,          KPAD,  NMLK|KMOD|R,  SENDSTR("\033Ou")   },
	{ XK_KP_6,          KPAD,  NMLK|KMOD|R,  SENDSTR("\033Ov")   },
	{ XK_KP_7,          KPAD,  NMLK|KMOD|R,  SENDSTR("\033Ow")   },
	{ XK_KP_8,          KPAD,  NMLK|KMOD|R,  SENDSTR("\033Ox")   },
	{ XK_KP_9,          KPAD,  NMLK|KMOD|R,  SENDSTR("\033Oy")   },

	/* Function */
	{ XK_F1,   0,  R,  SENDTILDE(11) },
	{ XK_F2,   0,  R,  SENDTILDE(12) },
	{ XK_F3,   0,  R,  SENDTILDE(13) },
	{ XK_F4,   0,  R,  SENDTILDE(14) },
	{ XK_F5,   0,  R,  SENDTILDE(15) },
	{ XK_F6,   0,  R,  SENDTILDE(17) },
	{ XK_F7,   0,  R,  SENDTILDE(18) },
	{ XK_F8,   0,  R,  SENDTILDE(19) },
	{ XK_F9,   0,  R,  SENDTILDE(20) },
	{ XK_F10,  0,  R,  SENDTILDE(21) },
	{ XK_F11,  0,  R,  SENDTILDE(23) },
	{ XK_F12,  0,  R,  SENDTILDE(24) },
	{ XK_F13,  0,  R,  SENDTILDE(25) },
	{ XK_F14,  0,  R,  SENDTILDE(26) },
	{ XK_F15,  0,  R,  SENDTILDE(28) },
	{ XK_F16,  0,  R,  SENDTILDE(29) },
	{ XK_F17,  0,  R,  SENDTILDE(31) },
	{ XK_F18,  0,  R,  SENDTILDE(32) },
	{ XK_F19,  0,  R,  SENDTILDE(33) },
	{ XK_F20,  0,  R,  SENDTILDE(34) },
	/* libtermkey only recognizes up to F20. */

	{ 0 },
};

SelType seltypes[] = {
	{ SEL_RECTANGULAR, A, KEXCL(A) },
	/* If no match is found, regular selection is used. */
	{ 0 },
};

uint
confstate(uint xstate, int rels)
{
	uint winmode;

	winmode = xgetmode();
	return (winmode&MODE_APPCURSOR ? CURS : 0)
		| (winmode&MODE_APPKEYPAD ? KPAD : 0)
		| (winmode&MODE_NUMLOCK ? NMLK : 0)
		| (rels ? RELS : 0)
		| (xstate << MODOFFS);
}

void
clipcopy(uint state, Arg arg)
{ xclipcopy(); }

void
clippaste(uint state, Arg arg)
{ xclippaste(); }

void
selpaste(uint state, Arg arg)
{ xselpaste(); }

void
zoomrel(uint state, Arg arg)
{ xzoomrel(arg.d); }

void
zoomrst(uint state, Arg arg)
{ xzoomrst(); }

void
numlock(uint state, Arg arg)
{ xtogmode(MODE_NUMLOCK); }

void
sendstr(uint state, Arg arg)
{ ttywrite(arg.str.s, arg.str.l, 1); }

void
sendcsi(uint state, Arg arg)
{
	char buf[64];
	size_t len;

	len = csienc(buf, sizeof buf, state, arg.csi.n, arg.csi.m, arg.csi.c);
	ttywrite(buf, len, 1);
}

void
printscreen(uint state, Arg arg)
{ tdump(); }

void
selprint(uint state, Arg arg)
{ tdumpsel(); }

void
sendbreak(uint state, Arg arg)
{ tsendbreak(); }

void
togprinter(uint state, Arg arg)
{ ttogprinter(); }
