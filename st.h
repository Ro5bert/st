/* See LICENSE for license details. */
/* Requires: stdint.h, size_t */

/* TODO replace all ints used as bools with bool from stdbool. Looking in git
 * history, st used to use bools, but someone replaced all of them with ints
 * without real justification. The benefit in code clearity with bools instead
 * of ints is significant. */
/* TODO unicode input with C-S-u */
/* TODO write/find a script to test control sequences */
/* TODO test alignment by changing tab size and checking nothing is fucked */
/* TODO rewrite text rendering? see suckless libdraw in dwm/dmenu */
/* TODO rename x-related functions */
/* TODO remove const */
/* TODO replace fprintf stderr calls with errlog function */
/* TODO use consistent terminology: the keyboard cursor is the "cursor" and
 * the mouse pointer is the "pointer" */
/* TODO it would be cool if the terminal stuff was completly independent of the
 * X stuff and could be instantiated multiple times (so no global variables).
 * This would allow, e.g., using the terminal code as a general terminal
 * backend that we could use for a graphical terminal emulator, a terminal
 * multiplexer like tmux/screen/dvtm, etc. */


#define UTF_INVALID 0xFFFD
#define UTF_SIZ     4


/* State used to match key/button/motion events. */
#define CURS (1<<0) /* Application cursor mode */
#define KPAD (1<<1) /* Application keypad mode */
#define NMLK (1<<2) /* Num lock */
#define RELS (1<<3) /* Key/button release; always 0 for motion events */
/* To allow checking more boolean properties when matching key/button events,
 * define them above, increment MODOFFS, and modify evtctx as needed. */
#define MODOFFS 4
#define SHFT (ShiftMask<<MODOFFS)
#define CTRL (ControlMask<<MODOFFS)
#define ALT  (Mod1Mask<<MODOFFS)
#define BTN1 (Button1Mask<<MODOFFS)
#define BTN2 (Button2Mask<<MODOFFS)
#define BTN3 (Button3Mask<<MODOFFS)
#define BTN4 (Button4Mask<<MODOFFS)
#define BTN5 (Button5Mask<<MODOFFS)
#define KMOD (SHFT|CTRL|ALT)
#define KEXCL(m) (KMOD&~(m))


#define MIN(a, b)              ((a) < (b) ? (a) : (b))
#define MAX(a, b)              ((a) < (b) ? (b) : (a))
#define LEN(a)                 (sizeof(a) / sizeof(a)[0])
#define BETWEEN(x, a, b)       ((a) <= (x) && (x) <= (b))
#define DIVCEIL(n, d)          (((n) + ((d) - 1)) / (d))
#define DEFAULT(a, b)          (a) = (a) ? (a) : (b)
#define LIMIT(x, a, b)         (x) = (x) < (a) ? (a) : (x) > (b) ? (b) : (x)
#define TIMEDIFF(t1, t2)       ((t1.tv_sec-t2.tv_sec)*1000 + \
                                (t1.tv_nsec-t2.tv_nsec)/1E6)
#define MODBIT(x, set, bit)    ((set) ? ((x) |= (bit)) : ((x) &= ~(bit)))
#define MATCH(state, set, clr) ((((set) & ~(state)) | ((clr) & (state))) == 0)
#define TRUECOLOR(r,g,b)       (1 << 24 | (r) << 16 | (g) << 8 | (b))
#define IS_TRUECOLOR(x)        (1 << 24 & (x))


typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned int   uint;
typedef unsigned long  ulong;

typedef int_least8_t  i8;
typedef int_least16_t i16;
typedef int_least32_t i32;
typedef int_least64_t i64;

typedef uint_least8_t  u8;
typedef uint_least16_t u16;
typedef uint_least32_t u32;
typedef uint_least64_t u64;

typedef u32 Rune;

