/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */

#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/XF86keysym.h>

#ifdef XINERAMA
	#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#ifndef NODRW
	#include <X11/Xft/Xft.h>
#endif /* NODRW */

#include "util.h"
#include "drw.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
							   * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]))
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)))
#define OPAQUE                  0xffU

#if defined(__GNUC__) || defined(__clang__)
    #define MAYBE_UNUSED __attribute__((unused))
#elif defined(__cplusplus) && __cplusplus >= 201703L
    #define MAYBE_UNUSED [[maybe_unused]]
#else
    #define MAYBE_UNUSED
#endif

/* enums */
enum Cur { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum Scheme { SchemeNorm, SchemeSel }; /* color schemes */
enum NetAtoms {
	NetSupported, NetWMName, NetWMIcon, NetWMState, NetWMCheck,
	NetWMFullscreen, NetActiveWindow, NetWMWindowType,
	NetWMWindowTypeDialog, NetClientList, NetWMWindowsOpacity, NetLast
}; /* EWMH atoms */
enum WMAtoms { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum Clk {
	ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
	ClkClientWin, ClkRootWin, ClkLast
}; /* clicks */
enum Position {
	PositionNone = 0, PositionFullscreen,
	PositionTiled, PositionCenter,
	PositionNW, PositionW, PositionSW,
	PositionN, PositionFill, PositionS,
	PositionNE, PositionE, PositionSE,
	PositionForced = 0x1000
}; /* Position */

typedef uint16_t Tag;

typedef union Arg Arg;
union Arg {
	bool b;
	unsigned int ui;
	int i;
	float f;
	const void* v;
};

typedef struct Button Button;
struct Button {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg arg);
	Arg arg;
};

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256];
	float mina, maxa;
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	unsigned char bw, oldbw;
	float opacity;
	Tag tags;
	enum Position position, oldposition;
	bool isunresizeable, isalwaysontop, isurgent, neverfocus;
	bool ignorehints, hintsvalid;
	Client* next;
	Client* snext;
	Monitor* mon;
	Window win;
	#ifndef NODRW
		Picture icon;
		unsigned int icw, ich;
	#endif /* NODRW */
};

typedef struct Key Key;
struct Key {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg);
	const Arg arg;
};

typedef struct Layout Layout;
struct Layout {
	const char* symbol;
	void (*arrange)(Monitor* m, int n);
};

struct Monitor {
	float mfact;
	unsigned char nmaster;
	unsigned char num;
	#ifndef NODRW
		Window barwin;
		bool showbar, topbar;
		int bx, by, bw, bh;         /* bar geometry */
	#endif /* NODRW */
	int mx, my, mw, mh;             /* screen size */
	int oldmx, oldmy, oldmw, oldmh; /* screen size old */
	int wx, wy, ww, wh;             /* window area  */
	int gapwindow, gapbar, gapedge; /* gaps in px */
	Tag seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	Client* clients;
	Client* sel;
	Client* stack;
	Monitor* next;
	const Layout* lt[2];
};

typedef struct Rule Rule;
struct Rule {
	const char* class;
	const char* instance;
	const char* title;
	Tag tags;
	enum Position position;
	unsigned char monitor;
};

/* layout declerations */
static void ltcol(Monitor* m, int n);
static void ltgrid(Monitor* m, int n);
static void ltmonocle(Monitor* m, int n);
static void ltrow(Monitor* m, int n);

/* command declerations */
MAYBE_UNUSED static void cmdfocusclient(const Arg arg);
MAYBE_UNUSED static void cmdfocusmon(const Arg arg);
MAYBE_UNUSED static void cmdfocusstack(const Arg arg);
MAYBE_UNUSED static void cmdinclayout(const Arg arg);
MAYBE_UNUSED static void cmdincnmaster(const Arg arg);
MAYBE_UNUSED static void cmdkillclient(const Arg arg);
MAYBE_UNUSED static void cmdmovemouse(const Arg arg);
MAYBE_UNUSED static void cmdquit(const Arg arg);
MAYBE_UNUSED static void cmdresizemouse(const Arg arg);
MAYBE_UNUSED static void cmdsetgapwindow(const Arg arg);
MAYBE_UNUSED static void cmdsetgapbar(const Arg arg);
MAYBE_UNUSED static void cmdsetgapedge(const Arg arg);
MAYBE_UNUSED static void cmdsetlayout(const Arg arg);
MAYBE_UNUSED static void cmdsetmfact(const Arg arg);
MAYBE_UNUSED static void cmdsetposition(const Arg arg);
MAYBE_UNUSED static void cmdsendmon(const Arg arg);
MAYBE_UNUSED static void cmdspawn(const Arg arg);
MAYBE_UNUSED static void cmdtag(const Arg arg);
MAYBE_UNUSED static void cmdtogglebar(const Arg arg);
MAYBE_UNUSED static void cmdtogglefloating(const Arg arg);
MAYBE_UNUSED static void cmdtogglealwaysontop(const Arg arg);
MAYBE_UNUSED static void cmdtoggletag(const Arg arg);
MAYBE_UNUSED static void cmdtoggleview(const Arg arg);
MAYBE_UNUSED static void cmdview(const Arg arg);
MAYBE_UNUSED static void cmdwinview(const Arg arg);
MAYBE_UNUSED static void cmdzoom(const Arg arg);

/* event declerations */
static void eventbuttonpress(XEvent* e);
static void eventclientmessage(XEvent* e);
static void eventconfigurenotify(XEvent* e);
static void eventconfigurerequest(XEvent* e);
static void eventdestroynotify(XEvent* e);
static void evententernotify(XEvent *e);
static void eventexpose(XEvent* e);
static void eventfocusin(XEvent* e);
static void eventkeypress(XEvent* e);
static void eventmappingnotify(XEvent* e);
static void eventmaprequest(XEvent* e);
static void eventmotionnotify(XEvent *e);
static void eventpropertynotify(XEvent* e);
static void eventunmapnotify(XEvent* e);
static void eventhandle(XEvent* ev);

/* function declarations */
static void applyrules(Client* c);
static int applysizehints(Client* c, int* x, int* y, int* w, int* h, bool interact);
static void arrange(Monitor* m);
static void arrangemon(Monitor* m);
static void attach(Client* c);
static void attachstack(Client* c);
static void autostartexec(void);
static void buttonbar(XButtonPressedEvent* ev, Arg* arg, unsigned int* click);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor* mon);
static void configure(Client* c);
static Monitor* createmon(void);
static void detach(Client* c);
static void detachstack(Client* c);
static Monitor* dirtomon(int dir);
static void drawbar(Monitor* m);
static void drawbars(void);
static void focus(Client* c);
static void focusmon(Monitor* m, bool refocus);
static Atom getatomprop(Client* c, Atom prop);
static int getrootptr(int* x, int* y);
static unsigned long getstate(Window w);
static int gettextprop(Window w, Atom atom, char* text, unsigned int size);
static void grabbuttons(Client* c, bool focused);
static void grabkeys(void);
static void killclient(Client* c);
static Client* manage(Window w, XWindowAttributes* wa);
static Client* nexttiled(Client* c);
static void opacity(Client* c, float opacity);
static void opacitywin(Window w, float opacity);
static void pop(Client* c);
/* static Monitor* recttomon(int x, int y, int w, int h); */
static Monitor* postomon(int x, int y);
static void resize(Client* c, int x, int y, int w, int h, bool interact);
static void resizeclient(Client* c, int x, int y, int w, int h);
static void setpositionmove(Client* c, int position, bool force);
static void setposition(Client* c, int position, bool force);
static void restack(Monitor* m);
static void run(void);
static void scan(void);
static int sendevent(Client* c, Atom proto);
static void sendmon(Client* c, Monitor* m, bool refocus);
static void setclientstate(Client* c, long state);
static void setfocus(Client* c);
static void setmaster(Client* c);
static void setup(void);
static void seturgent(Client* c, int urg);
static void hideclient(Client* c);
static void showhide(Client* c);
static void togglefloating(Client* c);
static void freeclient(Client* c);
static void freeicon(Client* c);
static void unfocus(Client* c, bool setfocus);
static void unmanage(Client* c, bool destroyed);
static void updatebarpos(Monitor* m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client* c);
static void updatestatus(void);
static void updatetitle(Client* c);
static void updateicon(Client* c);
static void updatewindowtype(Client* c);
static void updatewmhints(Client* c);
static Client* wintoclient(Window w);
static Monitor* wintomon(Window w);
static int xerror(Display* dpy, XErrorEvent* ee);
static int xerrordummy(Display* dpy, XErrorEvent* ee);
static int xerrorstart(Display* dpy, XErrorEvent* ee);
static void xinitvisual();

