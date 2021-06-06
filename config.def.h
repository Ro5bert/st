/* See LICENSE file for copyright and license details. */

/*
 * appearance
 *
 * font: see http://freedesktop.org/software/fontconfig/fontconfig-user.html
 */
static char *font = "Fira Mono:pixelsize=18:antialias=true:autohint=true";
static int borderpx = 2;

/*
 * What program is execed by st depends of these precedence rules:
 * 1: program passed with -e
 * 2: scroll and/or utmp
 * 3: SHELL environment variable
 * 4: value of shell in /etc/passwd
 * 5: value of shell in config.h
 */
static char *shell = "/bin/sh";
char *utmp = NULL;
/* scroll program: to enable use a string like "scroll" */
char *scroll = NULL;
char *stty_args = "stty raw pass8 nl -echo -iexten -cstopb 38400";

/* identification sequence returned in DA and DECID */
char *vtiden = "\033[?6c";

/* Kerning / character bounding-box multipliers */
static float cwscale = 1.0;
static float chscale = 1.0;

/*
 * word delimiter string
 *
 * More advanced example: L" `'\"()[]{}"
 */
wchar_t *worddelimiters = L" ";

/* selection timeouts (in milliseconds) */
static unsigned int doubleclicktimeout = 300;
static unsigned int tripleclicktimeout = 600;

/* alt screens */
int allowaltscreen = 1;

/* allow certain non-interactive (insecure) window operations such as:
   setting the clipboard text */
int allowwindowops = 0;

/*
 * draw latency range in ms - from new content/keypress/etc until drawing.
 * within this range, st draws when content stops arriving (idle). mostly it's
 * near minlatency, but it waits longer for slow updates to avoid partial draw.
 * low minlatency will tear/flicker more, as it can "detect" idle too early.
 */
static double minlatency = 8;
static double maxlatency = 33;

/*
 * blinking timeout (set to 0 to disable blinking) for the terminal blinking
 * attribute.
 */
static unsigned int blinktimeout = 800;

/*
 * thickness of underline and bar cursors
 */
static unsigned int cursorthickness = 2;

/*
 * bell volume. It must be a value between -100 and 100. Use 0 for disabling
 * it
 */
static int bellvolume = 0;

/* default TERM value */
char *termname = "st-256color";

/*
 * spaces per tab
 *
 * When you are changing this value, don't forget to adapt the »it« value in
 * the st.info and appropriately install the st.info in the environment where
 * you use this st version.
 *
 *	it#$tabspaces,
 *
 * Secondly make sure your kernel is not expanding tabs. When running `stty
 * -a` »tab0« should appear. You can tell the terminal to not expand tabs by
 *  running following command:
 *
 *	stty tabs
 */
unsigned int tabspaces = 4;

/* Terminal colors (16 first used in escape sequence) */
static const char *colorname[] = {
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

	/* more colors can be added after 255 to use with DefaultXX */
	"#cccccc",
	"#555555",
};


/*
 * Default colors (colorname index)
 * foreground, background, cursor, reverse cursor
 */
unsigned int defaultfg = 7;
unsigned int defaultbg = 0;
static unsigned int defaultcs = 256;
static unsigned int defaultrcs = 257;

/*
 * Default shape of cursor
 * 2: Block ("█")
 * 4: Underline ("_")
 * 6: Bar ("|")
 * 7: Snowman ("☃")
 */
static unsigned int cursorshape = 2;

/*
 * Default columns and rows numbers
 */

static unsigned int cols = 80;
static unsigned int rows = 24;

/*
 * Default colour and shape of the mouse cursor
 */
static unsigned int mouseshape = XC_xterm;
static unsigned int mousefg = 7;
static unsigned int mousebg = 0;

/*
 * Color used to display font attributes when fontconfig selected a font which
 * doesn't match the ones requested.
 */
static unsigned int defaultattr = 11;

/*
 * Force mouse select/shortcuts while mask is active (when MODE_MOUSE is set).
 * Note that if you want to use ShiftMask with selmasks, set this to an other
 * modifier, set to 0 to not use it.
 */
static uint forcemousemod = ShiftMask;

/*
 * Internal mouse shortcuts.
 * Beware that overloading Button1 will disable the selection.
 */