typedef union {
	int i;
	uint u;
	double d;
	const void *v;
	struct {
		uint  l; /* Length of s, excluding null */
		const char *s;
	} str;
	struct {
		uint n; /* First parameter; ignored if 0 or 1 */
		uint m; /* State bits to ignore when encoding as CSI */
		char c;
	} csi;
} Arg;
#define ARG_STR(s_)       { .str.l = sizeof(s_)-1, .str.s = (s_) }
#define ARG_CSI(n_,m_,c_) { .csi.n = (n_), .csi.m = (m_), .csi.c = (c_) }
#define ARG_DUMMY         { .i = 0 }

typedef struct {
	/* TODO use fixed size ints; we need a certain number of bits */
	uint m;   /* boolean properties, particularly keyboard modifiers */
	int x, y; /* cursor position */
} EvtCtx;

typedef void (*Handler)(Arg, EvtCtx);

/* Tristate logic: each bit in set/clr is interpreted as follows:
 * set  clr
 *  0    0    don't care
 *  0    1    bit must be clear
 *  1    0    bit must be set
 *  1    1    unsatisfiable (i.e., useless configuration) */

typedef struct {
	uint btn;
	uint set, clr;
	Handler fn;
	Arg arg;
} Btn;

typedef struct {
	KeySym sym;
	uint set, clr;
	Handler fn;
	Arg arg;
} Key;

typedef struct {
	int type;
	uint set, clr;
} SelType;

enum glyph_attribute {
	ATTR_NULL       = 0,
	ATTR_BOLD       = 1 << 0,
	ATTR_FAINT      = 1 << 1,
	ATTR_ITALIC     = 1 << 2,
	ATTR_UNDERLINE  = 1 << 3,
	ATTR_BLINK      = 1 << 4,
	ATTR_REVERSE    = 1 << 5,
	ATTR_INVISIBLE  = 1 << 6,
	ATTR_STRUCK     = 1 << 7,
	ATTR_WRAP       = 1 << 8,
	ATTR_WIDE       = 1 << 9,
	ATTR_WDUMMY     = 1 << 10,
	ATTR_BOLD_FAINT = ATTR_BOLD | ATTR_FAINT,
};

enum selection_type {
	SEL_REGULAR = 1,
	SEL_RECTANGULAR = 2,
};

enum selection_snap {
	SEL_SNAP_CHAR = 0,
	SEL_SNAP_WORD = 1,
	SEL_SNAP_LINE = 2,
};

#define Glyph Glyph_
typedef struct {
	Rune u;      /* character code */
	ushort mode; /* attribute flags */
	uint32_t fg; /* foreground */
	uint32_t bg; /* background */
} Glyph;

typedef Glyph *Line;

enum mouse_report_mode {
	MOUSE_NONE = 0,
	MOUSE_X10,    /* Report button num and location on button presses */
	MOUSE_BUTTON, /* Report button num, location, and keyboard modifiers on
	                 button presses and releases */
	MOUSE_MOTION, /* Like MOUSE_BUTTON, except motion with a button held is
	                 also reported */
	MOUSE_MANY,   /* Like MOUSE_MOTION, except motion is reported even without
	                 a button held */
};

typedef struct {
	Atom xtarget;
	char *primary, *clipboard;
	struct timespec tclick1;
	struct timespec tclick2;
} XSelection;

typedef struct {
	int tw, th; /* tty width and height */
	int w, h; /* window width and height */
	int ch; /* char height */
	int cw; /* char width  */
	int cursor; /* cursor style */
	int prevx, prevy; /* last cell moused over */
	uint btns; /* bit mask of pressed btns */
	uint visible:1;
	uint focused:1;
	uint blink:1; /* blinking glyphs currently not visible */
	uint appcursor:1; /* app keypad mode; changes some encodings */
	uint appkeypad:1; /* app cursor mode; changes some encodings */
	uint reversevid:1;
	uint kbdlock:1;
	uint hidecursor:1;
	uint reportfocus:1; /* report X focus events to terminal client */
	uint bracketpaste:1; /* pasted text is "bracketed" with CSI seqs */
	uint numlock:1;
	uint mousemode:3; /* see enum mouse_report_mode */
	uint mousesgr:1; /* mouse reporting done in saner format */
} TermWindow;

