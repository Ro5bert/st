/* See LICENSE for license details. */

#include <errno.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/XKBlib.h>

#include "st.h"

/* XEMBED messages */
#define XEMBED_FOCUS_IN  4
#define XEMBED_FOCUS_OUT 5

#define IS_SET(flag) ((win.mode & (flag)) != 0)
#define TRUERED(x)   (((x) & 0xff0000) >> 8)
#define TRUEGREEN(x) (((x) & 0xff00))
#define TRUEBLUE(x)  (((x) & 0xff) << 8)

/* Font structure */
#define Font Font_
typedef struct {
	int height;
	int width;
	int ascent;
	int descent;
	int badslant;
	int badweight;
	short lbearing;
	short rbearing;
	XftFont *match;
	FcFontSet *set;
	FcPattern *pattern;
} Font;

/* Drawing Context */
typedef struct {
	XftColor *col;
	size_t collen;
	Font font, bfont, ifont, ibfont;
	GC gc;
} DC;

/* TODO: separate functions that draw selection from those that inform X of
 * selected text */
static inline int glyphattrcmp(Glyph, Glyph);
static inline ushort sixd_to_16bit(int);
static int xmakeglyphfontspecs(XftGlyphFontSpec *, const Glyph *, int, int, int);
static void xdrawglyphfontspecs(const XftGlyphFontSpec *, Glyph, int, int, int);
static void xdrawglyph(Glyph, int, int);
static void xclear(int, int, int, int);
static int xgeommasktogravity(int);
static int ximopen(Display *);
static void ximinstantiate(Display *, XPointer, XPointer);
static void ximdestroy(XIM, XPointer, XPointer);
static int xicdestroy(XIC, XPointer, XPointer);
static void cresize(int, int);
static void xresize(int, int);
static void xhints(void);
static int xloadcolor(int, const char *, XftColor *);
static int xloadfont(Font *, FcPattern *);
static void xloadfonts(const char *, double);
static void xunloadfont(Font *);
static void xunloadfonts(void);
static void xsetenv(void);
static void xseturgency(int);

static int dosym(KeySym, EvtCtx);
static int dobutton(uint, EvtCtx);

/* X event handlers */
/* TODO the definitions of these have inconsistencies: some call the XEvent
 * e; some call it ev; let's change all of them to evt. */
static void keyaction(XKeyEvent *, int);
static void keypress(XEvent *);
static void keyrelease(XEvent *);
static void buttonaction(XButtonEvent *, int);
static void buttonpress(XEvent *);
static void buttonrelease(XEvent *);
static void motionnotify(XEvent *);
static void focusin(XEvent *);
static void focusout(XEvent *);
static void expose(XEvent *);
static void visibilitynotify(XEvent *);
static void unmapnotify(XEvent *);
static void configurenotify(XEvent *);
static void propertynotify(XEvent *);
static void selectionclear(XEvent *);
static void selectionrequest(XEvent *);
static void selectionnotify(XEvent *);
static void clientmessage(XEvent *);

static void setsel(char *, Time);
static void mousereport(uint x, uint y, uint btn, int rel, uint state);

/* When adding a new event type to this table, the event_mask window attribute
 * in xinit needs to be updated with the appropriate mask bit(s). */
void (*handler[LASTEvent])(XEvent *) = {
	/* KeyPress (resp., KeyRelease) is generated when a keyboard key is pressed
	 * (resp., released). */
	[KeyPress] = keypress,
	[KeyRelease] = keyrelease,
	/* ButtonPress (resp., ButtonRelease) is generated when a mouse button is
	 * pressed (resp., released). */
	[ButtonPress] = buttonpress,
	[ButtonRelease] = buttonrelease,
	/* MotionNotify is generated when the mouse cursor moves. We only receive
	 * this event when a mouse button is held down, and this event is used to
	 * implement text selections. */
	[MotionNotify] = motionnotify,
	/* FocusIn (resp., FocusOut) is generated when the window acquires (resp.,
	 * loses) keyboard focus. (Only one window is said to have keyboard focus;
	 * this is the window that keyboard events are sent to.) */
	[FocusIn] = focusin,
	[FocusOut] = focusout,
	/* Expose is generated when part of the window has been exposed, indicating
	 * that part of the window needs to be redrawn. */
	[Expose] = expose,
	/* VisibilityNotify is generated when visibility of window changes between
	 * VisibilityUnobscured, VisibilityPartiallyObscured, and
	 * VisibilityFullyObscured (which mean what you think). */
	[VisibilityNotify] = visibilitynotify,
	/* UnmapNotify is generated when the window is "unmapped" from a screen,
	 * which in particular indicates that it is no longer visible. */
	[UnmapNotify] = unmapnotify,
	/* ConfigureNotify is generated when various changes to the window's state,
	 * like its size, occur. */
	[ConfigureNotify] = configurenotify,
	/* PropertyNotify is generated when a window "property" is modified.
	 * (Properties are a general way to communicate information between
	 * applications.) We only reveive PropertyNotify events when there is some
	 * INCR transfer happening for the selection retrieval. (An INCR transfer
	 * is an incremental data transfer; these are used for large payloads.) */
	[PropertyNotify] = propertynotify,
	/* SelectionClear is generated when the window loses ownership of a
	 * "selection". (Selections are the primary mechanism in X for
	 * communication between X programs, which is needed for clipboard
	 * management, for instance. Note that external means of communication,
	 * like files or TCP/IP, do not generally suffice for communication between
	 * two X clients, since the clients do not generally know their relative
	 * locations! E.g., they could be running on different machines.) Uncomment
	 * if you want the selection to disappear when you select something
	 * different in another window. */
	/* [SelectionClear] = selectionclear, */
	/* SelectionRequest is generated when some window requests a selection that
	 * our window owns. */
	[SelectionRequest] = selectionrequest,
	/* SelectionNotify is generated in the process of transfering a selection.
	 * In particular, a selection owner sends a SelectionNotify to the
	 * requestor after a successful transfer. */
	[SelectionNotify] = selectionnotify,
	/* ClientMessage events are never generated automatically by the X server;
	 * they are for general client communication. We receive ClientMessage
	 * events to implement XEmbed. */
	[ClientMessage] = clientmessage,
};

/* Globals */
static DC dc;
XWindow xw;
XSelection xsel;
/* TODO why does this get to be called win, while the XWindow is xw? for
 * consistency, this should by tw or something */
TermWindow win;

/* XXX what part of this is a ring? usually a ring means a circular linked
 * list or array, but this is a growing array */
/* Font Ring Cache */
enum {
	FRC_NORMAL,
	FRC_ITALIC,
	FRC_BOLD,
	FRC_ITALICBOLD,
};

typedef struct {
	XftFont *font;
	int flags;
	Rune unicodep;
} Fontcache;

/* Fontcache is an array now. A new font will be appended to the array. */
static Fontcache *frc = NULL;
static int frclen = 0;
static int frccap = 0;
static char *usedfont = NULL;
static double usedfontsize = 0;
static double defaultfontsize = 0;

/* Printable characters in ASCII. Used to estimate the advance width
 * of single wide characters. */
static char ascii_printable[] =
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmnopqrstuvwxyz{|}~";