static MouseShortcut mshortcuts[] = {
	/* mask                 button   function        argument       release */
	{ XK_ANY_MOD,           Button2, selpaste,       {.i = 0},      1 },
	{ ShiftMask,            Button4, ttysend,        {.s = "\033[5;2~"} },
	{ XK_ANY_MOD,           Button4, ttysend,        {.s = "\031"} },
	{ ShiftMask,            Button5, ttysend,        {.s = "\033[6;2~"} },
	{ XK_ANY_MOD,           Button5, ttysend,        {.s = "\005"} },
};

#define TERMMOD (ControlMask|ShiftMask)
static Shortcut shortcuts[] = {
	/* mask                 keysym          function        argument */
	{ XK_ANY_MOD,           XK_Break,       sendbreak,      {.i =  0} },
	{ ControlMask,          XK_Print,       toggleprinter,  {.i =  0} },
	{ ShiftMask,            XK_Print,       printscreen,    {.i =  0} },
	{ XK_ANY_MOD,           XK_Print,       printsel,       {.i =  0} },
	{ TERMMOD,              XK_Prior,       zoom,           {.f = +1} },
	{ TERMMOD,              XK_Next,        zoom,           {.f = -1} },
	{ TERMMOD,              XK_Home,        zoomreset,      {.f =  0} },
	{ TERMMOD,              XK_C,           clipcopy,       {.i =  0} },
	{ TERMMOD,              XK_V,           clippaste,      {.i =  0} },
	{ TERMMOD,              XK_Y,           selpaste,       {.i =  0} },
	{ ShiftMask,            XK_Insert,      selpaste,       {.i =  0} },
	{ TERMMOD,              XK_Num_Lock,    numlock,        {.i =  0} },
};

/*
 * State bits to ignore when matching key or button events.  By default,
 * numlock (Mod2Mask) and keyboard layout (XK_SWITCH_MOD) are ignored.
 */
static uint ignoremod = Mod2Mask|XK_SWITCH_MOD;

#define CURS (1<<0)
#define KPAD (1<<1)
#define NMLK (1<<2)
#define MODOFFS 3
#define S    (1<<3)
#define A    (1<<4)
#define C    (1<<5)
#define ALLM (S|A|C)

#define STR(s_) kencstr, .arg.str.l = sizeof(s_)-1, .arg.str.s = (s_)
#define CSI(n_,m_,c_) kenccsi, .arg.csi.n = (n_), .arg.csi.m = (m_), .arg.csi.c = (c_)
#define TILDE(n) CSI((n),0,'~')
#define UNICODE(cp) CSI((cp),S,'u')

typedef union {
	struct {
		uint l;
		char *s;
	} str;
	struct {
		uint n;
		uchar m;
		char c;
	} csi;
} KeyArg;

typedef int (*KeyEncoder)(char *buf, size_t len,
		KeySym sym, uint mod, KeyArg arg);

typedef struct {
	/* Tristate logic. Each bit in set/clr is interpreted as follows:
	 * set  clr
	 *  0    0    don't care
	 *  0    1    bit must be clear
	 *  1    0    bit must be set
	 *  1    1    unsatisfiable (i.e., useless configuration) */
	KeySym sym;
	uint set, clr;
	KeyEncoder fn;
	KeyArg arg;
} Key;

int
kencstr(char *buf, size_t len, KeySym sym, uint state, KeyArg arg)
{
	size_t i;

	for (i = 0; i < len-1 && i < arg.str.l; i++)
		buf[i] = arg.str.s[i];
	if (len > 0)
		buf[i] = '\0';
	return i;
}

int
kcsi(char *buf, size_t len, uint state, uint n, uchar m, char c)
{
	uint mod;

	mod = (state & ~m) >> MODOFFS;
	if (mod > 0)
		return snprintf(buf, len, "\033[%d;%d%c", n, mod+1, c);
	else if (n > 1)
		return snprintf(buf, len, "\033[%d%c", n, c);
	else
		return snprintf(buf, len, "\033[%c", c);
}

int
kenccsi(char *buf, size_t len, KeySym sym, uint state, KeyArg arg)
{
	kcsi(buf, len, state, arg.csi.n, arg.csi.m, arg.csi.c);
}