/* variables */
static void (*const eventhandler[LASTEvent]) (XEvent*) = {
	[ButtonPress] = eventbuttonpress,
	[ClientMessage] = eventclientmessage,
	[ConfigureRequest] = eventconfigurerequest,
	[ConfigureNotify] = eventconfigurenotify,
	[DestroyNotify] = eventdestroynotify,
	[EnterNotify] = evententernotify,
	[Expose] = eventexpose,
	[FocusIn] = eventfocusin,
	[KeyPress] = eventkeypress,
	[MappingNotify] = eventmappingnotify,
	[MapRequest] = eventmaprequest,
	[MotionNotify] = eventmotionnotify,
	[PropertyNotify] = eventpropertynotify,
	[UnmapNotify] = eventunmapnotify
};
static unsigned int modifiers[] = { 0, LockMask, 0, 0 };
static int screen;
static int sw, sh; /* X display screen geometry width, height */
static int (*xerrorxlib)(Display*, XErrorEvent*);
static unsigned int numlockmask = 0;
static Atom wmatom[WMLast], netatom[NetLast];
static bool running = true;
static Cursor cursor[CurLast];
static Clr** scheme;
static Display* dpy;
static Monitor* mons;
static Monitor* selmon;
static Window root, wmcheckwin;
static Client* grabbedclient = NULL;
static long dummy;
#define dummyptr (&dummy)

#ifndef NODRW
	static Drw* drw;
	static char stext[512];
	static int textpad;
	static int useargb = 0;
	static int bh;
	static Visual* visual;
	static int depth;
	static Colormap cmap;
#endif /* NODRW */

#define AUTOSTARTDISOWN ((const char*)1) /* the zero page! */

/* configuration, allows nested code to access above variables */
#include "config.h"

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* layout implementations */

void ltcol(Monitor* m, int n) {
	unsigned int i, x, y, w, h, ww, mw, nm;
	Client* c;

	if (n == 1) {
		c = nexttiled(m->clients);
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, false);
		return;
	}

	nm = m->nmaster > n ? n : m->nmaster;
	x = m->wx;
	y = m->wy;
	w = m->ww + m->gapwindow;
	h = m->wh;
	mw = m->ww * m->mfact;
	ww = w - mw;
	mw /= nm;
	if (nm != n) ww /= n - nm;
	
	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), ++i) {
		if (i < nm) {
			resize(c, x, y, mw - m->gapwindow - 2 * c->bw, h - 2 * c->bw, false);
			x += mw;
		} else if (m->mfact < 1.0) {
			resize(c, x, y, ww - m->gapwindow - 2 * c->bw, h - 2 * c->bw, false);
			x += ww;
		} else {
			hideclient(c);
		}
	}
}

void ltrow(Monitor* m, int n) {
	unsigned int i, nm, my, wy, mh, wh;
	Client* c;

	if (n == 1) {
		c = nexttiled(m->clients);
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, false);
		return;
	}

	nm = m->nmaster > n ? n : m->nmaster;
	my = m->wy;
	wy = m->wy;
	wh = (m->wh + m->gapwindow);
	if (n != nm) wh /= n - nm;
	mh = (m->wh + m->gapwindow);
	if (nm != 0) mh /= nm;
	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), ++i) {
		if (i < nm) {
			resize(c, m->wx, my, m->ww * m->mfact - m->gapwindow - 2 * c->bw, mh - m->gapwindow - 2 * c->bw, false);
			my += mh;
		} else if (m->mfact < 1.0) {
			resize(c, m->wx + m->ww * m->mfact, wy, m->ww * (1.0 - m->mfact) - 2 * c->bw, wh - m->gapwindow - 2 * c->bw, false);
			wy += wh;
		} else {
			hideclient(c);
		}
	}
}

void ltgrid(Monitor* m, int n) {
	unsigned int tilex, tiley, x, y, ww, wh, i, a = 1;
	Client* c;

	if (n == 1) {
		c = nexttiled(m->clients);
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, false);
		return;
	}

	while (1) {
		if (a * a >= n) {
			tilex = tiley = a;
			break;
		}
		if (a * (a + 1) >= n) {
			if (m->ww > m->wh) {
				tilex = a + 1;
				tiley = a;
			} else {
				tilex = a;
				tiley = a + 1;
			}
			break;
		}
		++a;
	}

	a = n % (m->ww > m->wh ? tiley : tilex); /* point to switch to regular grid */
	if (a == 0) {
		a = -1;
		ww = (m->ww + m->gapwindow) / tilex;
		wh = (m->wh + m->gapwindow) / tiley;
	} else  if (m->ww > m->wh) {
		ww = (m->ww + m->gapwindow) / tilex;
		wh = (m->wh + m->gapwindow) / a;
	} else {
		ww = (m->ww + m->gapwindow) / a;
		wh = (m->wh + m->gapwindow) / tiley;
	}
	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), ++i) {
		if (i == a) {
			ww = (m->ww + m->gapwindow) / tilex;
			wh = (m->wh + m->gapwindow) / tiley;
			i += (m->ww > m->wh ? tiley : tilex) - a; /* complete row or column */
		}
		if (m->ww > m->wh) {
			x = m->wx + (i / tiley) * ww;
			y = m->wy + (i % tiley) * wh;
		} else {
			x = m->wx + (i % tilex) * ww;
			y = m->wy + (i / tilex) * wh;
		}
		resize(c, x, y, ww - m->gapwindow - 2 * c->bw, wh - m->gapwindow - 2 * c->bw, false);
	}

}

void ltmonocle(Monitor* m, int n) {
	Client* c;
	for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, false);
}

/* command implementations */

void cmdfocusclient(const Arg arg) {
	if (arg.v) focus((Client*)arg.v);
}

void cmdfocusmon(const Arg arg) {
	Monitor* m;
	if (!mons->next)
		return;
	if ((m = dirtomon(arg.i)) == selmon)
		return;
	focusmon(m, true);
}

void cmdfocusstack(const Arg arg) {
	Client* c = NULL;
	Client* i;
	if (!selmon->sel)
		return;
	if (arg.i > 0) {
		for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
		if (!c)
			for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
	} else {
		for (i = selmon->clients; i != selmon->sel; i = i->next)
			if (ISVISIBLE(i))
				c = i;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i))
					c = i;
	}
	if (c) {
		focus(c);
		restack(selmon);
	}
}