typedef struct {
	Display *dpy;
	Colormap cmap;
	Window win;
	Drawable buf;
	XftGlyphFontSpec *specbuf; /* font spec buffer used for rendering */
	Atom xembed, wmdeletewin, netwmname, netwmiconname, netwmpid;
	struct {
		XIM xim;
		XIC xic;
		XPoint spot;
		XVaNestedList spotlist;
	} ime;
	XftDraw *draw;
	Visual *vis;
	XSetWindowAttributes attrs; /* TODO do we really need to save this? i think not */
	int scr;
	int isfixed; /* is fixed geometry? */
	int l, t; /* left and top offset */
	int gm; /* geometry mask */
	char *title;
	char *name;
} XWindow;


/* st.c */
extern char **opt_cmd;
extern char *opt_embed;
extern char *opt_geom;
extern int opt_fixed;
extern char *opt_io;
extern char *opt_line;

extern char *defaultfont;
extern char *defaultshell;
extern uint defaultcols;
extern uint defaultrows;
extern double minlatency;
extern double maxlatency;
extern double blinktimeout;
extern char *termname;
extern char *defaulttitle;


/* util.c */
void die(const char *, ...);
void *emalloc(size_t);
void *erealloc(void *, size_t);
char *estrdup(const char *);
ssize_t ewrite(int, const char *, size_t);
size_t utf8dec(const char *, Rune *, size_t);
size_t utf8enc(Rune, char *);
char *base64dec(const char *);
size_t csienc(char *, size_t, uint, uint, uint, char);


/* tty.c */
extern char *stty_args;
extern char *vtiden;
extern wchar_t *worddelimiters;
extern int allowwindowops;
extern uint tabspaces;

void selinit(void); /* TODO needn't be in here */
void selclear(void);
void selstart(int, int, int);
void selextend(int, int, int, int);
int selcontains(int, int);
char *seltext(void);

void draw(void);
void redraw(void);

void ttogprinter(void);
void tdump(void);
void tdumpsel(void);
void tsendbreak(void);
int tattrset(int);
void tresize(int, int);
void tsetdirtattr(int);
void ttyhangup(void);
int ttynew(const char *, char *, const char *, char **);
size_t ttyread(void);
void ttyresize(int, int);
void ttywrite(const char *, size_t, int);
void tinit(int, int);
int tstart(char *);


/* win.c */
extern void (*handler[LASTEvent])(XEvent *);

extern int borderpx;
extern float cwscale;
extern float chscale;
extern uint doubleclicktimeout;
extern uint tripleclicktimeout;
extern uint cursorthickness;
extern int bellvolume;
extern uint mouseshape;
extern uint mousefg;
extern uint mousebg;
extern uint defaultattr;
extern const char *colorname[];
extern uint defaultfg;
extern uint defaultbg;
extern uint defaultcs;
extern uint defaultrcs;
extern uint cursorshape;

/* TODO temp; these shouldn't be public probably */
extern TermWindow win;
extern XWindow xw;
extern XSelection xsel;

extern Btn btns[];
extern Key keys[];
extern SelType seltypes[];

EvtCtx evtctx(uint xstate, int rels, int x, int y);
int xtocol(int);
int ytorow(int);
void mousesel(EvtCtx, int);
void xclipcopy(void);
void xclippaste(void);
void xselpaste(void);
void xzoomabs(double);
void xzoomrel(double);
void xzoomrst(void);
void xbell(void);
void xclipcopy(void);
void xdrawcursor(int, int, Glyph, int, int, Glyph);
void xdrawline(Line, int, int, int);
void xfinishdraw(void);
void xloadcolors(void);
int xsetcolorname(int, const char *);
void xseticontitle(char *);
void xsettitle(char *);
int xsetcursor(int);
void xsetpointermotion(int);
void xsetsel(char *);
int xstartdraw(void);
void xximspot(int, int);
void xinit(int, int, char *, char *, char *);
int xstart(void);