int
dosym(KeySym sym, EvtCtx ctx)
{
	Key *k;
	char buf[64]; /* big enough for CSI sequence */
	size_t len;

	/* 1. Custom handling from config */
	for (k = keys; k->sym; k++) {
		if (k->sym == sym && MATCH(ctx.m, k->set, k->clr)) {
			k->fn(k->arg, ctx);
			return 1;
		}
	}

	/* 2. Printable ASCII (some special cases are handled in the keys table) */
	if (0x20 <= sym && sym < 0x7f) {
		/* CTRL + [ALT +] non-lowercase-letter must be encoded as CSI
		 * XXX some of these should probably be mapped to C0 controls */
		if ((ctx.m&CTRL) > 0 && !('a' <= sym && sym <= 'z')) {
			len = csienc(buf, sizeof buf, ctx.m, sym, SHFT, 'u');
		} else {
			buf[0] = sym;
			len = 1;
			/* CTRL + lowercase letter can usually be handled by translating to
			 * the corresponding C0 control */
			if ((ctx.m&CTRL) > 0)
				buf[0] -= 0x60;
			/* ALT can usually be handled by appending ESC */
			if ((ctx.m&ALT) > 0) {
				buf[1] = buf[0];
				buf[0] = '\033';
				len = 2;
			}
		}
		ttywrite(buf, len, 1);
		return 1;
	}

	return 0;
}

void
keyaction(XKeyEvent *e, int rel)
{
	EvtCtx ctx;
	char buf[64]; /* big enough for CSI sequence */
	size_t len;
	KeySym sym;
	Status status;
	Rune c;

	if (win.kbdlock)
		return;

	ctx = evtctx(e->state, rel, e->x, e->y);

	sym = NoSymbol;
	if (xw.ime.xic)
		len = XmbLookupString(xw.ime.xic, e, buf, sizeof buf, &sym, &status);
	else
		len = XLookupString(e, buf, sizeof buf, &sym, NULL);

	/* 1. Sym  */
	if (sym != NoSymbol && dosym(sym, ctx))
		return;
	/* TODO: else dostring(buf, len, ctx); */

	if (len == 0) {
		/* Failed to do anything with sym, and there are no chars to send */
		return;
	}

	/* 2. Modified UTF8-encoded unicode */
	if ((ctx.m&KMOD) > 0 && utf8dec(buf, &c, len) == len && c != UTF_INVALID)
		len = csienc(buf, sizeof buf, ctx.m, c, SHFT, 'u');

	/* 3. Default to directly sending composed string from the input method
	 * (might not be UTF8; encoding is dependent on locale of input method) */

	ttywrite(buf, len, 1);
}

void
keypress(XEvent *e)
{
	keyaction(&e->xkey, 0);
}

void
keyrelease(XEvent *e)
{
	keyaction(&e->xkey, 1);
}

int
dobutton(uint btn, EvtCtx ctx)
{
	Btn *b;

	for (b = btns; b->btn; b++) {
		if (b->btn == btn && MATCH(ctx.m, b->set, b->clr)) {
			b->fn(b->arg, ctx);
			return 1;
		}
	}

	return 0;
}

void
buttonaction(XButtonEvent *e, int rel)
{
	uint btn = e->button;

	if (1 <= btn && btn <= 11)
		MODBIT(win.btns, !rel, 1 << (btn-1));

	if (win.mousemode != MOUSE_NONE)
		mousereport(xtocol(e->x), ytorow(e->y), btn, rel, e->state);
	else
		dobutton(btn, evtctx(e->state, rel, e->x, e->y));
}

void
buttonpress(XEvent *e)
{
	buttonaction(&e->xbutton, 0);
}

void
buttonrelease(XEvent *e)
{
	buttonaction(&e->xbutton, 1);
}

void
motionnotify(XEvent *e)
{
	XMotionEvent *ev = &e->xmotion;
	if (win.mousemode != MOUSE_NONE)
		mousereport(xtocol(ev->x), ytorow(ev->y), 0, 0, ev->state);
	else
		mousesel(evtctx(ev->state, 0, ev->x, ev->y), 0);
}

void
focusin(XEvent *ev)
{
	XFocusChangeEvent *e = &ev->xfocus;

	if (e->mode == NotifyGrab)
		return;

	if (xw.ime.xic)
		XSetICFocus(xw.ime.xic);
	win.focused = 1;
	xseturgency(0);
	if (win.reportfocus)
		ttywrite("\033[I", 3, 0);
}

void
focusout(XEvent *ev)
{
	XFocusChangeEvent *e = &ev->xfocus;

	if (e->mode == NotifyGrab)
		return;

	if (xw.ime.xic)
		XUnsetICFocus(xw.ime.xic);
	win.focused = 0;
	if (win.reportfocus)
		ttywrite("\033[O", 3, 0);
}

void
expose(XEvent *ev)
{
	redraw();
}

void
visibilitynotify(XEvent *ev)
{
	win.visible = ev->xvisibility.state != VisibilityFullyObscured;
}

void
unmapnotify(XEvent *ev)
{
	win.visible = 0;
}

void
configurenotify(XEvent *e)
{
	if (e->xconfigure.width == win.w && e->xconfigure.height == win.h)
		return;

	cresize(e->xconfigure.width, e->xconfigure.height);
}

void
propertynotify(XEvent *e)
{
	XPropertyEvent *xpev;
	Atom clipboard = XInternAtom(xw.dpy, "CLIPBOARD", 0);

	xpev = &e->xproperty;
	if (xpev->state == PropertyNewValue &&
			(xpev->atom == XA_PRIMARY || xpev->atom == clipboard))
		selectionnotify(e);
}

void
selectionclear(XEvent *e)
{
	selclear();
}

void
selectionrequest(XEvent *e)
{
	XSelectionRequestEvent *xsre;
	XSelectionEvent xev;
	Atom xa_targets, string, clipboard;
	char *seltext;

	xsre = (XSelectionRequestEvent *) e;
	xev.type = SelectionNotify;
	xev.requestor = xsre->requestor;
	xev.selection = xsre->selection;
	xev.target = xsre->target;
	xev.time = xsre->time;
	if (xsre->property == None)
		xsre->property = xsre->target;

	/* reject */
	xev.property = None;

	xa_targets = XInternAtom(xw.dpy, "TARGETS", 0);
	if (xsre->target == xa_targets) {
		/* respond with the supported type */
		string = xsel.xtarget;
		XChangeProperty(xsre->display, xsre->requestor, xsre->property,
				XA_ATOM, 32, PropModeReplace,
				(uchar *) &string, 1);
		xev.property = xsre->property;
	} else if (xsre->target == xsel.xtarget || xsre->target == XA_STRING) {
		/* xith XA_STRING non ascii characters may be incorrect in the
		 * requestor. It is not our problem, use utf8. */
		clipboard = XInternAtom(xw.dpy, "CLIPBOARD", 0);
		if (xsre->selection == XA_PRIMARY) {
			seltext = xsel.primary;
		} else if (xsre->selection == clipboard) {
			seltext = xsel.clipboard;
		} else {
			fprintf(stderr,
					"unhandled clipboard selection 0x%lx\n",
					xsre->selection);
			return;
		}
		if (seltext != NULL) {
			XChangeProperty(xsre->display, xsre->requestor,
					xsre->property, xsre->target,
					8, PropModeReplace,
					(uchar *)seltext, strlen(seltext));
			xev.property = xsre->property;
		}
	}

	/* all done, send a notification to the listener */
	if (!XSendEvent(xsre->display, xsre->requestor, 1, 0, (XEvent *) &xev))
		fprintf(stderr, "error sending SelectionNotify event\n");
}