void cmdinclayout(const Arg arg) {
	int n = arg.i, i;
	for (i = 0; i < abs(n); ++i) {
		if (n < 0) {
			if (selmon->lt[selmon->sellt] == layouts)
				selmon->lt[selmon->sellt] = layouts + LENGTH(layouts) - 1;
			else
				--selmon->lt[selmon->sellt];
		} else {
			if (selmon->lt[selmon->sellt] == layouts + LENGTH(layouts) - 1)
				selmon->lt[selmon->sellt] = layouts;
			else
				++selmon->lt[selmon->sellt];
		}
	}
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

void cmdincnmaster(const Arg arg) {
	selmon->nmaster = MAX(selmon->nmaster + arg.i, 0);
	arrange(selmon);
}

void cmdkillclient(const Arg arg) {
	killclient(selmon->sel);
}

void cmdmovemouse(const Arg arg) {
	int x, y, ocx, ocy, nx, ny;
	Client* c;
	Monitor* m;
	XEvent ev;
	Time lasttime = 0;
	int moved = 0;

	if (!(c = selmon->sel))
		return;
	if (getrootptr(&x, &y) == false)
		return;
	if (XGrabPointer(dpy, root, false, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove], CurrentTime) != GrabSuccess)
		return;
	grabbedclient = c;

	ocx = c->x;
	ocy = c->y;
	focus(c);
	do {
		XNextEvent(dpy, &ev);
		if (ev.type == MotionNotify) {
			if ((ev.xmotion.time - lasttime) > (1000 / 60)) {
				lasttime = ev.xmotion.time;
				nx = ocx + (ev.xmotion.x - x);
				ny = ocy + (ev.xmotion.y - y);
				if (abs(selmon->wx - nx) < snap)
					nx = selmon->wx;
				else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
					nx = selmon->wx + selmon->ww - WIDTH(c);
				if (abs(selmon->wy - ny) < snap)
					ny = selmon->wy;
				else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
					ny = selmon->wy + selmon->wh - HEIGHT(c);
				/* arg.i doesnt do anything rn */
				resizeclient(c, nx, ny, c->w, c->h);
				if (!moved) {
					moved = 1;
					c->isalwaysontop = 0;
					setposition(c, PositionNone, false);
				}
			}
		} else {
			eventhandle(&ev);
			if (
				grabbedclient != c || /* this window has probably been destroyed */
				(moved && c->position != PositionNone) /* something else has taken control of this window */
			) {
				XUngrabPointer(dpy, CurrentTime);
				return;
			}
		}
	} while (ISVISIBLE(c) && ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if (moved) {
		x = ev.xbutton.x_root;
		y = ev.xbutton.y_root;
		if ((m = postomon(x, y)) != selmon) {
			sendmon(c, m, true);
		}
	} else {
		setmaster(c);
	}
}

void cmdresizemouse(const Arg arg) {
	int x, y, ocx, ocy, ocw, och, nw, nh, aw, ah;
	Client* c;
	XEvent ev;
	Time lasttime = 0;
	bool moved = false;

	if (!(c = selmon->sel))
		return;
	if (XGrabPointer(dpy, root, false, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize], CurrentTime) != GrabSuccess)
		return;
	grabbedclient = c;

	ocx = c->x;
	ocy = c->y;
	ocw = c->w;
	och = c->h;
	if (resizemousewarp) {
		x = c->x + c->w + c->bw;
		y = c->y + c->h + c->bw;
		XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw, c->h + c->bw);
	} else {
		if (!getrootptr(&x, &y))
			return;
	}
	focus(c);
	do {
		XNextEvent(dpy, &ev);
		if (ev.type == MotionNotify) {
			if ((ev.xmotion.time - lasttime) > (1000 / 60)) {
				lasttime = ev.xmotion.time;
				nw = ocw + (ev.xmotion.x - x);
				nh = och + (ev.xmotion.y - y);
				if (nw != c->w && nh != c->h) {
					aw = MAX(abs(nw), 1);
					ah = MAX(abs(nh), 1);
					if (arg.i)
						resizeclient(c,
							nw < 0 ? ocx - aw : ocx, nh < 0 ? ocy - ah : ocy,
							aw, ah
						);
					else
						resize(c,
							nw < 0 ? ocx - aw : ocx, nh < 0 ? ocy - ah : ocy,
							aw, ah,
							true
						);
					if (!moved || c->position != PositionNone) {
						moved = true;
						c->isalwaysontop = false;
						setposition(c, PositionNone, false);
					}
				}
			}
		} else {
			eventhandle(&ev);
			if (
				grabbedclient != c || /* this window has probably been destroyed */
				(moved && c->position != PositionNone) /* something else has taken control of this window */
			) {
				XUngrabPointer(dpy, CurrentTime);
				return;
			}
		}
	} while (ISVISIBLE(c) && ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));

}

void cmdquit(const Arg arg) {
	running = false;
}

void cmdsetgapwindow(const Arg arg) {
	if ((arg.i == 0) || (selmon->gapwindow + arg.i < 0))
		selmon->gapwindow = 0;
	else
		selmon->gapwindow += arg.i;
	arrange(selmon);
}

void cmdsetgapbar(const Arg arg) {
	#ifndef NODRW
		if ((arg.i == 0) || (selmon->gapbar + arg.i < 0))
			selmon->gapbar = 0;
		else
			selmon->gapbar += arg.i;
		updatebarpos(selmon);
		XMoveResizeWindow(dpy, selmon->barwin, selmon->bx, selmon->by, selmon->bw, selmon->bh);
		arrange(selmon);
		drawbar(selmon);
	#endif /* NODRW */
}

void cmdsetgapedge(const Arg arg) {
	if ((arg.i == 0) || (selmon->gapedge + arg.i < 0))
		selmon->gapedge = 0;
	else
		selmon->gapedge += arg.i;
	updatebarpos(selmon);
	arrange(selmon);
	drawbar(selmon);
}

void cmdsetlayout(const Arg arg) {
	if (!arg.v || arg.v != selmon->lt[selmon->sellt])
		selmon->sellt ^= 1;
	if (arg.v)
		selmon->lt[selmon->sellt] = (Layout*)arg.v;
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

void cmdsetmfact(const Arg arg) {
	/* arg > 1.0 will set mfact absolutely */
	float f;
	if (!arg.v || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg.f < 1.0 ? arg.f + selmon->mfact : arg.f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
	selmon->mfact = f;
	arrange(selmon);
}

void cmdsetposition(const Arg arg) {
	if (!selmon->sel) return;
	setposition(selmon->sel, arg.i & (~PositionForced), (bool)(arg.i & PositionForced));
	focus(selmon->sel);
}

void cmdsendmon(const Arg arg) {
	Monitor* m;
	if (!selmon->sel)
		return;
	if (!mons->next)
		return;
	if ((m = dirtomon(arg.i)) == selmon)
		return;
	if (selmon == m)
		return;
	sendmon(selmon->sel, m, true);
}

void cmdspawn(const Arg arg) {
	struct sigaction sa;

	if ((char**)arg.v == (char**)dmenucmd)
		dmenumon[0] = '0' + selmon->num;

	if (fork() == 0) {
		char** p;
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_DFL;
		sigaction(SIGCHLD, &sa, NULL);

		printf("dwm: execvp");
		for (p = (char**)arg.v; *p != NULL; ++p) printf(" \"%s\"", *p);
		putchar('\n');

		execvp(((char**)arg.v)[0], (char**)arg.v);
		die("dwm: execvp \"%s\" failed:", ((char**)arg.v)[0]);
	}
}

void cmdtag(const Arg arg) {
	if (selmon->sel && arg.ui & TAGMASK) {
		selmon->sel->tags = arg.ui & TAGMASK;
		focus(NULL);
		arrange(selmon);
	}
}

void cmdtogglebar(const Arg arg) {
	#ifndef NODRW
		selmon->showbar = !selmon->showbar;
		updatebarpos(selmon);
		if (selmon->showbar) {
			XMoveResizeWindow(dpy, selmon->barwin, selmon->bx, selmon->by, selmon->bw, selmon->bh);
			XMapWindow(dpy, selmon->barwin);
		} else {
			XUnmapWindow(dpy, selmon->barwin);
		}
		arrange(selmon);
	#endif /* NODRW */
}

void cmdtogglefloating(const Arg arg) {
	togglefloating(selmon->sel);
}

void cmdtogglealwaysontop(const Arg arg) {
	Client *c, *d;
	if (arg.ui && arg.ui != 1)
		c = (Client*)arg.v;
	else if (selmon->sel)
		c = selmon->sel;
	else
		return;
	if (c->isalwaysontop) {
		c->isalwaysontop = false;
	} else {
		if (arg.ui == 1 && c->mon->clients) {
			for (d = c->mon->clients; d; d = d->next)
				if (ISVISIBLE(d))
					d->isalwaysontop = false;
		}
		c->isalwaysontop = true;
		/* setposition(c, PositionNone, false); */
	}
	arrange(c->mon);
}

void cmdtoggletag(const Arg arg) {
	unsigned int newtags;
	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg.ui & TAGMASK);
	if (newtags) {
		selmon->sel->tags = newtags;
		focus(NULL);
		arrange(selmon);
	}
}

void cmdtoggleview(const Arg arg) {
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg.ui & TAGMASK);
	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;
		focus(NULL);
		arrange(selmon);
	}
}

void cmdview(const Arg arg) {
	if ((arg.ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg.ui & TAGMASK)
		selmon->tagset[selmon->seltags] = arg.ui & TAGMASK;
	focus(NULL);
	arrange(selmon);
}

void cmdwinview(const Arg arg) {
	/* Selects for the view of the focused window. The list of tags */
	/* to be displayed is matched to the focused window tag list. */
	Window win, win_r, win_p, * win_c;
	unsigned nc;
	int unused;
	Client* c;

	if (!XGetInputFocus(dpy, &win, &unused)) return;
	while (XQueryTree(dpy, win, &win_r, &win_p, &win_c, &nc)
		&& win_p != win_r) win = win_p;

	if (!(c = wintoclient(win))) return;

	cmdview((Arg){ .ui = c->tags });
}

void cmdzoom(const Arg arg) {
	Client* c = selmon->sel;
	if (!selmon->lt[selmon->sellt]->arrange || !c || c->position == PositionNone)
		return;
	if (c == nexttiled(selmon->clients) && !(c = nexttiled(c->next)))
		return;
	pop(c);
}

/* event implementations */

void eventbuttonpress(XEvent* e) {
	unsigned int i, click;
	Arg arg;
	Client* c;
	Monitor* m;
	XButtonPressedEvent* ev = &e->xbutton;
	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = postomon(ev->x_root, ev->y_root)) && (focusmononwheel || (ev->button != Button4 && ev->button != Button5)))
		focusmon(m, true);
	#ifndef NODRW
		if (ev->window == m->barwin) {
			buttonbar(ev, &arg, &click);
	#endif /* NODRW */
	} else if ((c = wintoclient(ev->window))) {
		if (focusonwheel || (ev->button != Button4 && ev->button != Button5))
			focus(c);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
			&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(arg.i == 0 ? buttons[i].arg : arg);
}

