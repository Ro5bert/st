/* See LICENSE for license details. */
/* Requires: stdint.h, size_t, util.h */

#define ATTRCMP(a, b) ((a).mode != (b).mode || (a).fg != (b).fg || \
                       (a).bg != (b).bg)
#define TRUECOLOR(r,g,b) (1 << 24 | (r) << 16 | (g) << 8 | (b))
#define IS_TRUECOL(x)    (1 << 24 & (x))

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
	SEL_READY = 2
};

enum selection_type {
	SEL_REGULAR = 1,
	SEL_RECTANGULAR = 2
};

enum selection_snap {
	SNAP_WORD = 1,
	SNAP_LINE = 2
};

#define Glyph Glyph_
typedef struct {
	Rune u;      /* character code */
	ushort mode; /* attribute flags */
	uint32_t fg; /* foreground  */
	uint32_t bg; /* background  */
} Glyph;

typedef Glyph *Line;

void redraw(void);
void draw(void);

void ttogprinter(void);
void tdump(void);
void tdumpsel(void);
void tsendbreak(void);
int tattrset(int);
void tnew(int, int);
void tresize(int, int);
void tsetdirtattr(int);
void ttyhangup(void);
int ttynew(const char *, char *, const char *, char **);
size_t ttyread(void);
void ttyresize(int, int);
void ttywrite(const char *, size_t, int);

void selclear(void);
void selinit(void);
void selstart(int, int, int);
void selextend(int, int, int, int);
int selected(int, int);
char *getsel(void);