void
selectionnotify(XEvent *e)
{
	ulong nitems, ofs, rem;
	int format;
	uchar *data, *last, *repl;
	Atom type, incratom, property = None;

	incratom = XInternAtom(xw.dpy, "INCR", 0);

	ofs = 0;
	if (e->type == SelectionNotify)
		property = e->xselection.property;
	else if (e->type == PropertyNotify)
		property = e->xproperty.atom;

	if (property == None)
		return;

	do {
		if (XGetWindowProperty(xw.dpy, xw.win, property, ofs,
					BUFSIZ/4, False, AnyPropertyType,
					&type, &format, &nitems, &rem,
					&data)) {
			fprintf(stderr, "clipboard allocation failed\n");
			return;
		}

		if (e->type == PropertyNotify && nitems == 0 && rem == 0) {
			/* If there is some PropertyNotify with no data, then
			 * this is the signal of the selection owner that all
			 * data has been transferred. We won't need to receive
			 * PropertyNotify events anymore. */
			MODBIT(xw.attrs.event_mask, 0, PropertyChangeMask);
			XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask, &xw.attrs);
		}

		if (type == incratom) {
			/* Activate the PropertyNotify events so we receive
			 * when the selection owner does send us the next
			 * chunk of data. */
			MODBIT(xw.attrs.event_mask, 1, PropertyChangeMask);
			XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask, &xw.attrs);

			/* Deleting the property is the transfer start signal. */
			XDeleteProperty(xw.dpy, xw.win, (int)property);
			continue;
		}

		/* As seen in seltext:
		 * Line endings are inconsistent in the terminal and GUI world
		 * copy and pasting. When receiving some selection data,
		 * replace all '\n' with '\r'.
		 * FIXME: Fix the computer world. */
		repl = data;
		last = data + nitems * format / 8;
		while ((repl = memchr(repl, '\n', last - repl)))
			*repl++ = '\r';

		if (win.bracketpaste && ofs == 0)
			ttywrite("\033[200~", 6, 0);
		ttywrite((char *)data, nitems * format / 8, 1);
		if (win.bracketpaste && rem == 0)
			ttywrite("\033[201~", 6, 0);
		XFree(data);
		/* number of 32-bit chunks returned */
		ofs += nitems * format / 32;
	} while (rem > 0);

	/* Deleting the property again tells the selection owner to send the
	 * next data chunk in the property. */
	XDeleteProperty(xw.dpy, xw.win, (int)property);
}

void
clientmessage(XEvent *e)
{
	/* See xembed specs:
	 * http://standards.freedesktop.org/xembed-spec/xembed-spec-latest.html */
	if (e->xclient.message_type == xw.xembed && e->xclient.format == 32) {
		if (e->xclient.data.l[1] == XEMBED_FOCUS_IN) {
			win.focused = 1;
			xseturgency(0);
		} else if (e->xclient.data.l[1] == XEMBED_FOCUS_OUT) {
			win.focused = 0;
		}
	} else if (e->xclient.data.l[0] == xw.wmdeletewin) {
		ttyhangup();
		exit(0);
	}
}

void
xclipcopy(void)
{
	Atom clipboard;

	free(xsel.clipboard);
	xsel.clipboard = NULL;

	if (xsel.primary != NULL) {
		xsel.clipboard = estrdup(xsel.primary);
		clipboard = XInternAtom(xw.dpy, "CLIPBOARD", 0);
		XSetSelectionOwner(xw.dpy, clipboard, xw.win, CurrentTime);
	}
}

void
xclippaste(void)
{
	Atom clipboard;

	clipboard = XInternAtom(xw.dpy, "CLIPBOARD", 0);
	XConvertSelection(xw.dpy, clipboard, xsel.xtarget, clipboard,
			xw.win, CurrentTime);
}

void
xselpaste(void)
{
	XConvertSelection(xw.dpy, XA_PRIMARY, xsel.xtarget, XA_PRIMARY,
			xw.win, CurrentTime);
}

void
xzoomabs(double fontsize)
{
	xunloadfonts();
	xloadfonts(usedfont, fontsize);
	cresize(0, 0);
	redraw();
	xhints();
}

void
xzoomrel(double delta)
{
	xzoomabs(usedfontsize+delta);
}

void
xzoomrst(void)
{
	if (defaultfontsize > 0)
		xzoomabs(defaultfontsize);
}

int
xtocol(int x)
{
	x -= borderpx;
	LIMIT(x, 0, win.tw - 1);
	return x / win.cw;
}

int
ytorow(int y)
{
	y -= borderpx;
	LIMIT(y, 0, win.th - 1);
	return y / win.ch;
}

void
mousesel(EvtCtx ctx, int done)
{
	int type;
	SelType *st;

	type = SEL_REGULAR;
	for (st = seltypes; st->type; st++) {
		if (MATCH(ctx.m, st->set, st->clr)) {
			type = st->type;
			break;
		}
	}

	selextend(xtocol(ctx.x), ytorow(ctx.y), type, done);
	if (done)
		/* TODO temp: should be event time, not current time; put time into
		 * evtctx */
		setsel(seltext(), CurrentTime);
}

/* TODO: should this accept an EvtCtx instead? */
void
mousereport(uint x, uint y, uint btn, int rel, uint state)
{
	uint n;
	uint b;
	char buf[64];
	int len;

	/* Non-SGR encoding scheme:
	 *     CSI 'M' n x y
	 * where x and y are character cell coordinates of the mouse cursor (the
	 * upper left corner being (1,1)), and n is binary encoded as follows:
	 *     000...11            some button released
	 *     000...bb, bb != 3   button bb + 1 pressed
	 *     010...bb            button bb + 4 pressed
	 *     100...bb            button bb + 8 pressed
	 *     bb1...bb            motion event; if no button is pressed, then
	 *                             bbbb == 0011; otherwise, the lowest numbered
	 *                             pressed button is encoded into bbbb as above
	 *     .....s..            s == 1 iff shift held
	 *     ....a...            a == 1 iff meta/alt held
	 *     ...c....            c == 1 iff control held
	 * To make them a printable character, 32 is added to each of n, x, and y.
	 * This is fine for n, because its most significant two bits are mutually
	 * exclusive, so n+32 never exceeds 255. However, this encoding scheme does
	 * not support x and y coordinates greater than 255-32 = 223.
	 * Note that, while this same encoding is used for all four of the non-SGR
	 * mouse reporting modes, some values of n are not used depending on the
	 * mouse reporting mode (e.g., X10 does not report releases or modifiers).
	 * 
	 * SGR encoding scheme:
	 *     CSI '<' n ';' x ';' y c
	 * where c is 'M' for a button press or 'm' for a button release, and n, x,
	 * and y are as above, except that n == 000???11 is impossible, because
	 * mouse releases are indicated using c instead, with the released button
	 * encoded into n in the same way as for presses. For motion events, st
	 * always has c == 'M', but applications should probably allow c == 'm' as
	 * well. */

	if (btn == 0) { /* motion event */
		if (x == win.prevx && y == win.prevy)
			return;
		if (win.mousemode != MOUSE_MOTION && win.mousemode != MOUSE_MANY)
			return;
		if (win.mousemode == MOUSE_MOTION && win.btns == 0)
			return;

		n = 0x20;
		/* Set b to lowest pressed button, or 12 if no buttons are pressed. */
		for (b = 1; b < 12 && !(win.btns & (1<<(b-1))); b++)
			;
	} else { /* button press or release */
		if (rel) {
			if (win.mousemode == MOUSE_X10)
				return;
			/* Don't send release events for scroll up/down */
			if (btn == 4 || btn == 5)
				return;
		}
		if (btn < 1 || btn > 11)
			return;

		n = 0;
		b = btn;
	}

	/* Encode button value into n. If no button is pressed for a motion event
	 * in mode MOUSE_MANY, then encode it as a release. */
	if ((!win.mousesgr && rel) || b == 12)
		n += 0x03;
	else if (b >= 8)
		n += 0x80 + b - 8;
	else if (b >= 4)
		n += 0x40 + b - 4;
	else
		n += b - 1;

	if (win.mousemode != MOUSE_X10) {
		n += (state & ShiftMask)   ? 0x04 : 0;
		n += (state & Mod1Mask)    ? 0x08 : 0;
		n += (state & ControlMask) ? 0x10 : 0;
	}

	if (win.mousesgr)
		len = snprintf(buf, sizeof buf, "\033[<%u;%u;%u%c",
				n, x+1, y+1, rel ? 'm' : 'M');
	else if (x <= 223 && y <= 223)
		len = snprintf(buf, sizeof buf, "\033[M%c%c%c",
				n+32, x+32+1, y+32+1);
	else
		return;
	ttywrite(buf, len, 0);

	win.prevx = x;
	win.prevy = y;
}