void eventclientmessage(XEvent* e) {
	XClientMessageEvent* cme = &e->xclient;
	Client* c = wintoclient(cme->window);

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen] || cme->data.l[2] == netatom[NetWMFullscreen]) {
			if (
				cme->data.l[0] == 1 || /* _NET_WM_STATE_ADD    */
				(cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && c->position != PositionFullscreen)
			) {
				setposition(c, PositionFullscreen, false);
				focus(c);
			} else {
				setposition(c, c->oldposition, false);
			}
		}
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c != selmon->sel && !c->isurgent)
			seturgent(c, true);
	}
}

void eventconfigurenotify(XEvent* e) {
	Monitor* m;
	Client* c;
	XConfigureEvent* ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		for (m = mons; m; m = m->next) {
			m->oldmx = m->mx;
			m->oldmy = m->my;
			m->oldmw = m->mw;
			m->oldmh = m->mh;
		}
		if (updategeom() || dirty) {
			#ifndef NODRW
				drw_resize(drw, sw, sh);
			#endif /* NODRW */
			updatebars();
			for (m = mons; m; m = m->next) {
				for (c = m->clients; c; c = c->next)
					if (c->position == PositionNone)
						resizeclient(c,
							c->x * m->mw / m->oldmw,
							c->y * m->mh / m->oldmh,
							c->w * m->mw / m->oldmw,
							c->h * m->mh / m->oldmh
						);
				#ifndef NODRW
					XMoveResizeWindow(dpy, m->barwin, m->bx, m->by, m->bw, m->bh);
				#endif /* NODRW */
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void eventconfigurerequest(XEvent* e) {
	Client* c;
	Monitor* m;
	XConfigureRequestEvent* ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else if (c->ignorehints)
			;
		else if (c->position == PositionNone || !selmon->lt[selmon->sellt]->arrange) {
			m = c->mon;
			if (ev->value_mask & CWX) {
				c->oldx = c->x;
				c->x = m->mx + ev->x;
			}
			if (ev->value_mask & CWY) {
				c->oldy = c->y;
				c->y = m->my + ev->y;
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > m->mx + m->mw && c->position == PositionNone)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->position == PositionNone)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
			if ((ev->value_mask & (CWX | CWY)) && !(ev->value_mask & (CWWidth | CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		} else
			configure(c);
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, false);
}

void eventdestroynotify(XEvent* e) {
	Client* c;
	XDestroyWindowEvent* ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, true);
}

void evententernotify(XEvent *e) {
	Client *c;
	XCrossingEvent *ev = &e->xcrossing;
	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	
	c = wintoclient(ev->window);
	if (!c) return;
	if (!focusonhover) {
		if (focusmononhover)
			focusmon(c->mon, true);
		return;
	}
	focus(c);
}


void eventexpose(XEvent* e) {
	Monitor* m;
	XExposeEvent* ev = &e->xexpose;
	if (ev->count == 0 && (m = wintomon(ev->window)))
		drawbar(m);
}

void eventmotionnotify(XEvent *e) {
	Monitor *m;
	Client *c;
	XMotionEvent *ev = &e->xmotion;

	if (focusmononhover == 0) return;
	if (focusmononhover && (m = postomon(ev->x_root, ev->y_root)) != selmon)
		focusmon(m, true);

	if (
		ev->window != root
		#ifndef NODRW
			&& ev->window != selmon->barwin
		#endif /* NODRW */
		&& (selmon->sel == NULL || selmon->sel->win != ev->window)
		&& (c = wintoclient(ev->window))) {
		focus(c);
	}
}

void eventfocusin(XEvent* e) {
	/* there are some broken focus acquiring clients needing extra handling */
	XFocusChangeEvent* ev = &e->xfocus;
	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void eventkeypress(XEvent* e) {
	unsigned int i;
	KeySym keysym;
	XKeyEvent* ev;

	ev = &e->xkey;
	/* keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0); */
	keysym = XkbKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
			&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
			&& keys[i].func)
			keys[i].func(keys[i].arg);
}

void eventmappingnotify(XEvent* e) {
	XMappingEvent* ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void eventmaprequest(XEvent* e) {
	static XWindowAttributes wa;
	XMapRequestEvent* ev = &e->xmaprequest;
	if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect)
		return;
	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
}

void eventpropertynotify(XEvent* e) {
	Client* c;
	Window trans;
	XPropertyEvent* ev = &e->xproperty;

	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch (ev->atom) {
			default: break;
			case XA_WM_TRANSIENT_FOR:
				if (c->position != PositionNone && (XGetTransientForHint(dpy, c->win, &trans)) && wintoclient(trans) != NULL) {
					setposition(c, PositionNone, false);
				}
				break;
			case XA_WM_NORMAL_HINTS:
				c->hintsvalid = false;
				break;
			case XA_WM_HINTS:
				updatewmhints(c);
				drawbars();
				break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		} else if (ev->atom == netatom[NetWMIcon]) {
			updateicon(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void eventunmapnotify(XEvent* e) {
	Client* c;
	XUnmapEvent* ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, false);
	}
}

void eventhandle(XEvent* ev) {
	if (eventhandler[ev->type])
		eventhandler[ev->type](ev); /* call eventhandler */
}

/* function implementations */

void applyrules(Client* c) {
	const char* class;
	const char* instance;
	unsigned int i;
	const Rule* r;
	Monitor* m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->position = PositionNone;
	c->tags = 0;
	XGetClassHint(dpy, c->win, &ch);
	class = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name ? ch.res_name : broken;

	c->ignorehints = false;
	for (i = 0; i < LENGTH(ignorehintsmatch); ++i) {
		if (strcmp(class, ignorehintsmatch[i]) == 0) {
			c->ignorehints = true;
			break;
		}
	}
	if (!c->ignorehints) {
		for (i = 0; i < LENGTH(ignorehintscontains); ++i) {
			if (strstr(class, ignorehintscontains[i]) == 0) {
				c->ignorehints = true;
				break;
			}
		}
	}

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
			&& (!r->class || strstr(class, r->class))
			&& (!r->instance || strstr(instance, r->instance)))
		{
			c->position = r->position; /* later applied */
			c->tags |= r->tags;
			for (m = mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int applysizehints(Client* c, int* x, int* y, int* w, int* h, bool interact) {
	int baseismin;
	Monitor* m = c->mon;

	/* set minimum possible */
	*w = MAX(10, *w);
	*h = MAX(10, *h);
	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (resizehints && !c->ignorehints) {
		if (!c->hintsvalid)
			updatesizehints(c);
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if (baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void arrange(Monitor* m) {
	XEvent ev;
	if (m)
		showhide(m->stack);
	else for (m = mons; m; m = m->next)
		showhide(m->stack);
	if (m) {
		arrangemon(m);
		restack(m);
	} else {
		for (m = mons; m; m = m->next)
			arrangemon(m);
		XSync(dpy, false);
		while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	}
}

void arrangemon(Monitor* m) {
	Client* c;
	int n = 0;
	for (c = m->clients; c; c = c->next) {
		if (ISVISIBLE(c)) {
			if (c->position != PositionNone && c->position != PositionTiled)
				setpositionmove(c, c->position, false);
			if (c->position == PositionTiled)
				++n;
		}
	}
	if (n && m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m, n);

}

void attach(Client* c) {
	if (!c) return;
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void attachstack(Client* c) {
	if (!c) return;
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

static void autostartexec() {
	/* execute command from autostart array */
	pid_t pid = getpid();
	if (fork() == 0) {
		static const struct timespec ts = { 0, 500000000L }; /* 500ms */
		const char* const* p;
		int i;
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		for (p = autostart; *p; i++, p++) {
			int disown = *p == AUTOSTARTDISOWN ? 1 : 0;
			if (disown) ++p;
			if (fork() == 0) {
				if (disown) setsid();
				execvp(*p, (char* const*)p);
				_exit(EXIT_FAILURE);
			}
			printf("dwm: execvp");
			for (/* empty */; *p != NULL; ++p) printf(" \"%s\"", *p);
			putchar('\n');
		}
		while (kill(pid, 0) > -1)
			nanosleep(&ts, NULL);
		kill(0, SIGKILL);
		_exit(EXIT_FAILURE);
	}
}

void checkotherwm(void) {
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, false);
	XSetErrorHandler(xerror);
	XSync(dpy, false);
}

void cleanup(void) {
	Layout foo = { "", NULL };
	Monitor* m;
	size_t i;

	cmdview((Arg){ .ui = ~0 });
	selmon->lt[selmon->sellt] = &foo;
	for (m = mons; m; m = m->next) {
		Client* next = m->stack;
		while (next) {
			m->stack = next;
			next = next->next;
			freeclient(m->stack);
		}
	}
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	while (mons)
		cleanupmon(mons);
	for (i = 0; i < LENGTH(colors); i++)
		free(scheme[i]);
	free(scheme);
	XDestroyWindow(dpy, wmcheckwin);
	#ifndef NODRW
		drw_free(drw);
	#endif /* NODRW */
	XSync(dpy, false);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);

}

void cleanupmon(Monitor* mon) {
	Monitor* m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	#ifndef NODRW
		XUnmapWindow(dpy, mon->barwin);
		XDestroyWindow(dpy, mon->barwin);
	#endif /* NODRW */
	free(mon);
}

void configure(Client* c) {
	XConfigureEvent ce;
	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = false;
	XSendEvent(dpy, c->win, false, StructureNotifyMask, (XEvent*)&ce);
}

Monitor* createmon(void) {
	Monitor* m;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->gapwindow = gapwindow;
	m->gapbar = gapbar;
	m->gapedge = gapedge;
	#ifndef NODRW
		m->topbar = topbar;
		m->showbar = showbar;
		m->bh = bh;
	#endif /* NODRW */
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];

	return m;
}

void detach(Client* c) {
	Client** tc;
	if (!c) return;
	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void detachstack(Client* c) {
	Client** tc, * t;
	if (!c) return;
	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

Monitor* dirtomon(int dir) {
	Monitor* m = NULL;
	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}

void buttonbar(XButtonPressedEvent* ev, Arg* arg, unsigned int* click) {
	#ifndef NODRW
		int x = 0, w, tw = 0;
		unsigned int i, n = 0;
		Client* c;
		Monitor* m = selmon;

		for (i = 0; i < LENGTH(tags); i++) {
			tw = TEXTW(tags[i]) + textpad * 2;
			if (tw < m->bh) tw = m->bh;
			x += tw;
			if (ev->x < x) {
				*click = ClkTagBar;
				arg->ui = 1 << i;
				return;
			}
		}

		if (m->lt[m->sellt]->symbol[0]) {
			tw = TEXTW(m->lt[m->sellt]->symbol) + textpad * 2;
			x += tw;
			if (ev->x < x) {
				*click = ClkLtSymbol;
				return;
			}
		}

		w = m->bw - x;

		if (m == selmon) { /* status is only drawn on selected monitor */
			tw = TEXTW(stext) + textpad * 2;
			if (tw > w) tw = w;
			if (ev->x > m->bx + m->bw - tw) {
				*click = ClkStatusText;
				return;
			}
			w -= tw;
		}
		
		if (w > 0) {
			for (c = m->clients; c; c = c->next) {
				if (ISVISIBLE(c))
					n++;
			}
			if (n > 0) {
				for (c = m->clients; c; c = c->next) {
					if (!ISVISIBLE(c))
						continue;
					x += w / n;
					if (ev->x < x) {
						*click = ClkWinTitle;
						arg->v = (const void*)c;
						return;
					}
				}
			}
		}
		*click = ClkStatusText;
	#endif /* NODRW */
}

void drawbar(Monitor* m) {
	#ifndef NODRW
		int indn;
		int x = 0, w, tw = 0, ew = 0, iw = 0;
		unsigned int i, n;
		Client* c;

		if (!m->showbar)
			return;

		for (i = 0; i < LENGTH(tags); i++) {
			drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
			tw = TEXTW(tags[i]) + textpad * 2;
			if (tw < m->bh) {
				drw_text(drw, x, 0, m->bh, m->bh, (m->bh - tw) / 2 + textpad, tags[i], false);
				tw = m->bh;
			} else {
				drw_text(drw, x, 0, tw, m->bh, textpad, tags[i], false);
			}
			for (indn = 0, c = m->clients; c; c = c->next) {
				if (c->tags & (1 << i)) {
					drw_rect(drw, x + 1 + (indn * 4), m->bh - 4, 3, 3, selmon->sel == c, false);
					indn++;
				}
			}
			x += tw;
		}

		if (m->lt[m->sellt]->symbol[0]) {
			drw_setscheme(drw, scheme[SchemeNorm]);
			tw = TEXTW(m->lt[m->sellt]->symbol);
			x = drw_text(drw, x, 0, bh, m->bh, (bh - tw) / 2, m->lt[m->sellt]->symbol, false);
		}

		w = m->bw - x;

		if (m == selmon && w > 0) { /* status is only drawn on selected monitor */
			drw_setscheme(drw, scheme[SchemeNorm]);
			tw = TEXTW(stext) + textpad * 2;
			if (tw > w) tw = w;
			drw_text(drw, m->bw - tw, 0, tw, m->bh, textpad, stext, false);
			w -= tw;
		}

		if (w > 0) {
			for (n = 0, c = m->clients; c; c = c->next) {
				if (ISVISIBLE(c)) ++n;
			}
			if (n > 0) {
				for (c = m->clients; c; c = c->next) {
					if (!ISVISIBLE(c))
						continue;
					ew = w / n;
					drw_setscheme(drw, scheme[m == selmon && m->sel == c ? SchemeSel : SchemeNorm]);
					if (c->icon) {
						iw = c->icw;
						if (iw > ew * 1.2) iw = 0;
					} else {
						iw = 0;
					}
					if (c->name[0] != 0) {
						tw = TEXTW(c->name) - iw;
						tw = MIN(tw, ew - iw - textpad * 2);
						if (tw < 0) tw = 0;
					} else {
						tw = 0;
					}
					if (tw && iw) {
						if (tw < ew - 2 * iw - 4 * textpad) {
							drw_text(drw, x, 0, ew, m->bh, (ew - tw) / 2, c->name, false);
							drw_pic(drw, x + textpad, (m->bh - c->ich) / 2, c->icw, c->ich, c->icon);
						} else {
							drw_rect(drw, x, 0, iw + 2 * textpad, m->bh, true, true);
							drw_pic(drw, x + textpad, (m->bh - c->ich) / 2, c->icw, c->ich, c->icon);
							drw_text(drw, x + (iw + 2 * textpad), 0, ew - (iw + 2 * textpad), m->bh, 0, c->name, false);
						}
					} else if (iw) {
						drw_rect(drw, x, 0, ew, m->bh, true, true);
						if (iw > ew)
							drw_pic(drw, x, (m->bh - c->ich) / 2, c->icw, c->ich, c->icon);
						else
							drw_pic(drw, x + (ew - iw) / 2, (m->bh - c->ich) / 2, c->icw, c->ich, c->icon);
					} else if (tw) {
						drw_text(drw, x, 0, ew, m->bh, (ew - tw) / 2, c->name, false);
					} else {
						drw_rect(drw, x, 0, ew, m->bh, true, true);
					}
					if (c->isalwaysontop) drw_rect(drw, x + 1, true, 4, 4, 0, false);
					x += ew;
				}
				w -= (w / n) * n;
			}
		}

		drw_setscheme(drw, scheme[SchemeNorm]);
		drw_rect(drw, x, 0, w, m->bh, true, true);
		drw_map(drw, m->barwin, 0, 0, m->bw, m->bh);
	#endif /* NODRW */
}

void drawbars(void) {
	#ifndef NODRW
		Monitor* m;
		for (m = mons; m; m = m->next)
			drawbar(m);
	#endif /* NODRW */
}

void focus(Client* c) {
	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
	if (selmon->sel)
		unfocus(selmon->sel, false);
	if (c) {
		focusmon(c->mon, false);
		if (c->isurgent)
			seturgent(c, false);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, true);
		if (c->position == PositionFullscreen && opacityfullscreen != -1)
			opacity(c, opacityfullscreen);
		else
			opacity(c, opacityfocus);
		XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
		setfocus(c);
		XRaiseWindow(dpy, c->win);
	} else {
		#ifdef NODRW
			XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		#else
			XSetInputFocus(dpy, selmon->barwin, RevertToPointerRoot, CurrentTime);
		#endif /* NODRW */
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	drawbars();
}

void focusmon(Monitor* m, bool refocus) {
	if (selmon == m)
		return;
	if (opacityfocus != opacityunfocus) {
		opacitywin(selmon->barwin, opacityunfocus);
		opacitywin(m->barwin, opacityfocus);
	}
	if (selmon->sel)
		unfocus(selmon->sel, false);
	selmon = m;
	selmon->sel = selmon->stack;
	if (refocus)
		focus(NULL);
}

Atom getatomprop(Client* c, Atom prop) {
	unsigned char* p = NULL;
	Atom type = None, atom = None;
	int format = 0;

	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, false, XA_ATOM,
		&type, &format, (unsigned long*)dummyptr, (unsigned long*)dummyptr, &p) == Success && p) {
		if (format == 0 && type == XA_ATOM)
			atom = *(Atom*)p;
		XFree(p);
	}
	return atom;
}

unsigned int prealpha(unsigned int p) {
	unsigned char a = p >> 24u;
	unsigned int rb = (a * (p & 0xFF00FFu)) >> 8u;
	unsigned int g = (a * (p & 0x00FF00u)) >> 8u;
	return (rb & 0xFF00FFu) | (g & 0x00FF00u) | (a << 24u);
}

int getrootptr(int* x, int* y) {
	return XQueryPointer(dpy, root, (Window*)dummyptr, (Window*)dummyptr, x, y, (int*)dummyptr, (int*)dummyptr, (unsigned int*)dummyptr);
}

unsigned long getstate(Window w) {
	int format;
	unsigned long result = -1;
	unsigned char* p = NULL;
	unsigned long n, extra;
	Atom real;
	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, false, wmatom[WMState],
		&real, &format, &n, &extra, (unsigned char**)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

int gettextprop(Window w, Atom atom, char* text, unsigned int size) {
	char** list = NULL;
	int n;
	XTextProperty name;
	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING) {
		strncpy(text, (char*)name.value, size - 1);
	} else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
		strncpy(text, *list, size - 1);
		XFreeStringList(list);
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void grabbuttons(Client* c, bool focused) {
	unsigned int i, j;
	updatenumlockmask();
	XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
	if (!focused)
		XGrabButton(dpy, AnyButton, AnyModifier, c->win, false,
			BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
	for (i = 0; i < LENGTH(buttons); i++)
		if (buttons[i].click == ClkClientWin)
			for (j = 0; j < LENGTH(modifiers); j++)
				XGrabButton(dpy, buttons[i].button,
					buttons[i].mask | modifiers[j],
					c->win, false, BUTTONMASK,
					GrabModeAsync, GrabModeSync, None, None);
}

void grabkeys(void) {
	unsigned int i, j, k;
	int start, end, skip;
	KeySym* syms;
	updatenumlockmask();
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	XDisplayKeycodes(dpy, &start, &end);
	syms = XGetKeyboardMapping(dpy, start, end - start + 1, &skip);
	if (!syms)
		return;
	for (k = start; k <= end; k++)
		for (i = 0; i < LENGTH(keys); i++)
			/* skip modifier codes, we do that ourselves */
			if (keys[i].keysym == syms[(k - start) * skip])
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, k,
						keys[i].mod | modifiers[j],
						root, True,
						GrabModeAsync, GrabModeAsync);
	XFree(syms);
}

void killclient(Client* c) {
	if (!c) return;
	if (!sendevent(c, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, c->win);
		XSync(dpy, false);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

#ifdef XINERAMA
	static bool isuniquegeom(XineramaScreenInfo* unique, size_t n, XineramaScreenInfo* info) {
		while (n--)
			if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
				&& unique[n].width == info->width && unique[n].height == info->height)
				return false;
		return true;
	}
#endif /* XINERAMA */

Client* manage(Window w, XWindowAttributes* wa) {
	Client* c;
	Client* t = NULL;
	Window trans = None;
	XWindowChanges wc;
	c = ecalloc(1, sizeof(Client));
	c->win = w;
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = c->bw = borderwidth;

	updateicon(c);
	updatetitle(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tags = t->tags;
	} else {
		c->mon = selmon;
		applyrules(c);
	}

	if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
		c->x = c->mon->wx + c->mon->ww - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
		c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->wx);
	c->y = MAX(c->y, c->mon->wy);

	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	XSelectInput(dpy, w, EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
	grabbuttons(c, false);
	if (c->position != PositionNone && (trans != None || c->isunresizeable))
		c->position = PositionNone; /* later applied */
	if (c->position == PositionNone)
		XRaiseWindow(dpy, c->win);
	attach(c);
	attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char*)&(c->win), true);
	hideclient(c); /* some windows require this */
	setclientstate(c, NormalState);
	c->opacity = -1;
	focus(c);
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	setfocus(c);
	return c;
}

Client* nexttiled(Client* c) {
	for (; c && (c->position != PositionTiled || !ISVISIBLE(c)); c = c->next);
	return c;
}

void opacity(Client* c, float opacity) {
	if (opacity == -1) return;
	if (c->opacity == opacity) return;
	opacitywin(c->win, opacity);
	c->opacity = opacity;
}

void opacitywin(Window w, float opacity) {
	/* https://dwm.suckless.org/patches/clientopacity/dwm-clientopacity-6.4.diff */
	if(opacity > 0 && opacity < 1) {
		static long opacitydata[1];
		opacitydata[0] = opacity * 0xffffffff;
		XChangeProperty(dpy, w, netatom[NetWMWindowsOpacity], XA_CARDINAL,
						32, PropModeReplace, (unsigned char*)opacitydata, true);
	} else {
		XDeleteProperty(dpy, w, netatom[NetWMWindowsOpacity]);
	}
}

void pop(Client* c) {
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

/* Monitor* recttomon(int x, int y, int w, int h) {
	Monitor* m;
	Monitor* r = selmon;
	int a, area = 0;
	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
} */

Monitor* postomon(int x, int y) {
	Monitor* m = selmon;
	if (m) {
		if (x >= m->mx && x < m->wx + m->mw && y >= m->my && y < m->my + m->mh)
			return m;
	}
	for (m = mons; m; m = m->next) {
		if (m == selmon) continue;
		if (x >= m->mx && x < m->wx + m->mw && y >= m->my && y < m->my + m->mh)
			return m;
	}
	return selmon;
}

void resize(Client* c, int x, int y, int w, int h, bool interact) {
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

void resizeclient(Client* c, int x, int y, int w, int h) {
	XWindowChanges wc;
	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, false);
}

void setpositionmove(Client* c, int position, bool force) {
	Monitor* m;
	int mw, mh, x, y, w, h;
	m = c->mon;
	mw = m->ww;
	mh = m->wh;
	switch (position) {
		case PositionTiled:
		case PositionNone:
			return;
		case PositionNW:
			x = 0;
			y = 0;
			w = mw / 2 - m->gapwindow / 2;
			h = mh / 2 - m->gapwindow / 2;
			break;
		case PositionW:
			x = 0;
			y = 0;
			w = mw / 2 - m->gapwindow / 2;
			h = mh;
			break;
		case PositionSW:
			x = 0;
			y = mh / 2 + m->gapwindow / 2;
			w = mw / 2 - m->gapwindow / 2;
			h = mh / 2 - m->gapwindow / 2;
			break;
		case PositionN:
			x = 0;
			y = 0;
			w = mw;
			h = mh / 2 - m->gapwindow / 2;
			break;
		case PositionFill:
			x = 0;
			y = 0;
			w = mw;
			h = mh;
			break;
		case PositionS:
			x = 0;
			y = mh / 2 + m->gapwindow / 2;
			w = mw;
			h = mh / 2 - m->gapwindow / 2;
			break;
		case PositionNE:
			x = mw / 2 + m->gapwindow / 2;
			y = 0;
			w = mw / 2 - m->gapwindow / 2;
			h = mh / 2 - m->gapwindow / 2;
			break;
		case PositionE:
			x = mw / 2 + m->gapwindow / 2;
			y = 0;
			w = mw / 2 - m->gapwindow / 2;
			h = mh;
			break;
		case PositionSE:
			x = mw / 2 + m->gapwindow / 2;
			y = mh / 2 + m->gapwindow / 2;
			w = mw / 2 - m->gapwindow / 2;
			h = mh / 2 - m->gapwindow / 2;
			break;
		case PositionCenter:
			if (force) {
				w = c->w + 2 * c->bw;
				h = c->h + 2 * c->bw;
			} else {
				x = 0;
				y = 0;
				w = mw / 2 - m->gapwindow / 2;
				h = mh / 2 - m->gapwindow / 2;
				applysizehints(c, &x, &y, &w, &h, true);
			}
			x = mw / 2 - w / 2;
			y = mh / 2 - h / 2;
			if (force) {
				if (x == c->x && y == c->y) return;
			}
			break;
		case PositionFullscreen:
			x = 0;
			y = 0;
			w = m->mw;
			h = m->mh;
			if (force)
				resizeclient(c, x + m->mx, y + m->my, w, h);
			else
				resize(c, x + m->mx, y + m->my, w, h, true);
			return;
		default:
			return;
	}
	if (force)
		resizeclient(c, x + m->wx, y + m->wy, w - c->bw * 2, h - c->bw * 2);
	else
		resize(c, x + m->wx, y + m->wy, w - c->bw * 2, h - c->bw * 2, true);
}

void setposition(Client* c, int position, bool force) {
	if (!c) return;
	if (c->position == position) {
		if (!force) return;
		c->oldposition = PositionNone;
	} else {
		c->oldposition = c->position;
	}
	c->position = position;
	if (c->oldposition == PositionTiled || c->position == PositionTiled) {
		arrange(c->mon);
	} else if (c->oldposition == PositionFullscreen) {
		c->bw = c->oldbw;
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)NULL, false);
		if (c->position == PositionNone) {
			resize(c, c->oldx, c->oldy, c->oldw, c->oldh, false);
			return;
		}
	} else if (c->position == PositionFullscreen) {
		c->oldbw = c->bw;
		c->bw = 0;
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], true);
	}
	setpositionmove(c, position, force);
}

void restack(Monitor* m) {
	Client* c;

	if (!m->sel) return;

	if (ISVISIBLE(m->sel))
		XRaiseWindow(dpy, m->sel->win);
	for (c = m->stack; c; c = c->snext)
		if (ISVISIBLE(c) && c->isalwaysontop)
			XRaiseWindow(dpy, c->win);

	XSync(dpy, false);

	drawbar(m);
}

void run(void) {
	XEvent ev;
	/* main event loop */
	XSync(dpy, false);
	while (running && !XNextEvent(dpy, &ev))
		eventhandle(&ev);
}

void scan(void) {
	unsigned int i, num;
	Window d1, d2;
	Window* wins = NULL;
	XWindowAttributes wa;
	if (!XQueryTree(dpy, root, &d1, &d2, &wins, &num)) return;
	for (i = 0; i < num; i++) {
		if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
			continue;
		if (wa.map_state == IsViewable) /* || getstate(wins[i]) == IconicState) */
			manage(wins[i], &wa);
	}
	for (i = 0; i < num; i++) { /* now the transients */
		if (!XGetWindowAttributes(dpy, wins[i], &wa))
			continue;
		if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
			manage(wins[i], &wa);
	}
	if (wins)
		XFree(wins);
	drawbars();
}

void sendmon(Client* c, Monitor* m, bool refocus) {
	if (c->mon == m)
		return;
	unfocus(c, true);
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
	attach(c);
	attachstack(c);
	focus(refocus ? c : NULL);
	arrange(NULL);
}

void setclientstate(Client* c, long state) {
	static long data[2] = { None, None };
	data[0] = state;
	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char*)data, 2);
}

int sendevent(Client* c, Atom proto) {
	int n;
	Atom* protocols;
	int exists = 0;
	XEvent ev;

	if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
		while (!exists && n--)
			exists = protocols[n] == proto;
		XFree(protocols);
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->win, false, NoEventMask, &ev);
	}
	return exists;
}

void setfocus(Client* c) {
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow],
			XA_WINDOW, 32, PropModeReplace,
			(unsigned char*)&(c->win), true);
	}
	sendevent(c, wmatom[WMTakeFocus]);
}

