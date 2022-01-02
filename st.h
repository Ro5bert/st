/* See LICENSE for license details. */
/* Requires: stdint.h, size_t */


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

enum selection_mode {
	SEL_IDLE = 0,
	SEL_EMPTY = 1,
	SEL_READY = 2,
};

enum selection_type {
	SEL_REGULAR = 1,
	SEL_RECTANGULAR = 2,
};

enum selection_snap {
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

enum win_mode {
	MODE_VISIBLE     = 1 << 0,
	MODE_FOCUSED     = 1 << 1,
	MODE_APPKEYPAD   = 1 << 2,
	MODE_MOUSEBTN    = 1 << 3,
	MODE_MOUSEMOTION = 1 << 4,
	MODE_REVERSE     = 1 << 5,
	MODE_KBDLOCK     = 1 << 6,
	MODE_HIDE        = 1 << 7,
	MODE_APPCURSOR   = 1 << 8,
	MODE_MOUSESGR    = 1 << 9,
	MODE_8BIT        = 1 << 10,
	MODE_BLINK       = 1 << 11,
	MODE_FBLINK      = 1 << 12,
	MODE_FOCUS       = 1 << 13,
	MODE_MOUSEX10    = 1 << 14,
	MODE_MOUSEMANY   = 1 << 15,
	MODE_BRCKTPASTE  = 1 << 16,
	MODE_NUMLOCK     = 1 << 17,
	MODE_MOUSE       = MODE_MOUSEBTN | MODE_MOUSEMOTION | MODE_MOUSEX10
	                   | MODE_MOUSEMANY,
};


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
void *xmalloc(size_t);
void *xrealloc(void *, size_t);
char *xstrdup(const char *);
ssize_t xwrite(int, const char *, size_t);
size_t utf8dec(const char *, Rune *, size_t);
size_t utf8enc(Rune, char *);
char *base64dec(const char *);
size_t csienc(char *, size_t, uint, uint, uint, char);


/* tty.c */
void selinit(void);
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
void tinit(int, int);
void tresize(int, int);
void tsetdirtattr(int);
void ttyhangup(void);
int ttynew(const char *, char *, const char *, char **);
size_t ttyread(void);
void ttyresize(int, int);
void ttywrite(const char *, size_t, int);


/* win.c */
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
uint xmode(uint, uint);
#define xgetmode() xmode(0,0)
#define xsetmode(set,flags) ((set) ? xmode((flags),0) : xmode(0,(flags)))
#define xtogmode(flags) xmode((flags),(flags))
void xsetpointermotion(int);
void xsetsel(char *);
int xstartdraw(void);
void xximspot(int, int);
void xinit(int, int);
void xmain(void);