void
setsel(char *str, Time t)
{
	if (!str)
		return;

	free(xsel.primary);
	xsel.primary = str;

	XSetSelectionOwner(xw.dpy, XA_PRIMARY, xw.win, t);
	if (XGetSelectionOwner(xw.dpy, XA_PRIMARY) != xw.win)
		selclear();
}

void
xsetsel(char *str)
{
	setsel(str, CurrentTime);
}

void
cresize(int width, int height)
{
	int col, row;

	if (width != 0)
		win.w = width;
	if (height != 0)
		win.h = height;

	col = (win.w - 2 * borderpx) / win.cw;
	row = (win.h - 2 * borderpx) / win.ch;
	col = MAX(1, col);
	row = MAX(1, row);

	tresize(col, row);
	xresize(col, row);
	ttyresize(win.tw, win.th);
}

void
xresize(int col, int row)
{
	win.tw = col * win.cw;
	win.th = row * win.ch;

	XFreePixmap(xw.dpy, xw.buf);
	xw.buf = XCreatePixmap(xw.dpy, xw.win, win.w, win.h,
			DefaultDepth(xw.dpy, xw.scr));
	XftDrawChange(xw.draw, xw.buf);
	xclear(0, 0, win.w, win.h);

	/* resize to new width */
	xw.specbuf = erealloc(xw.specbuf, col * sizeof(XftGlyphFontSpec));
}

ushort
sixd_to_16bit(int x)
{
	return x == 0 ? 0 : 0x3737 + 0x2828 * x;
}

int
xloadcolor(int i, const char *name, XftColor *ncolor)
{
	XRenderColor color = { .alpha = 0xffff };

	if (!name) {
		if (BETWEEN(i, 16, 255)) { /* 256 color */
			if (i < 6*6*6+16) { /* same colors as xterm */
				color.red   = sixd_to_16bit( ((i-16)/36)%6 );
				color.green = sixd_to_16bit( ((i-16)/6) %6 );
				color.blue  = sixd_to_16bit( ((i-16)/1) %6 );
			} else { /* greyscale */
				color.red = 0x0808 + 0x0a0a * (i - (6*6*6+16));
				color.green = color.blue = color.red;
			}
			return XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &color, ncolor);
		} else {
			name = colorname[i];
		}
	}

	return XftColorAllocName(xw.dpy, xw.vis, xw.cmap, name, ncolor);
}

void
xloadcolors(void)
{
	int i;
	static int loaded;
	XftColor *cp;
	const char **c;

	if (loaded) {
		for (cp = dc.col; cp < &dc.col[dc.collen]; ++cp)
			XftColorFree(xw.dpy, xw.vis, xw.cmap, cp);
	} else {
		dc.collen = 256;
		for (c = &colorname[256]; *c; c++)
			dc.collen++;
		dc.col = emalloc(dc.collen * sizeof(XftColor));
	}

	for (i = 0; i < dc.collen; i++) {
		if (!xloadcolor(i, NULL, &dc.col[i])) {
			if (colorname[i])
				die("allocate color '%s' failed\n", colorname[i]);
			else
				die("allocate color %d failed\n", i);
		}
	}
	loaded = 1;
}

int
xsetcolorname(int x, const char *name)
{
	XftColor ncolor;

	if (!BETWEEN(x, 0, dc.collen))
		return 1;

	if (!xloadcolor(x, name, &ncolor))
		return 1;

	XftColorFree(xw.dpy, xw.vis, xw.cmap, &dc.col[x]);
	dc.col[x] = ncolor;

	return 0;
}

/* Absolute coordinates. */
void
xclear(int x1, int y1, int x2, int y2)
{
	XftDrawRect(xw.draw,
			&dc.col[win.reversevid ? defaultfg : defaultbg],
			x1, y1, x2-x1, y2-y1);
}

void
xhints(void)
{
	XClassHint class = {opt_name ? opt_name : termname,
	                    opt_class ? opt_class : termname};
	XWMHints wm = {.flags = InputHint, .input = 1};
	XSizeHints *sizeh;

	sizeh = XAllocSizeHints();

	sizeh->flags = PSize | PResizeInc | PBaseSize | PMinSize;
	sizeh->height = win.h;
	sizeh->width = win.w;
	sizeh->height_inc = win.ch;
	sizeh->width_inc = win.cw;
	sizeh->base_height = 2 * borderpx;
	sizeh->base_width = 2 * borderpx;
	sizeh->min_height = win.ch + 2 * borderpx;
	sizeh->min_width = win.cw + 2 * borderpx;
	if (xw.isfixed) {
		sizeh->flags |= PMaxSize;
		sizeh->min_width = sizeh->max_width = win.w;
		sizeh->min_height = sizeh->max_height = win.h;
	}
	if (xw.gm & (XValue|YValue)) {
		sizeh->flags |= USPosition | PWinGravity;
		sizeh->x = xw.l;
		sizeh->y = xw.t;
		sizeh->win_gravity = xgeommasktogravity(xw.gm);
	}

	XSetWMProperties(xw.dpy, xw.win, NULL, NULL, NULL, 0, sizeh, &wm, &class);
	XFree(sizeh);
}

int
xgeommasktogravity(int mask)
{
	switch (mask & (XNegative|YNegative)) {
	case 0:
		return NorthWestGravity;
	case XNegative:
		return NorthEastGravity;
	case YNegative:
		return SouthWestGravity;
	}

	return SouthEastGravity;
}