void setmaster(Client* master) {
	if (!master) return;
	detach(master);
	attach(master);
	focus(master);
	setposition(master, PositionTiled, true);
}

void setup(void) {
	XSetWindowAttributes wa;
	Atom utf8string;
	struct sigaction sa;
	int i;

	/* do not transform children into zombies when they terminate */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, NULL);

	/* clean up any zombies */
	while (waitpid(-1, NULL, WNOHANG) > 0) { /* nothing */}

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	#ifndef NODRW
		xinitvisual();
		drw = drw_create(dpy, screen, root, sw, sh, visual, depth, cmap);
		if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
			die("no fonts could be loaded.");
		textpad = drw->fonts->h / 2;
		bh = drw->fonts->h * 1.5;
	#endif
	updategeom();
	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", false);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", false);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", false);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", false);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", false);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", false);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", false);
	netatom[NetWMWindowsOpacity] = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", false);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", false);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", false);
	netatom[NetWMIcon] = XInternAtom(dpy, "_NET_WM_ICON", false);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", false);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", false);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", false);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", false);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", false);
	/* init cursors */
	cursor[CurNormal] = XCreateFontCursor(dpy, XC_left_ptr);
	cursor[CurResize] = XCreateFontCursor(dpy, XC_sizing);
	cursor[CurMove] = XCreateFontCursor(dpy, XC_fleur);
	/* init appearance */
	scheme = ecalloc(LENGTH(colors), sizeof(Clr*));
	for (i = 0; i < LENGTH(colors); ++i) {
		#ifdef NODRW
			scheme[i] = drw_scm_create(colors[i], alphas[i], 3);
		#else
			scheme[i] = drw_scm_create(drw, colors[i], alphas[i], 3);
		#endif
	}
	/* init bars */
	updatebars();
	updatestatus();
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, true, true, 0, 0, false);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char*)&wmcheckwin, true);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char*)"dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char*)&wmcheckwin, true);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char*)netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal];
	wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask
		| ButtonPressMask | PointerMotionMask | EnterWindowMask
		| StructureNotifyMask | PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
	focus(NULL);
}