Key keys[] = {
	/* SHORTCUTS (must be first) */
	/* XK_Print */
	/* XK_Break */
	/* XK_Num_Lock */

	/* LATIN1 */
	{ XK_space,          C,  A|S,  STR("\0")     },
	{ XK_space,        C|A,    S,  STR("\033\0") },
	{ XK_at,             C,    0,  UNICODE('@')  }, /* '@'-64 is NUL */
	{ XK_O,              A,    0,  UNICODE('O')  }, /* ESC O  is SS3 */
	{ XK_i,              C,    0,  UNICODE('i')  }, /* 'i'-64 is tab */
	{ XK_m,              C,    0,  UNICODE('m')  }, /* 'm'-64 is CR  */
	{ XK_bracketleft,    C,    0,  UNICODE('[')  }, /* '['-64 is ESC */
	{ XK_bracketleft,    A,    0,  UNICODE('[')  }, /* ESC [  is CSI */

	/* MISC */
	{ XK_BackSpace,     0,  ALLM,  STR("\177")     },
	{ XK_BackSpace,     A,   C|S,  STR("\033\177") },
	{ XK_BackSpace,     0,     0,  CSI(127,0,'u')  },
	{ XK_Tab,           0,  ALLM,  STR("\t")       },
	{ XK_Tab,           A,   C|S,  STR("\033\t")   },
	{ XK_Tab,           S,     0,  CSI(1,S,'Z')    },
	{ XK_Tab,           0,     0,  CSI('\t',0,'u') },
	{ XK_Return,        0,  ALLM,  STR("\r")       },
	{ XK_Return,        A,   C|S,  STR("\033\r")   },
	{ XK_Return,        0,     0,  CSI('\r',0,'u') },
	{ XK_Escape,        0,  ALLM,  STR("\033")     },
	{ XK_Escape,        A,   C|S,  STR("\033\033") },
	{ XK_Escape,        0,     0,  CSI(27,0,'u')   },
	{ XK_Delete,        0,     0,  TILDE(3)        },
	{ XK_Home,       CURS,  ALLM,  STR("\033OH")   },
	{ XK_Home,          0,     0,  CSI(1,0,'H')    },
	{ XK_Left,       CURS,  ALLM,  STR("\033OD")   },
	{ XK_Left,          0,     0,  CSI(1,0,'D')    },
	{ XK_Up,         CURS,  ALLM,  STR("\033OA")   },
	{ XK_Up,            0,     0,  CSI(1,0,'A')    },
	{ XK_Right,      CURS,  ALLM,  STR("\033OC")   },
	{ XK_Right,         0,     0,  CSI(1,0,'C')    },
	{ XK_Down,       CURS,  ALLM,  STR("\033OB")   },
	{ XK_Down,          0,     0,  CSI(1,0,'B')    },
	{ XK_Prior,         0,     0,  TILDE(5)        },
	{ XK_Next,          0,     0,  TILDE(6)        },
	{ XK_End,        CURS,  ALLM,  STR("\033OF")   },
	{ XK_End,           0,     0,  CSI(1,0,'F')    },
	{ XK_Begin,         0,     0,  CSI(1,0,'E')    },
	{ XK_Select,        0,     0,  TILDE(4)        },
	{ XK_Insert,        0,     0,  TILDE(2)        },
	{ XK_Find,          0,     0,  TILDE(1)        },

	/* KEYPAD */
	{ XK_KP_Enter,      KPAD,  NMLK|ALLM,  STR("\033OM")   },
	{ XK_KP_Enter,         0,       ALLM,  STR("\r")       },
	{ XK_KP_Enter,         A,        C|S,  STR("\033\r")   },
	{ XK_KP_Enter,         0,          0,  CSI('\r',0,'u') },
	{ XK_KP_F1,            0,       ALLM,  STR("\033OP")   },
	{ XK_KP_F1,            0,          0,  CSI(1,0,'P')    },
	{ XK_KP_F2,            0,       ALLM,  STR("\033OQ")   },
	{ XK_KP_F2,            0,          0,  CSI(1,0,'Q')    },
	{ XK_KP_F3,            0,       ALLM,  STR("\033OR")   },
	{ XK_KP_F3,            0,          0,  CSI(1,0,'R')    },
	{ XK_KP_F4,            0,       ALLM,  STR("\033OS")   },
	{ XK_KP_F4,            0,          0,  CSI(1,0,'S')    },
	{ XK_KP_Home,       CURS,       ALLM,  STR("\033OH")   },
	{ XK_KP_Home,          0,          0,  CSI(1,0,'H')    },
	{ XK_KP_Left,       CURS,       ALLM,  STR("\033OD")   },
	{ XK_KP_Left,          0,          0,  CSI(1,0,'D')    },
	{ XK_KP_Up,         CURS,       ALLM,  STR("\033OA")   },
	{ XK_KP_Up,            0,          0,  CSI(1,0,'A')    },
	{ XK_KP_Right,      CURS,       ALLM,  STR("\033OC")   },
	{ XK_KP_Right,         0,          0,  CSI(1,0,'C')    },
	{ XK_KP_Down,       CURS,       ALLM,  STR("\033OB")   },
	{ XK_KP_Down,          0,          0,  CSI(1,0,'B')    },
	{ XK_KP_Prior,         0,          0,  TILDE(5)        },
	{ XK_KP_Next,          0,          0,  TILDE(6)        },
	{ XK_KP_End,        CURS,       ALLM,  STR("\033OF")   },
	{ XK_KP_End,           0,          0,  CSI(1,0,'F')    },
	{ XK_KP_Begin,         0,          0,  CSI(1,0,'E')    },
	{ XK_KP_Insert,        0,          0,  TILDE(2)        },
	{ XK_KP_Delete,        0,          0,  TILDE(3)        },
	{ XK_KP_Equal,      KPAD,  NMLK|ALLM,  STR("\033OX")   },
	{ XK_KP_Multiply,   KPAD,  NMLK|ALLM,  STR("\033Oj")   },
	{ XK_KP_Add,        KPAD,  NMLK|ALLM,  STR("\033Ok")   },
	{ XK_KP_Separator,  KPAD,  NMLK|ALLM,  STR("\033Ol")   },
	{ XK_KP_Subtract,   KPAD,  NMLK|ALLM,  STR("\033Om")   },
	{ XK_KP_Decimal,    KPAD,  NMLK|ALLM,  STR("\033On")   },
	{ XK_KP_Divide,     KPAD,  NMLK|ALLM,  STR("\033Oo")   },
	{ XK_KP_0,          KPAD,  NMLK|ALLM,  STR("\033Op")   },
	{ XK_KP_1,          KPAD,  NMLK|ALLM,  STR("\033Oq")   },
	{ XK_KP_2,          KPAD,  NMLK|ALLM,  STR("\033Or")   },
	{ XK_KP_3,          KPAD,  NMLK|ALLM,  STR("\033Os")   },
	{ XK_KP_4,          KPAD,  NMLK|ALLM,  STR("\033Ot")   },
	{ XK_KP_5,          KPAD,  NMLK|ALLM,  STR("\033Ou")   },
	{ XK_KP_6,          KPAD,  NMLK|ALLM,  STR("\033Ov")   },
	{ XK_KP_7,          KPAD,  NMLK|ALLM,  STR("\033Ow")   },
	{ XK_KP_8,          KPAD,  NMLK|ALLM,  STR("\033Ox")   },
	{ XK_KP_9,          KPAD,  NMLK|ALLM,  STR("\033Oy")   },

	/* FUNCTION */
	{ XK_F1,   0,  0,  TILDE(11) },
	{ XK_F2,   0,  0,  TILDE(12) },
	{ XK_F3,   0,  0,  TILDE(13) },
	{ XK_F4,   0,  0,  TILDE(14) },
	{ XK_F5,   0,  0,  TILDE(15) },
	{ XK_F6,   0,  0,  TILDE(17) },
	{ XK_F7,   0,  0,  TILDE(18) },
	{ XK_F8,   0,  0,  TILDE(19) },
	{ XK_F9,   0,  0,  TILDE(20) },
	{ XK_F10,  0,  0,  TILDE(21) },
	{ XK_F11,  0,  0,  TILDE(23) },
	{ XK_F12,  0,  0,  TILDE(24) },
	{ XK_F13,  0,  0,  TILDE(25) },
	{ XK_F14,  0,  0,  TILDE(26) },
	{ XK_F15,  0,  0,  TILDE(28) },
	{ XK_F16,  0,  0,  TILDE(29) },
	{ XK_F17,  0,  0,  TILDE(31) },
	{ XK_F18,  0,  0,  TILDE(32) },
	{ XK_F19,  0,  0,  TILDE(33) },
	{ XK_F20,  0,  0,  TILDE(34) },
};

/*
 * Selection types' masks.
 * Use the same masks as usual.
 * Button1Mask is always unset, to make masks match between ButtonPress.
 * ButtonRelease and MotionNotify.
 * If no match is found, regular selection is used.
 */
static uint selmasks[] = {
	[SEL_RECTANGULAR] = Mod1Mask,
};

/*
 * Printable characters in ASCII, used to estimate the advance width
 * of single wide characters.
 */
static char ascii_printable[] =
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmnopqrstuvwxyz{|}~";