int
xloadfont(Font *f, FcPattern *pattern)
{
	FcPattern *configured;
	FcPattern *match;
	FcResult result;
	XGlyphInfo extents;
	int wantattr, haveattr;

	/* Manually configure instead of calling XftMatchFont so that we can use
	 * the configured pattern for "missing glyph" lookups. */
	configured = FcPatternDuplicate(pattern);
	if (!configured)
		return 1;

	FcConfigSubstitute(NULL, configured, FcMatchPattern);
	XftDefaultSubstitute(xw.dpy, xw.scr, configured);

	match = FcFontMatch(NULL, configured, &result);
	if (!match) {
		FcPatternDestroy(configured);
		return 1;
	}

	if (!(f->match = XftFontOpenPattern(xw.dpy, match))) {
		FcPatternDestroy(configured);
		FcPatternDestroy(match);
		return 1;
	}

	if ((XftPatternGetInteger(pattern, "slant", 0, &wantattr) ==
	    XftResultMatch)) {
		/* Check if xft was unable to find a font with the appropriate
		 * slant but gave us one anyway. Try to mitigate. */
		if ((XftPatternGetInteger(f->match->pattern, "slant", 0,
		    &haveattr) != XftResultMatch) || haveattr < wantattr) {
			f->badslant = 1;
			fputs("font slant does not match\n", stderr);
		}
	}

	if ((XftPatternGetInteger(pattern, "weight", 0, &wantattr) ==
	    XftResultMatch)) {
		if ((XftPatternGetInteger(f->match->pattern, "weight", 0,
		    &haveattr) != XftResultMatch) || haveattr != wantattr) {
			f->badweight = 1;
			fputs("font weight does not match\n", stderr);
		}
	}

	XftTextExtentsUtf8(xw.dpy, f->match,
		(const FcChar8 *) ascii_printable,
		strlen(ascii_printable), &extents);

	f->set = NULL;
	f->pattern = configured;

	f->ascent = f->match->ascent;
	f->descent = f->match->descent;
	f->lbearing = 0;
	f->rbearing = f->match->max_advance_width;

	f->height = f->ascent + f->descent;
	f->width = DIVCEIL(extents.xOff, strlen(ascii_printable));

	return 0;
}

void
xloadfonts(const char *fontstr, double fontsize)
{
	FcPattern *pattern;
	double fontval;

	if (fontstr[0] == '-')
		pattern = XftXlfdParse(fontstr, False, False);
	else
		pattern = FcNameParse((const FcChar8 *)fontstr);

	if (!pattern)
		die("open font '%s' failed\n", fontstr);

	if (fontsize > 1) {
		FcPatternDel(pattern, FC_PIXEL_SIZE);
		FcPatternDel(pattern, FC_SIZE);
		FcPatternAddDouble(pattern, FC_PIXEL_SIZE, fontsize);
		usedfontsize = fontsize;
	} else {
		if (FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &fontval) ==
				FcResultMatch) {
			usedfontsize = fontval;
		} else if (FcPatternGetDouble(pattern, FC_SIZE, 0, &fontval) ==
				FcResultMatch) {
			usedfontsize = -1;
		} else {
			/* Default font size is 12, if none given. This is to
			 * have a known usedfontsize value. */
			FcPatternAddDouble(pattern, FC_PIXEL_SIZE, 12);
			usedfontsize = 12;
		}
		defaultfontsize = usedfontsize;
	}

	if (xloadfont(&dc.font, pattern))
		die("open font '%s' failed\n", fontstr);

	if (usedfontsize < 0) {
		FcPatternGetDouble(dc.font.match->pattern, FC_PIXEL_SIZE, 0, &fontval);
		usedfontsize = fontval;
		if (fontsize == 0)
			defaultfontsize = fontval;
	}

	/* Setting character width and height. */
	win.cw = ceilf(dc.font.width * cwscale);
	win.ch = ceilf(dc.font.height * chscale);

	FcPatternDel(pattern, FC_SLANT);
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
	if (xloadfont(&dc.ifont, pattern))
		die("open font '%s' failed\n", fontstr);

	FcPatternDel(pattern, FC_WEIGHT);
	FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
	if (xloadfont(&dc.ibfont, pattern))
		die("open font '%s' failed\n", fontstr);

	FcPatternDel(pattern, FC_SLANT);
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
	if (xloadfont(&dc.bfont, pattern))
		die("open font '%s' failed\n", fontstr);

	FcPatternDestroy(pattern);
}

void
xunloadfont(Font *f)
{
	XftFontClose(xw.dpy, f->match);
	FcPatternDestroy(f->pattern);
	if (f->set)
		FcFontSetDestroy(f->set);
}

void
xunloadfonts(void)
{
	/* Free the loaded fonts in the font cache.  */
	while (frclen > 0)
		XftFontClose(xw.dpy, frc[--frclen].font);

	xunloadfont(&dc.font);
	xunloadfont(&dc.bfont);
	xunloadfont(&dc.ifont);
	xunloadfont(&dc.ibfont);
}

int
ximopen(Display *dpy)
{
	XIMCallback imdestroy = { .client_data = NULL, .callback = ximdestroy };
	XICCallback icdestroy = { .client_data = NULL, .callback = xicdestroy };

	xw.ime.xim = XOpenIM(xw.dpy, NULL, NULL, NULL);
	if (xw.ime.xim == NULL)
		return 0;

	if (XSetIMValues(xw.ime.xim, XNDestroyCallback, &imdestroy, NULL))
		fprintf(stderr, "XSetIMValues: failed to set XNDestroyCallback\n");

	xw.ime.spotlist = XVaCreateNestedList(0, XNSpotLocation,
			&xw.ime.spot, NULL);

	if (xw.ime.xic == NULL) {
		xw.ime.xic = XCreateIC(xw.ime.xim, XNInputStyle,
				XIMPreeditNothing | XIMStatusNothing, XNClientWindow, xw.win,
				XNDestroyCallback, &icdestroy, NULL);
		if (xw.ime.xic == NULL)
			fprintf(stderr, "XCreateIC: failed to create input context\n");
	}

	return 1;
}

void
ximinstantiate(Display *dpy, XPointer client, XPointer call)
{
	if (ximopen(dpy))
		XUnregisterIMInstantiateCallback(xw.dpy, NULL, NULL, NULL,
				ximinstantiate, NULL);
}

void
ximdestroy(XIM xim, XPointer client, XPointer call)
{
	xw.ime.xim = NULL;
	XRegisterIMInstantiateCallback(xw.dpy, NULL, NULL, NULL,
			ximinstantiate, NULL);
	XFree(xw.ime.spotlist);
}

int
xicdestroy(XIC xim, XPointer client, XPointer call)
{
	xw.ime.xic = NULL;
	return 1;
}