void seturgent(Client* c, int urg) {
	XWMHints* wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

void hideclient(Client* c) {
	#ifdef NODRW
		if (sw > sh)
			XMoveWindow(dpy, c->win, c->x, sh * 2 + c->h * 2);
		else
			XMoveWindow(dpy, c->win, sw * 2 + c->w * 2, c->y);
	#else
		Monitor* m = c->mon;
		if (sw > sh) {
			// make window not intersect with bar
			if (m->topbar)
				XMoveWindow(dpy, c->win, c->x, sh * 2 + c->h * 2);
			else
				XMoveWindow(dpy, c->win, c->x, -sh - c->h * 2);
		} else {
			// windows go outwards to sides
			if (c->x + c->w / 2 < m->wx + m->ww / 2)
				XMoveWindow(dpy, c->win, sw * 2 + c->w * 2, c->y);
			else
				XMoveWindow(dpy, c->win, -sw - c->w * 2, c->y);
		}
	#endif
}

void showhide(Client* c) {
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->position == PositionNone) && c->position != PositionFullscreen)
			resize(c, c->x, c->y, c->w, c->h, false);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		hideclient(c);
	}
}

void togglefloating(Client* c) {
	if (c->position == PositionTiled) {
		c->position = PositionNone;
	} else {
		c->isalwaysontop = 0;
		c->position = PositionTiled;
	}
	arrange(c->mon);
}

