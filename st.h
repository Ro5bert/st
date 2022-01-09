/* See LICENSE for license details. */
/* Requires: stdint.h, size_t */

/* TODO: replace all ints used as bools with bool from stdbool. Looking in git
 * history, st used to use bools, but someone replaced all of them with ints
 * without real justification. The benefit in code clearity with bools instead
 * of ints is significant. */
/* TODO unicode input with C-S-u */
/* TODO write/find a script to test control sequences */
/* TODO test alignment by changing tab size and checking nothing is fucked */
/* TODO rewrite text rendering? see suckless libdraw in dwm/dmenu */
/* TODO rename x-related functions */


#define UTF_INVALID 0xFFFD
#define UTF_SIZ     4


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


typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

typedef uint_least32_t Rune;

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
} XWindow;


/* TODO temporary */
#include <X11/Xlib.h>
#include "config.h" 


/* st.c */
extern char *opt_class;
extern char **opt_cmd;
extern char *opt_embed;
extern char *opt_font;
extern char *opt_geom;
extern int opt_fixed;
extern char *opt_io;
extern char *opt_line;
extern char *opt_name;
extern char *opt_title;


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
int tstart(void);


/* win.c */
/* TODO temp; these shouldn't be public probably */
extern TermWindow win;
extern XWindow xw;
extern XSelection xsel;
extern void (*handler[LASTEvent])(XEvent *);
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
void xinit(int, int);
int xstart(void);