int
xmakeglyphfontspecs(XftGlyphFontSpec *specs, const Glyph *glyphs, int len, int x, int y)
{
	float winx = borderpx + x * win.cw, winy = borderpx + y * win.ch, xp, yp;
	ushort mode, prevmode = USHRT_MAX;
	Font *font = &dc.font;
	int frcflags = FRC_NORMAL;
	float runewidth = win.cw;
	Rune rune;
	FT_UInt glyphidx;
	FcResult fcres;
	FcPattern *fcpattern, *fontpattern;
	FcFontSet *fcsets[] = { NULL };
	FcCharSet *fccharset;
	int i, f, numspecs = 0;

	for (i = 0, xp = winx, yp = winy + font->ascent; i < len; ++i) {
		/* Fetch rune and mode for current glyph. */
		rune = glyphs[i].u;
		mode = glyphs[i].mode;

		/* Skip dummy wide-character spacing. */
		if (mode == ATTR_WDUMMY)
			continue;

		/* Determine font for glyph if different from previous glyph. */
		if (prevmode != mode) {
			prevmode = mode;
			font = &dc.font;
			frcflags = FRC_NORMAL;
			runewidth = win.cw * ((mode & ATTR_WIDE) ? 2.0f : 1.0f);
			if ((mode & ATTR_ITALIC) && (mode & ATTR_BOLD)) {
				font = &dc.ibfont;
				frcflags = FRC_ITALICBOLD;
			} else if (mode & ATTR_ITALIC) {
				font = &dc.ifont;
				frcflags = FRC_ITALIC;
			} else if (mode & ATTR_BOLD) {
				font = &dc.bfont;
				frcflags = FRC_BOLD;
			}
			yp = winy + font->ascent;
		}

		/* Lookup character index with default font. */
		glyphidx = XftCharIndex(xw.dpy, font->match, rune);
		if (glyphidx) {
			specs[numspecs].font = font->match;
			specs[numspecs].glyph = glyphidx;
			specs[numspecs].x = (short)xp;
			specs[numspecs].y = (short)yp;
			xp += runewidth;
			numspecs++;
			continue;
		}

		/* Fallback on font cache, search the font cache for match. */
		for (f = 0; f < frclen; f++) {
			glyphidx = XftCharIndex(xw.dpy, frc[f].font, rune);
			/* Everything correct. */
			if (glyphidx && frc[f].flags == frcflags)
				break;
			/* We got a default font for a not found glyph. */
			if (!glyphidx && frc[f].flags == frcflags
					&& frc[f].unicodep == rune) {
				break;
			}
		}

		/* Nothing was found. Use fontconfig to find matching font. */
		if (f >= frclen) {
			if (!font->set)
				font->set = FcFontSort(0, font->pattern, 1, 0, &fcres);
			fcsets[0] = font->set;

			/* Nothing was found in the cache. Now use
			 * some dozen of Fontconfig calls to get the
			 * font for one single character.
			 *
			 * Xft and fontconfig are design failures. */
			fcpattern = FcPatternDuplicate(font->pattern);
			fccharset = FcCharSetCreate();

			FcCharSetAddChar(fccharset, rune);
			FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
			FcPatternAddBool(fcpattern, FC_SCALABLE, 1);

			FcConfigSubstitute(0, fcpattern, FcMatchPattern);
			FcDefaultSubstitute(fcpattern);

			fontpattern = FcFontSetMatch(0, fcsets, 1, fcpattern, &fcres);

			/* Allocate memory for the new cache entry. */
			if (frclen >= frccap) {
				frccap += 16;
				frc = erealloc(frc, frccap * sizeof(Fontcache));
			}

			frc[frclen].font = XftFontOpenPattern(xw.dpy, fontpattern);
			if (!frc[frclen].font)
				die("XftFontOpenPattern failed seeking fallback font: %s\n",
						strerror(errno));
			frc[frclen].flags = frcflags;
			frc[frclen].unicodep = rune;

			glyphidx = XftCharIndex(xw.dpy, frc[frclen].font, rune);

			f = frclen;
			frclen++;

			FcPatternDestroy(fcpattern);
			FcCharSetDestroy(fccharset);
		}

		specs[numspecs].font = frc[f].font;
		specs[numspecs].glyph = glyphidx;
		specs[numspecs].x = (short)xp;
		specs[numspecs].y = (short)yp;
		xp += runewidth;
		numspecs++;
	}

	return numspecs;
}

void
xdrawglyphfontspecs(const XftGlyphFontSpec *specs, Glyph base, int len, int x, int y)
{
	int charlen = len * ((base.mode & ATTR_WIDE) ? 2 : 1);
	int winx = borderpx + x * win.cw, winy = borderpx + y * win.ch,
	    width = charlen * win.cw;
	XftColor *fg, *bg, *temp, revfg, revbg, truefg, truebg;
	XRenderColor colfg, colbg;
	XRectangle r;

	/* Fallback on color display for attributes not supported by the font */
	if (base.mode & ATTR_ITALIC && base.mode & ATTR_BOLD) {
		if (dc.ibfont.badslant || dc.ibfont.badweight)
			base.fg = defaultattr;
	} else if ((base.mode & ATTR_ITALIC && dc.ifont.badslant) ||
	    (base.mode & ATTR_BOLD && dc.bfont.badweight)) {
		base.fg = defaultattr;
	}

	if (IS_TRUECOLOR(base.fg)) {
		colfg.alpha = 0xffff;
		colfg.red = TRUERED(base.fg);
		colfg.green = TRUEGREEN(base.fg);
		colfg.blue = TRUEBLUE(base.fg);
		XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colfg, &truefg);
		fg = &truefg;
	} else {
		fg = &dc.col[base.fg];
	}

	if (IS_TRUECOLOR(base.bg)) {
		colbg.alpha = 0xffff;
		colbg.green = TRUEGREEN(base.bg);
		colbg.red = TRUERED(base.bg);
		colbg.blue = TRUEBLUE(base.bg);
		XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colbg, &truebg);
		bg = &truebg;
	} else {
		bg = &dc.col[base.bg];
	}

	/* Change basic system colors [0-7] to bright system colors [8-15] */
	if ((base.mode & ATTR_BOLD_FAINT) == ATTR_BOLD && BETWEEN(base.fg, 0, 7))
		fg = &dc.col[base.fg + 8];

	if (win.reversevid) {
		if (fg == &dc.col[defaultfg]) {
			fg = &dc.col[defaultbg];
		} else {
			colfg.red = ~fg->color.red;
			colfg.green = ~fg->color.green;
			colfg.blue = ~fg->color.blue;
			colfg.alpha = fg->color.alpha;
			XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colfg, &revfg);
			fg = &revfg;
		}

		if (bg == &dc.col[defaultbg]) {
			bg = &dc.col[defaultfg];
		} else {
			colbg.red = ~bg->color.red;
			colbg.green = ~bg->color.green;
			colbg.blue = ~bg->color.blue;
			colbg.alpha = bg->color.alpha;
			XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colbg, &revbg);
			bg = &revbg;
		}
	}

	if ((base.mode & ATTR_BOLD_FAINT) == ATTR_FAINT) {
		colfg.red = fg->color.red / 2;
		colfg.green = fg->color.green / 2;
		colfg.blue = fg->color.blue / 2;
		colfg.alpha = fg->color.alpha;
		XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colfg, &revfg);
		fg = &revfg;
	}

	if (base.mode & ATTR_REVERSE) {
		temp = fg;
		fg = bg;
		bg = temp;
	}

	if (base.mode & ATTR_BLINK && win.blink)
		fg = bg;

	if (base.mode & ATTR_INVISIBLE)
		fg = bg;

	/* Intelligent cleaning up of the borders. */
	if (x == 0) {
		xclear(0, (y == 0)? 0 : winy, borderpx, winy + win.ch +
				((winy + win.ch >= borderpx + win.th) ? win.h : 0));
	}
	if (winx + width >= borderpx + win.tw) {
		xclear(winx + width, (y == 0)? 0 : winy, win.w, ((winy + win.ch >=
				borderpx + win.th) ? win.h : (winy + win.ch)));
	}
	if (y == 0)
		xclear(winx, 0, winx + width, borderpx);
	if (winy + win.ch >= borderpx + win.th)
		xclear(winx, winy + win.ch, winx + width, win.h);

	/* Clean up the region we want to draw to. */
	XftDrawRect(xw.draw, bg, winx, winy, width, win.ch);

	/* Set the clip region because Xft is sometimes dirty. */
	r.x = 0;
	r.y = 0;
	r.height = win.ch;
	r.width = width;
	XftDrawSetClipRectangles(xw.draw, winx, winy, &r, 1);

	/* Render the glyphs. */
	XftDrawGlyphFontSpec(xw.draw, fg, specs, len);

	/* Render underline and strikethrough. */
	if (base.mode & ATTR_UNDERLINE) {
		XftDrawRect(xw.draw, fg, winx, winy + dc.font.ascent + 1, width, 1);
	}

	if (base.mode & ATTR_STRUCK) {
		XftDrawRect(xw.draw, fg, winx, winy + 2 * dc.font.ascent / 3,
				width, 1);
	}

	/* Reset clip to none. */
	XftDrawSetClip(xw.draw, 0);
}