void freeclient(Client* c) {
	freeicon(c);
	free(c);
}

void freeicon(Client* c) {
	#ifndef NODRW
		if (!c->icon) return;
		XRenderFreePicture(dpy, c->icon);
		c->icon = None;
	#endif
}

void unfocus(Client* c, bool setfocus) {
	if (!c)
		return;
	grabbuttons(c, false);
	if (c->position == PositionFullscreen && opacityfullscreen != -1)
		opacity(c, opacityfullscreen);
	else
		opacity(c, opacityunfocus);
	XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void unmanage(Client* c, bool destroyed) {
	Monitor* m = c->mon;
	XWindowChanges wc;
	detach(c);
	detachstack(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XSelectInput(dpy, c->win, NoEventMask);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, false);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	focus(NULL);
	updateclientlist();
	arrange(m);
	if (c == grabbedclient)
		grabbedclient = NULL;
	freeclient(c);
}

void updatebars(void) {
	#ifndef NODRW
	Monitor* m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixel = 0,
		.border_pixel = 0,
		.colormap = cmap,
		.event_mask = ButtonPressMask | ExposureMask
	};
	XClassHint ch = { "dwm", "dwm" };
	for (m = mons; m; m = m->next) {
		if (m->barwin)
			continue;
		m->barwin = XCreateWindow(dpy, root, m->bx, m->by, m->bw, m->bh, 0, depth,
			InputOutput, visual,
			CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWColormap | CWEventMask, &wa);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]);
		XMapRaised(dpy, m->barwin);
		XSetClassHint(dpy, m->barwin, &ch);
	}
	#endif
}

