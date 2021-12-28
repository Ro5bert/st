/* See LICENSE file for license details. */
/* Requires: wchar.h, X11/X.h, util.h */

#define CURS (1<<0) /* Application cursor mode */
#define KPAD (1<<1) /* Application keypad mode */
#define NMLK (1<<2) /* Num lock */
#define RELS (1<<3) /* Key/buttom release */
/* To allow checking more boolean properties when matching key/button events,
 * define them here and modify MODOFFS and confstate. */
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

typedef void (*Handler)(uint state, Arg arg);

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

extern char *font;
extern int borderpx;
extern float cwscale;
extern float chscale;
extern char *shell;
extern char *utmp;
extern char *scroll;
extern char *stty_args;
extern char *vtiden;
extern wchar_t *worddelimiters;
extern uint doubleclicktimeout;
extern uint tripleclicktimeout;
extern int allowaltscreen;
extern int allowwindowops;
extern double minlatency;
extern double maxlatency;
extern uint blinktimeout;
extern uint cursorthickness;
extern int bellvolume;
extern char *termname;
extern uint tabspaces;
extern const char *colorname[];
extern uint defaultfg;
extern uint defaultbg;
extern uint defaultcs;
extern uint defaultrcs;
extern uint cursorshape;
extern uint cols;
extern uint rows;
extern uint mouseshape;
extern uint mousefg;
extern uint mousebg;
extern uint defaultattr;
extern uint ignoremod;
extern Btn btns[];
extern Key keys[];
extern SelType seltypes[];

uint confstate(uint, int);