void
xdrawglyph(Glyph g, int x, int y)
{
	int numspecs;
	XftGlyphFontSpec spec;

	numspecs = xmakeglyphfontspecs(&spec, &g, 1, x, y);
	xdrawglyphfontspecs(&spec, g, numspecs, x, y);
}

void
xdrawcursor(int cx, int cy, Glyph g, int ox, int oy, Glyph og)
{
	XftColor drawcol;

	/* remove the old cursor */
	if (selcontains(ox, oy))
		og.mode ^= ATTR_REVERSE;
	xdrawglyph(og, ox, oy);

	if (win.hidecursor)
		return;

	/* Select the right color for the right mode. */
	g.mode &= ATTR_BOLD|ATTR_ITALIC|ATTR_UNDERLINE|ATTR_STRUCK|ATTR_WIDE;

	if (win.reversevid) {
		g.mode |= ATTR_REVERSE;
		g.bg = defaultfg;
		if (selcontains(cx, cy)) {
			drawcol = dc.col[defaultcs];
			g.fg = defaultrcs;
		} else {
			drawcol = dc.col[defaultrcs];
			g.fg = defaultcs;
		}
	} else {
		if (selcontains(cx, cy)) {
			g.fg = defaultfg;
			g.bg = defaultrcs;
		} else {
			g.fg = defaultbg;
			g.bg = defaultcs;
		}
		drawcol = dc.col[g.bg];
	}

	/* draw the new one */
	if (win.focused) {
		switch (win.cursor) {
		case 7: /* st extension */
			g.u = 0x2603; /* snowman (U+2603) */
			/* FALLTHROUGH */
		case 0: /* Blinking Block */
		case 1: /* Blinking Block (Default) */
		case 2: /* Steady Block */
			xdrawglyph(g, cx, cy);
			break;
		case 3: /* Blinking Underline */
		case 4: /* Steady Underline */
			XftDrawRect(xw.draw, &drawcol,
					borderpx + cx * win.cw,
					borderpx + (cy + 1) * win.ch - \
						cursorthickness,
					win.cw, cursorthickness);
			break;
		case 5: /* Blinking bar */
		case 6: /* Steady bar */
			XftDrawRect(xw.draw, &drawcol,
					borderpx + cx * win.cw,
					borderpx + cy * win.ch,
					cursorthickness, win.ch);
			break;
		}
	} else {
		XftDrawRect(xw.draw, &drawcol,
				borderpx + cx * win.cw,
				borderpx + cy * win.ch,
				win.cw - 1, 1);
		XftDrawRect(xw.draw, &drawcol,
				borderpx + cx * win.cw,
				borderpx + cy * win.ch,
				1, win.ch - 1);
		XftDrawRect(xw.draw, &drawcol,
				borderpx + (cx + 1) * win.cw - 1,
				borderpx + cy * win.ch,
				1, win.ch - 1);
		XftDrawRect(xw.draw, &drawcol,
				borderpx + cx * win.cw,
				borderpx + (cy + 1) * win.ch - 1,
				win.cw, 1);
	}
}

void
xsetenv(void)
{
	char buf[sizeof(long) * 8 + 1];

	snprintf(buf, sizeof(buf), "%lu", xw.win);
	setenv("WINDOWID", buf, 1);
}

void
xseticontitle(char *p)
{
	XTextProperty prop;
	DEFAULT(p, opt_title);

	Xutf8TextListToTextProperty(xw.dpy, &p, 1, XUTF8StringStyle, &prop);
	XSetWMIconName(xw.dpy, xw.win, &prop);
	XSetTextProperty(xw.dpy, xw.win, &prop, xw.netwmiconname);
	XFree(prop.value);
}

void
xsettitle(char *p)
{
	XTextProperty prop;
	DEFAULT(p, opt_title);

	Xutf8TextListToTextProperty(xw.dpy, &p, 1, XUTF8StringStyle, &prop);
	XSetWMName(xw.dpy, xw.win, &prop);
	XSetTextProperty(xw.dpy, xw.win, &prop, xw.netwmname);
	XFree(prop.value);
}

int
xstartdraw(void)
{
	return win.visible;
}

int
glyphattrcmp(Glyph a, Glyph b)
{
	return a.mode != b.mode || a.fg != b.fg || a.bg != b.bg;
}

void
xdrawline(Line line, int x1, int y1, int x2)
{
	int i, x, ox, numspecs;
	Glyph base, new;
	XftGlyphFontSpec *specs = xw.specbuf;

	numspecs = xmakeglyphfontspecs(specs, &line[x1], x2 - x1, x1, y1);
	i = ox = 0;
	for (x = x1; x < x2 && i < numspecs; x++) {
		new = line[x];
		if (new.mode == ATTR_WDUMMY)
			continue;
		if (selcontains(x, y1))
			new.mode ^= ATTR_REVERSE;
		if (i > 0 && glyphattrcmp(base, new)) {
			xdrawglyphfontspecs(specs, base, i, ox, y1);
			specs += i;
			numspecs -= i;
			i = 0;
		}
		if (i == 0) {
			ox = x;
			base = new;
		}
		i++;
	}
	if (i > 0)
		xdrawglyphfontspecs(specs, base, i, ox, y1);
}

void
xfinishdraw(void)
{
	XCopyArea(xw.dpy, xw.buf, xw.win, dc.gc, 0, 0, win.w, win.h, 0, 0);
	XSetForeground(xw.dpy, dc.gc,
			dc.col[win.reversevid ? defaultfg : defaultbg].pixel);
}

void
xximspot(int x, int y)
{
	if (xw.ime.xic == NULL)
		return;

	xw.ime.spot.x = borderpx + x * win.cw;
	xw.ime.spot.y = borderpx + (y + 1) * win.ch;

	XSetICValues(xw.ime.xic, XNPreeditAttributes, xw.ime.spotlist, NULL);
}