void updatebarpos(Monitor* m) {
	#ifdef NODRW
		m->wx = m->mx + m->gapedge;
		m->wy = m->my + m->gapedge;
		m->ww = m->mw - 2 * m->gapedge;
		m->wh = m->mh - 2 * m->gapedge;
	#else
		m->wx = m->mx + m->gapedge;
		if (!m->showbar) {
			m->wy = m->my + m->gapedge;
			m->ww = m->mw - 2 * m->gapedge;
			m->wh = m->mh - 2 * m->gapedge;
			m->bx = m->bw = 0;
			m->by = -m->bh;
			return;
		}
		m->bw = m->mw - 2 * m->gapbar;
		m->bx = m->mx + m->gapbar;
		m->ww = m->mw - 2 * m->gapedge;
		m->wh = m->mh - m->bh - 2 * m->gapedge - m->gapbar;
		if (m->topbar) {
			m->wy = m->my + m->bh + m->gapedge + m->gapbar;
			m->by = m->my + m->gapbar;
		} else {
			m->wy = m->my + m->gapedge;
			m->by = m->my + m->mh - m->bh - m->gapbar;
		}
	#endif
}

void updateclientlist(void) {
	Client* c;
	Monitor* m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList],
				XA_WINDOW, 32, PropModeAppend,
				(unsigned char*)&(c->win), true);
}

int updategeom(void) {
	bool dirty = false;

	#ifdef XINERAMA
		if (XineramaIsActive(dpy)) {
			int i, j, n, nn;
			Client* c;
			Monitor* m;
			XineramaScreenInfo* info = XineramaQueryScreens(dpy, &nn);
			XineramaScreenInfo* unique = NULL;

			for (n = 0, m = mons; m; m = m->next, n++);
			/* only consider unique geometries as separate screens */
			unique = ecalloc(nn, sizeof(XineramaScreenInfo));
			for (i = 0, j = 0; i < nn; i++)
				if (isuniquegeom(unique, j, &info[i]))
					memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
			XFree(info);
			nn = j;

			/* new monitors if nn > n */
			for (i = n; i < nn; i++) {
				for (m = mons; m && m->next; m = m->next);
				if (m)
					m->next = createmon();
				else
					mons = createmon();
			}
			for (i = 0, m = mons; i < nn && m; m = m->next, i++)
				if (i >= n
					|| unique[i].x_org != m->mx || unique[i].y_org != m->my
					|| unique[i].width != m->mw || unique[i].height != m->mh)
				{
					dirty = 1;
					m->num = i;
					m->mx = m->wx = unique[i].x_org;
					m->my = m->wy = unique[i].y_org;
					m->mw = m->ww = unique[i].width;
					m->mh = m->wh = unique[i].height;
					updatebarpos(m);
				}
			/* removed monitors if n > nn */
			for (i = nn; i < n; i++) {
				for (m = mons; m && m->next; m = m->next);
				while ((c = m->clients)) {
					dirty = true;
					m->clients = c->next;
					detachstack(c);
					c->mon = mons;
					attach(c);
					attachstack(c);
				}
				if (m == selmon)
					selmon = mons;
				cleanupmon(m);
			}
			free(unique);
		} else
	#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		mons->mx = mons->my = 0;
		if (mons->mw != sw || mons->mh != sh) {
			dirty = true;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

void updatenumlockmask(void) {
	unsigned int i, j;
	XModifierKeymap* modmap;
	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
	modifiers[2] = numlockmask;
	modifiers[3] = numlockmask | LockMask;
}

void updatesizehints(Client* c) {
	long msize;
	XSizeHints size;
	if (c->ignorehints) return;
	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isunresizeable = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
	c->hintsvalid = true;
}

void updatestatus(void) {
	#ifndef NODRW
		if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
			strcpy(stext, "dwm-"VERSION);
		drawbar(selmon);
	#endif /* NODRW */
}

void updatetitle(Client* c) {
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);
}

void updateicon(Client* c) {
	#ifndef NODRW
		int format;
		unsigned long n, extra;
		long* p = NULL;
		long* bstp = NULL;
		unsigned int w, h, sz;
		Atom real;

		freeicon(c);
		c->icon = None;

		if (XGetWindowProperty(dpy, c->win, netatom[NetWMIcon], 0L, LONG_MAX, false, AnyPropertyType,
			&real, &format, &n, &extra, (unsigned char**)&p) != Success)
			return;
		if (n == 0 || format != 32) { XFree(p); return; }

		{
			long* i;
			const long* end = p + n;
			unsigned int bstd = UINT32_MAX, d, m;
			for (i = p; i < end - 1; i += sz) {
				if ((w = *i++) >= 16384 || (h = *i++) >= 16384) { XFree(p); return; }
				if ((sz = w * h) > end - i) break;
				if ((m = w > h ? w : h) >= ICONSIZE && (d = m - ICONSIZE) < bstd) { bstd = d; bstp = i; }
			}
			if (!bstp) {
				for (i = p; i < end - 1; i += sz) {
					if ((w = *i++) >= 16384 || (h = *i++) >= 16384) { XFree(p); return; }
					if ((sz = w * h) > end - i) break;
					if ((d = ICONSIZE - (w > h ? w : h)) < bstd) { bstd = d; bstp = i; }
				}
			}
			if (!bstp) { XFree(p); return; }
		}

		if ((w = *(bstp - 2)) == 0 || (h = *(bstp - 1)) == 0) { XFree(p); return; }

		unsigned int icw, ich;
		if (w <= h) {
			ich = ICONSIZE; icw = w * ICONSIZE / h;
			if (icw == 0) icw = 1;
		} else {
			icw = ICONSIZE; ich = h * ICONSIZE / w;
			if (ich == 0) ich = 1;
		}
		c->icw = icw; c->ich = ich;

		unsigned int i, * bstp32 = (unsigned int*)bstp;
		for (sz = w * h, i = 0; i < sz; ++i) bstp32[i] = prealpha(bstp[i]);

		c->icon = drw_picture_create_resized(drw, (char*)bstp, w, h, icw, ich);
		XFree(p);
	#endif /* NODRW */
}

void updatewindowtype(Client* c) {
	if (getatomprop(c, netatom[NetWMState]) == netatom[NetWMFullscreen]) {
		setposition(c, PositionFullscreen, false);
		return;
	}
	if (getatomprop(c, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeDialog]) {
		setposition(c, PositionNone, false);
	}
}

void updatewmhints(Client* c) {
	XWMHints* wmh;
	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else
			c->isurgent = (bool)(wmh->flags & XUrgencyHint);
		if (wmh->flags & InputHint)
			c->neverfocus = (bool)(!wmh->input);
		else
			c->neverfocus = false;
		XFree(wmh);
	}
}

Client* wintoclient(Window w) {
	Client* c;
	Monitor* m;
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}

Monitor* wintomon(Window w) {
	int x, y;
	Client* c;
	if (w == root && getrootptr(&x, &y))
		return postomon(x, y);
	#ifndef NODRW
		for (Monitor* m = mons; m; m = m->next)
			if (w == m->barwin)
				return m;
	#endif /* NODRW */
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

int xerror(Display* dpy, XErrorEvent* ee) {
	/* There's no way to check accesses to destroyed windows, thus those cases are
	* ignored (especially on UnmapNotify's). Other types of errors call Xlibs
	* default error handler, which may call exit. */
	if (ee->error_code == BadWindow
		|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
		|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
		|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
		|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
		|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
		|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
		|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
		|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int xerrordummy(Display* dpy, XErrorEvent* ev) {
	(void)dpy; (void)ev;
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display* dpy, XErrorEvent* ev) {
	(void)dpy; (void)ev;
	die("dwm: another window manager is already running");
	return -1;
}

void xinitvisual() {
	#ifndef NODRW
		XVisualInfo* infos;
		XRenderPictFormat* fmt;
		int nitems;
		int i;

		XVisualInfo tpl = {
			.screen = screen,
			.depth = 32,
			.class = TrueColor
		};
		long masks = VisualScreenMask | VisualDepthMask | VisualClassMask;

		infos = XGetVisualInfo(dpy, masks, &tpl, &nitems);
		visual = NULL;
		for (i = 0; i < nitems; i++) {
			fmt = XRenderFindVisualFormat(dpy, infos[i].visual);
			if (fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
				visual = infos[i].visual;
				depth = infos[i].depth;
				cmap = XCreateColormap(dpy, root, visual, AllocNone);
				useargb = 1;
				break;
			}
		}

		XFree(infos);

		if (!visual) {
			visual = DefaultVisual(dpy, screen);
			depth = DefaultDepth(dpy, screen);
			cmap = DefaultColormap(dpy, screen);
		}
	#endif /* NODRW */
}

int main(int argc, char* argv[]) {
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-"VERSION);
	else if (argc != 1)
		die("usage: dwm [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display");
	checkotherwm();
	autostartexec();
	setup();
	#if defined(__OpenBSD__) && defined(BSDEnablePledge)
		if (pledge("stdio rpath proc exec", NULL) == -1)
			die("pledge");
	#endif /* __OpenBSD__ */
	scan();
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