void
xsetpointermotion(int set)
{
	MODBIT(xw.attrs.event_mask, set, PointerMotionMask);
	XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask, &xw.attrs);
}

int
xsetcursor(int cursor)
{
	if (!BETWEEN(cursor, 0, 7)) /* 7: st extension */
		return 1;
	win.cursor = cursor;
	return 0;
}

void
xseturgency(int add)
{
	XWMHints *h = XGetWMHints(xw.dpy, xw.win);

	MODBIT(h->flags, add, XUrgencyHint);
	XSetWMHints(xw.dpy, xw.win, h);
	XFree(h);
}

void
xbell(void)
{
	if (!win.focused)
		xseturgency(1);
	if (bellvolume)
		XkbBell(xw.dpy, xw.win, bellvolume, (Atom)NULL);
}

void
xinit(int cols, int rows)
{
	XGCValues gcvalues;
	Cursor cursor;
	Window parent;
	pid_t thispid = getpid();
	XColor xmousefg, xmousebg;

	/* XXX: check this (moved from main) */
	xw.l = xw.t = 0;
	xw.isfixed = opt_fixed;
	xsetcursor(cursorshape);

	if (opt_geom)
		xw.gm = XParseGeometry(opt_geom, &xw.l, &xw.t,
				(uint*) &cols, (uint*) &rows);

	if (!(xw.dpy = XOpenDisplay(NULL)))
		die("open display failed\n");
	xw.scr = XDefaultScreen(xw.dpy);
	xw.vis = XDefaultVisual(xw.dpy, xw.scr);

	/* font */
	if (!FcInit())
		die("fontconfig init failed\n");

	usedfont = (opt_font == NULL) ? font : opt_font;
	xloadfonts(usedfont, 0);

	/* colors */
	xw.cmap = XDefaultColormap(xw.dpy, xw.scr);
	xloadcolors();

	/* adjust fixed window geometry */
	win.w = 2 * borderpx + cols * win.cw;
	win.h = 2 * borderpx + rows * win.ch;
	if (xw.gm & XNegative)
		xw.l += DisplayWidth(xw.dpy, xw.scr) - win.w - 2;
	if (xw.gm & YNegative)
		xw.t += DisplayHeight(xw.dpy, xw.scr) - win.h - 2;

	/* Events */
	xw.attrs.background_pixel = dc.col[defaultbg].pixel;
	xw.attrs.border_pixel = dc.col[defaultbg].pixel;
	xw.attrs.bit_gravity = NorthWestGravity;
	xw.attrs.event_mask = FocusChangeMask | KeyPressMask | KeyReleaseMask
		| ExposureMask | VisibilityChangeMask | StructureNotifyMask
		| ButtonMotionMask | ButtonPressMask | ButtonReleaseMask;
	xw.attrs.colormap = xw.cmap;

	if (!(opt_embed && (parent = strtol(opt_embed, NULL, 0))))
		parent = XRootWindow(xw.dpy, xw.scr);
	xw.win = XCreateWindow(xw.dpy, parent, xw.l, xw.t,
			win.w, win.h, 0, XDefaultDepth(xw.dpy, xw.scr), InputOutput,
			xw.vis, CWBackPixel | CWBorderPixel | CWBitGravity
			| CWEventMask | CWColormap, &xw.attrs);

	memset(&gcvalues, 0, sizeof(gcvalues));
	gcvalues.graphics_exposures = False;
	dc.gc = XCreateGC(xw.dpy, parent, GCGraphicsExposures,
			&gcvalues);
	xw.buf = XCreatePixmap(xw.dpy, xw.win, win.w, win.h,
			DefaultDepth(xw.dpy, xw.scr));
	XSetForeground(xw.dpy, dc.gc, dc.col[defaultbg].pixel);
	XFillRectangle(xw.dpy, xw.buf, dc.gc, 0, 0, win.w, win.h);

	/* font spec buffer */
	xw.specbuf = emalloc(cols * sizeof(XftGlyphFontSpec));

	/* Xft rendering context */
	xw.draw = XftDrawCreate(xw.dpy, xw.buf, xw.vis, xw.cmap);

	/* input methods */
	if (!ximopen(xw.dpy))
		XRegisterIMInstantiateCallback(xw.dpy, NULL, NULL, NULL,
				ximinstantiate, NULL);

	/* white cursor, black outline */
	cursor = XCreateFontCursor(xw.dpy, mouseshape);
	XDefineCursor(xw.dpy, xw.win, cursor);

	if (XParseColor(xw.dpy, xw.cmap, colorname[mousefg], &xmousefg) == 0) {
		xmousefg.red   = 0xffff;
		xmousefg.green = 0xffff;
		xmousefg.blue  = 0xffff;
	}

	if (XParseColor(xw.dpy, xw.cmap, colorname[mousebg], &xmousebg) == 0) {
		xmousebg.red   = 0x0000;
		xmousebg.green = 0x0000;
		xmousebg.blue  = 0x0000;
	}

	XRecolorCursor(xw.dpy, cursor, &xmousefg, &xmousebg);

	xw.xembed = XInternAtom(xw.dpy, "_XEMBED", False);
	xw.wmdeletewin = XInternAtom(xw.dpy, "WM_DELETE_WINDOW", False);
	xw.netwmname = XInternAtom(xw.dpy, "_NET_WM_NAME", False);
	xw.netwmiconname = XInternAtom(xw.dpy, "_NET_WM_ICON_NAME", False);
	XSetWMProtocols(xw.dpy, xw.win, &xw.wmdeletewin, 1);

	xw.netwmpid = XInternAtom(xw.dpy, "_NET_WM_PID", False);
	XChangeProperty(xw.dpy, xw.win, xw.netwmpid, XA_CARDINAL, 32,
			PropModeReplace, (uchar *)&thispid, 1);

	win.numlock = 1;
	/* TODO: every other mode bit is 0; do this explicitly? */
	xsettitle(NULL);
	xhints();
	XMapWindow(xw.dpy, xw.win);
	XSync(xw.dpy, False);

	/* TODO there is a possiblity for a false triple click to be registered
	 * immediately after st starts... not really an issue, but why not zero
	 * these instead? */
	clock_gettime(CLOCK_MONOTONIC, &xsel.tclick1);
	clock_gettime(CLOCK_MONOTONIC, &xsel.tclick2);
	xsel.primary = NULL;
	xsel.clipboard = NULL;
	xsel.xtarget = XInternAtom(xw.dpy, "UTF8_STRING", 0);
	if (xsel.xtarget == None)
		xsel.xtarget = XA_STRING;

	/* XXX: check this (moved from main) */
	xsetenv();
}

int
xstart(void)
{
	int xfd = XConnectionNumber(xw.dpy);
	int w = win.w, h = win.h;
	XEvent ev;

	/* Wait for window mapping */
	do {
		XNextEvent(xw.dpy, &ev);
		/* This XFilterEvent call is required because the input method might
		 * have filtered the event. */
		if (XFilterEvent(&ev, None))
			continue;
		if (ev.type == ConfigureNotify) {
			w = ev.xconfigure.width;
			h = ev.xconfigure.height;
		}
	} while (ev.type != MapNotify);
	/* XXX this feels wrong. ConfigureNotify special case shouldn't exist */
	cresize(w, h);

	return xfd;
}
