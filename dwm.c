/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
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
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>
#include <pango/pango.h>

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
							   * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_font_getwidth(drw, (X), False))
#define TEXTWM(X)               (drw_font_getwidth(drw, (X), True))
#define OPAQUE                  0xffU

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel }; /* color schemes */
enum {
	NetSupported, NetWMName, NetWMIcon, NetWMState, NetWMCheck,
	NetWMFullscreen, NetActiveWindow, NetWMWindowType,
	NetWMWindowTypeDialog, NetClientList, NetLast
}; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum {
	ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
	ClkClientWin, ClkRootWin, ClkLast
}; /* clicks */
enum {
	TileNW, TileW, TileSW, TileN, TileFill, TileS, TileNE, TileE,
	TileSE, TileFullscreen, TileCenter, TileDoubleFullscreen
}; /* Tiling */

typedef union Arg Arg;
union Arg {
	int i;
	unsigned int ui;
	float f;
	const void* v;
};

typedef struct Button Button;
struct Button {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg* arg);
	const Arg arg;
};

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256];
	float mina, maxa;
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
	int bw, oldbw;
	unsigned int tags;
	int isfixed, iscentered, isfloating, isalwaysontop, isurgent, neverfocus, oldstate, isfullscreen;
	unsigned int icw, ich;
	Picture icon;
	int issteam;
	Client* next;
	Client* snext;
	Monitor* mon;
	Window win;
};

typedef struct Key Key;
struct Key {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg*);
	const Arg arg;
};

typedef struct Layout Layout;
struct Layout {
	const char* symbol;
	void (*arrange)(Monitor*);
};

struct Monitor {
	float mfact;
	int nmaster;
	int num;
	int bx, by, bw, bh;   /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	int gappx;            /* gaps between windows */
	int restacking;       /* whether the monitor is being restacked */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	int showbar;
	int topbar;
	Client* clients;
	Client* sel;
	Client* stack;
	Monitor* next;
	Window barwin;
	const Layout* lt[2];
};

typedef struct Rule Rule;
struct Rule {
	const char* class;
	const char* instance;
	const char* title;
	unsigned int tags;
	int isfloating;
	int monitor;
};

/* function declarations */
static void applyrules(Client* c);
static int applysizehints(Client* c, int* x, int* y, int* w, int* h, int interact);
static void arrange(Monitor* m);
static void arrangemon(Monitor* m);
static void attach(Client* c);
static void attachstack(Client* c);
static void buttonbar(XButtonPressedEvent* ev, Arg* arg, unsigned int* click);
static void buttonpress(XEvent* e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor* mon);
static void clientmessage(XEvent* e);
static void col(Monitor*);
static void configure(Client* c);
static void configurenotify(XEvent* e);
static void configurerequest(XEvent* e);
static Monitor* createmon(void);
static void destroynotify(XEvent* e);
static void detach(Client* c);
static void detachstack(Client* c);
static Monitor* dirtomon(int dir);
static void drawbar(Monitor* m);
static void drawbars(void);
static void enternotify(XEvent *e);
static void expose(XEvent* e);
static void focus(Client* c);
static void focusclient(const Arg* arg);
static void focusin(XEvent* e);
static void focusmon(const Arg* arg);
static void focusstack(const Arg* arg);
static Atom getatomprop(Client* c, Atom prop);
static Picture geticonprop(Window w, unsigned int* icw, unsigned int* ich);
static int getrootptr(int* x, int* y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char* text, unsigned int size);
static void grabbuttons(Client* c, int focused);
static void grabkeys(void);
static void inclayout(const Arg* arg);
static void incnmaster(const Arg* arg);
static void keypress(XEvent* e);
static void killclient(const Arg* arg);
static void manage(Window w, XWindowAttributes* wa);
static void mappingnotify(XEvent* e);
static void maprequest(XEvent* e);
static void monocle(Monitor* m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg* arg);
static void moveresizetile(const Arg* arg);
static Client* nexttiled(Client* c);
static void pop(Client* c);
static void propertynotify(XEvent* e);
static void quit(const Arg* arg);
static Monitor* recttomon(int x, int y, int w, int h);
static void resize(Client* c, int x, int y, int w, int h, int interact);
static void resizeclient(Client* c, int x, int y, int w, int h);
static void resizemouse(const Arg* arg);
static void restack(Monitor* m);
static void run(void);
static void scan(void);
static int sendevent(Client* c, Atom proto);
static void sendmon(Client* c, Monitor* m);
static void setclientstate(Client* c, long state);
static void setfocus(Client* c);
static void setfullscreen(Client* c, int fullscreen);
static void setgaps(const Arg* arg);
static void setlayout(const Arg* arg);
static void setmaster(Client* c);
static void setmfact(const Arg* arg);
static void setup(void);
static void seturgent(Client* c, int urg);
static void showhide(Client* c);
static void spawn(const Arg* arg);
static void tag(const Arg* arg);
static void tagmon(const Arg* arg);
static void tile(Monitor* m);
static void togglebar(const Arg* arg);
static void togglefloating(const Arg* arg);
static void togglealwaysontop(const Arg* arg);
static void toggletag(const Arg* arg);
static void toggleview(const Arg* arg);
static void freeicon(Client* c);
static void unfocus(Client* c, int setfocus);
static void unmanage(Client* c, int destroyed);
static void unmapnotify(XEvent* e);
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
static void view(const Arg* arg);
static Client* wintoclient(Window w);
static Monitor* wintomon(Window w);
static void winview(const Arg* arg);
static int xerror(Display* dpy, XErrorEvent* ee);
static int xerrordummy(Display* dpy, XErrorEvent* ee);
static int xerrorstart(Display* dpy, XErrorEvent* ee);
static void xinitvisual();
static void zoom(const Arg* arg);
static void autostart_exec(void);

/* variables */
static const char broken[] = "broken";
static char stext[512];
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static unsigned int textpad; /* Padding for text */
static int (*xerrorxlib)(Display*, XErrorEvent*);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent*) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast];
static int running = 1;
static Cur* cursor[CurLast];
static Clr** scheme;
static Display* dpy;
static Drw* drw;
static Monitor* mons, * selmon;
static Window root, wmcheckwin;

static int useargb = 0;
static int bh;
static int restacking = 0;
static Visual* visual;
static int depth;
static Colormap cmap;

/* configuration, allows nested code to access above variables */
#include "config.h"

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* dwm will keep pid's of processes from autostart array and kill them at quit */
static pid_t* autostart_pids;
static size_t autostart_len;

/* execute command from autostart array */
static void autostart_exec() {
	const char* const* p;
	size_t i = 0;

	/* count entries */
	for (p = autostart; *p; autostart_len++, p++)
		while (*++p);

	autostart_pids = malloc(autostart_len * sizeof(pid_t));
	for (p = autostart; *p; i++, p++) {
		if ((autostart_pids[i] = fork()) == 0) {
			setsid();
			execvp(*p, (char* const*)p);
			_exit(EXIT_FAILURE);
		}
		printf("dwm: execvp");
		for (/* empty */; *p != NULL; ++p) printf(" \"%s\"", *p);
		putchar('\n');
		++p;
	}
}

/* function implementations */

void applyrules(Client* c) {
	const char* class, * instance;
	unsigned int i;
	const Rule* r;
	Monitor* m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tags = 0;
	XGetClassHint(dpy, c->win, &ch);
	class = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name ? ch.res_name : broken;

	if (strstr(class, "Steam") || strstr(class, "steam_app_"))
		c->issteam = 1;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
			&& (!r->class || strstr(class, r->class))
			&& (!r->instance || strstr(instance, r->instance)))
		{
			c->isfloating = r->isfloating;
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

int applysizehints(Client* c, int* x, int* y, int* w, int* h, int interact) {
	int baseismin;
	Monitor* m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
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
	if (*h < 10)
		*h = 10;
	if (*w < 10)
		*w = 10;
	if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
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
		XSync(dpy, False);
		while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	}
}

void arrangemon(Monitor* m) {
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}

void attach(Client* c) {
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void attachstack(Client* c) {
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void buttonpress(XEvent* e) {
	unsigned int i, click;
	Arg arg = { 0 };
	Client* c;
	Monitor* m;
	XButtonPressedEvent* ev = &e->xbutton;

	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon
		&& (focusonwheel || (ev->button != Button4 && ev->button != Button5))) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	if (ev->window == selmon->barwin) {
		buttonbar(ev, &arg, &click);
	} else if ((c = wintoclient(ev->window))) {
		if (focusonwheel || (ev->button != Button4 && ev->button != Button5))
			focus(c);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
			&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(arg.i == 0 ? &buttons[i].arg : &arg);
}

void checkotherwm(void) {
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void cleanup(void) {
	Arg a = { .ui = ~0 };
	Layout foo = { "", NULL };
	Monitor* m;
	size_t i;

	view(&a);
	selmon->lt[selmon->sellt] = &foo;
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	while (mons)
		cleanupmon(mons);
	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < LENGTH(colors); i++)
		free(scheme[i]);
	free(scheme);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	XSync(dpy, False);
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
	XUnmapWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->barwin);
	free(mon);
}

void clientmessage(XEvent* e) {
	XClientMessageEvent* cme = &e->xclient;
	Client* c = wintoclient(cme->window);

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
			|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c != selmon->sel && !c->isurgent)
			seturgent(c, 1);
	}
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
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent*)&ce);
}

void configurenotify(XEvent* e) {
	Monitor* m;
	Client* c;
	XConfigureEvent* ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize(drw, sw, sh);
			updatebars();
			for (m = mons; m; m = m->next) {
				for (c = m->clients; c; c = c->next)
					if (c->isfullscreen)
						resizeclient(c, m->mx, m->my, m->mw, m->mh);
				XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, m->bh);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void configurerequest(XEvent* e) {
	Client* c;
	Monitor* m;
	XConfigureRequestEvent* ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
			m = c->mon;
			if (!c->issteam) {
				if (ev->value_mask & CWX) {
					c->oldx = c->x;
					c->x = m->mx + ev->x;
				}
				if (ev->value_mask & CWY) {
					c->oldy = c->y;
					c->y = m->my + ev->y;
				}
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
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
	XSync(dpy, False);
}

Monitor* createmon(void) {
	Monitor* m;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
	m->topbar = topbar;
	m->gappx = gappx;
	m->bh = bh;
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	return m;
}

void destroynotify(XEvent* e) {
	Client* c;
	XDestroyWindowEvent* ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
}

void detach(Client* c) {
	Client** tc;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void detachstack(Client* c) {
	Client** tc, * t;

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
	int x = 0, w, tw = 0;
	unsigned int i, n = 0;
	Client* c;
	Monitor* m = selmon;

	w = m->bw;
	if (m == selmon) { /* status is only drawn on selected monitor */
		tw = TEXTWM(stext) + textpad * 2;
		if (ev->x > m->bx + m->bw - tw) {
			*click = ClkStatusText;
			return;
		}
		w -= tw;
	}

	for (c = m->clients; c; c = c->next) {
		if (ISVISIBLE(c))
			n++;
	}
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

	w -= x;
	
	if (n > 0) {
		for (c = m->clients; c; c = c->next) {
			if (!ISVISIBLE(c))
				continue;
			x += w / n;
			if (ev->x < x) {
				*click = ClkWinTitle;
				arg->v = c;
				return;
			}
		}
	}
	*click = ClkStatusText;

}

void drawbar(Monitor* m) {

	int indn;
	int x = 0, w, tw = 0, ew = 0;
	unsigned int i, n;
	Client* c;
	if (m->restacking)
		return;

	if (!m->showbar)
		return;

	w = m->bw;

	if (m == selmon) { /* status is only drawn on selected monitor */
		drw_setscheme(drw, scheme[SchemeNorm]);
		tw = TEXTWM(stext) + textpad * 2;
		drw_text(drw, m->bw - tw, 0, tw, m->bh, textpad, stext, 0, True);
		w -= tw;
	}

	for (i = 0; i < LENGTH(tags); i++) {
		drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
		tw = TEXTW(tags[i]) + textpad * 2;
		if (tw < m->bh) {
			drw_text(drw, x, 0, m->bh, m->bh, (m->bh - tw) / 2 + textpad, tags[i], 0, False);
			tw = m->bh;
		} else {
			drw_text(drw, x, 0, tw, m->bh, textpad, tags[i], 0, False);
		}
		for (indn = 0, c = m->clients; c; c = c->next) {
			if (c->tags & (1 << i)) {
				drw_rect(drw, x + 1 + (indn * 4), m->bh - 4, 3, 3, selmon->sel == c, 0);
				indn++;
			}
		}
		x += tw;
	}

	if (m->lt[m->sellt]->symbol[0]) {
		drw_setscheme(drw, scheme[SchemeNorm]);
		tw = TEXTW(m->lt[m->sellt]->symbol) + textpad * 2;
		if (tw < m->bh) {
			x = drw_text(drw, x, 0, m->bh, m->bh, (m->bh - tw) / 2 + textpad, m->lt[m->sellt]->symbol, 0, False);
		} else {
			x = drw_text(drw, x, 0, tw, m->bh, textpad, m->lt[m->sellt]->symbol, 0, False);
		}
	}

	w -= x;
	for (n = 0, c = m->clients; c; c = c->next) {
		if (ISVISIBLE(c)) ++n;
	}
	if (n > 0) {
		for (c = m->clients; c; c = c->next) {
			if (!ISVISIBLE(c))
				continue;
			ew = w / n;
			drw_setscheme(drw, scheme[m == selmon && m->sel == c ? SchemeSel : SchemeNorm]);
			
			if (c->icon && (tw = c->icw + textpad * 2) <= ew) {
				drw_rect(drw, x, 0, tw, m->bh, 1, 1);
				drw_pic(drw, x + textpad, (m->bh - c->ich) / 2, c->icw, c->ich, c->icon);
				if (c->isalwaysontop) drw_rect(drw, x + 1, 1, 4, 4, 0, 0);
				x += tw;
				ew -= tw;
			} else {
				if (c->isalwaysontop) drw_rect(drw, x + 1, 1, 4, 4, 0, 0);
			}
			tw = TEXTW(c->name) + 2 * textpad;
			if (tw > ew)
				drw_text(drw, x, 0, ew, m->bh, textpad, c->name, 0, 0);
			else
				drw_text(drw, x, 0, ew, m->bh, (ew - tw) / 2 + textpad, c->name, 0, 0);
			x += ew;
		}
		w -= (w / n) * n;
	}
	drw_setscheme(drw, scheme[SchemeNorm]);
	drw_rect(drw, x, 0, w, m->bh, 1, 1);
	drw_map(drw, m->barwin, 0, 0, m->bw, m->bh);

}

void drawbars(void) {
	Monitor* m;
	if (restacking) return;
	restacking = 1;
	for (m = mons; m; m = m->next)
		drawbar(m);
	restacking = 0;
}

void enternotify(XEvent *e) {
	if (focusonhover == 0) return;
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;
	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
	} else if (!c || c == selmon->sel)
		return;
	focus(c);
}

void expose(XEvent* e) {
	Monitor* m;
	XExposeEvent* ev = &e->xexpose;
	if (ev->count == 0 && (m = wintomon(ev->window)))
		drawbar(m);
}

void motionnotify(XEvent *e) {
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	mon = m;
}

void focus(Client* c) {
	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
	if (selmon->sel && selmon->sel != c)
		unfocus(selmon->sel, 0);
	if (c) {
		if (c->mon != selmon)
			selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
		setfocus(c);
	} else {
		XSetInputFocus(dpy, selmon->barwin, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	drawbars();
}

void focusclient(const Arg* arg) {
	if (arg) focus((Client*)arg->v);
}

void focusin(XEvent* e) {
	/* there are some broken focus acquiring clients needing extra handling */
	XFocusChangeEvent* ev = &e->xfocus;
	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void focusmon(const Arg* arg) {
	Monitor* m;
	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
}

void focusstack(const Arg* arg) {
	Client* c = NULL, * i;
	if (!selmon->sel || (selmon->sel->isfullscreen && lockfullscreen))
		return;
	if (arg->i > 0) {
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

Atom getatomprop(Client* c, Atom prop) {
	int di;
	unsigned long dl;
	unsigned char* p = NULL;
	Atom da, atom = None;

	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
		&da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom*)p;
		XFree(p);
	}
	return atom;
}

uint32_t prealpha(uint32_t p) {
	uint8_t a = p >> 24u;
	uint32_t rb = (a * (p & 0xFF00FFu)) >> 8u;
	uint32_t g = (a * (p & 0x00FF00u)) >> 8u;
	return (rb & 0xFF00FFu) | (g & 0x00FF00u) | (a << 24u);
}

Picture geticonprop(Window win, unsigned int* picw, unsigned int* pich) {
	int format;
	unsigned long n, extra, * p = NULL;
	Atom real;

	if (XGetWindowProperty(dpy, win, netatom[NetWMIcon], 0L, LONG_MAX, False, AnyPropertyType,
		&real, &format, &n, &extra, (unsigned char**)&p) != Success)
		return None;
	if (n == 0 || format != 32) { XFree(p); return None; }

	unsigned long* bstp = NULL;
	uint32_t w, h, sz;
	{
		unsigned long* i; const unsigned long* end = p + n;
		uint32_t bstd = UINT32_MAX, d, m;
		for (i = p; i < end - 1; i += sz) {
			if ((w = *i++) >= 16384 || (h = *i++) >= 16384) { XFree(p); return None; }
			if ((sz = w * h) > end - i) break;
			if ((m = w > h ? w : h) >= ICONSIZE && (d = m - ICONSIZE) < bstd) { bstd = d; bstp = i; }
		}
		if (!bstp) {
			for (i = p; i < end - 1; i += sz) {
				if ((w = *i++) >= 16384 || (h = *i++) >= 16384) { XFree(p); return None; }
				if ((sz = w * h) > end - i) break;
				if ((d = ICONSIZE - (w > h ? w : h)) < bstd) { bstd = d; bstp = i; }
			}
		}
		if (!bstp) { XFree(p); return None; }
	}

	if ((w = *(bstp - 2)) == 0 || (h = *(bstp - 1)) == 0) { XFree(p); return None; }

	uint32_t icw, ich;
	if (w <= h) {
		ich = ICONSIZE; icw = w * ICONSIZE / h;
		if (icw == 0) icw = 1;
	} else {
		icw = ICONSIZE; ich = h * ICONSIZE / w;
		if (ich == 0) ich = 1;
	}
	*picw = icw; *pich = ich;

	uint32_t i, * bstp32 = (uint32_t*)bstp;
	for (sz = w * h, i = 0; i < sz; ++i) bstp32[i] = prealpha(bstp[i]);

	Picture ret = drw_picture_create_resized(drw, (char*)bstp, w, h, icw, ich);
	XFree(p);

	return ret;
}

int getrootptr(int* x, int* y) {
	int di;
	unsigned int dui;
	Window dummy;
	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long getstate(Window w) {
	int format;
	long result = -1;
	unsigned char* p = NULL;
	unsigned long n, extra;
	Atom real;
	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
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

void grabbuttons(Client* c, int focused) {
	updatenumlockmask();
	unsigned int i, j;
	unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask | LockMask };
	XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
	if (!focused)
		XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
			BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
	for (i = 0; i < LENGTH(buttons); i++)
		if (buttons[i].click == ClkClientWin)
			for (j = 0; j < LENGTH(modifiers); j++)
				XGrabButton(dpy, buttons[i].button,
					buttons[i].mask | modifiers[j],
					c->win, False, BUTTONMASK,
					GrabModeAsync, GrabModeSync, None, None);
}

void grabkeys(void) {
	updatenumlockmask();
	unsigned int i, j, k;
	unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask | LockMask };
	int start, end, skip;
	KeySym* syms;
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

void inclayout(const Arg* arg) {
	int n = arg->i;
	for (int i = 0; i < abs(n); ++i) {
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

void incnmaster(const Arg* arg) {
	selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

#ifdef XINERAMA
static int isuniquegeom(XineramaScreenInfo* unique, size_t n, XineramaScreenInfo* info) {
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
			&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void keypress(XEvent* e) {
	unsigned int i;
	KeySym keysym;
	XKeyEvent* ev;

	ev = &e->xkey;
	// keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	keysym = XkbKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0, (KeyCode)ev->state & ShiftMask ? 1 : 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
			&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
			&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void killclient(const Arg* arg) {
	Client *c;
	if (arg)
		c = (Client*)arg->v;
	else if (selmon->sel)
		c = selmon->sel;
	else
		return;
	if (!sendevent(c, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, c->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void manage(Window w, XWindowAttributes* wa) {
	Client* c, * t = NULL;
	Window trans = None;
	XWindowChanges wc;
	c = ecalloc(1, sizeof(Client));
	c->win = w;
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;

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
	c->bw = borderpx;

	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	XSelectInput(dpy, w, EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
	grabbuttons(c, 0);
	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating)
		XRaiseWindow(dpy, c->win);
	attach(c);
	attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char*)&(c->win), 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
	setclientstate(c, NormalState);
	if (c->mon == selmon)
		unfocus(selmon->sel, 0);
	c->mon->sel = c;
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	focus(NULL);
}

void mappingnotify(XEvent* e) {
	XMappingEvent* ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void maprequest(XEvent* e) {
	static XWindowAttributes wa;
	XMapRequestEvent* ev = &e->xmaprequest;
	if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect)
		return;
	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
}

void movemouse(const Arg* arg) {
	int x, y, ocx, ocy, nx, ny;
	Client* c;
	Monitor* m;
	XEvent ev;
	Time lasttime = 0;
	int moved = 0;

	if (!(c = selmon->sel))
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
		switch (ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
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
			if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
				&& (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
				togglefloating(NULL);
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating) {
				resize(c, nx, ny, c->w, c->h, 1);
				moved = 1;
			}
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if (moved) {
		if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
			sendmon(c, m);
			selmon = m;
			focus(NULL);
		}
	} else {
		setmaster(c);
	}
}

void moveresizetile(const Arg* arg) {
	static Client* c;
	static Monitor* m;
	if (!(c = selmon->sel))
		return;
	m = c->mon;
	static int mw, mh;
	mw = m->ww;
	mh = m->wh;
	static int x, y, w, h;
	switch (arg->ui) {
	case TileNW:
		x = 0;
		y = 0;
		w = mw / 2 - m->gappx / 2;
		h = mh / 2 - m->gappx / 2;
		break;
	case TileW:
		x = 0;
		y = 0;
		w = mw / 2 - m->gappx / 2;
		h = mh;
		break;
	case TileSW:
		x = 0;
		y = mh / 2 + m->gappx / 2;
		w = mw / 2 - m->gappx / 2;
		h = mh / 2 - m->gappx / 2;
		break;
	case TileN:
		x = 0;
		y = 0;
		w = mw;
		h = mh / 2 - m->gappx / 2;
		break;
	case TileFill:
		x = 0;
		y = 0;
		w = mw;
		h = mh;
		break;
	case TileS:
		x = 0;
		y = mh / 2 + m->gappx / 2;
		w = mw;
		h = mh / 2 - m->gappx / 2;
		break;
	case TileNE:
		x = mw / 2 + m->gappx / 2;
		y = 0;
		w = mw / 2 - m->gappx / 2;
		h = mh / 2 - m->gappx / 2;
		break;
	case TileE:
		x = mw / 2 + m->gappx / 2;
		y = 0;
		w = mw / 2 - m->gappx / 2;
		h = mh;
		break;
	case TileSE:
		x = mw / 2 + m->gappx / 2;
		y = mh / 2 + m->gappx / 2;
		w = mw / 2 - m->gappx / 2;
		h = mh / 2 - m->gappx / 2;
		break;
	case TileCenter:
		x = mw / 2 - (mw / 2 - m->gappx / 2) / 2;
		y = mh / 2 - (mh / 2 - m->gappx / 2) / 2;
		w = mw / 2 - m->gappx / 2;
		h = mh / 2 - m->gappx / 2;
		break;
	case TileFullscreen:
		x = -m->wx;
		y = -m->wy;
		w = m->mw;
		h = m->mh;
		break;
	case TileDoubleFullscreen:
		x = -m->wx - m->mw;
		y = -m->wy - m->mh;
		w = m->mw * 2;
		h = m->mh * 2;
		break;
	}
	x += m->wx;
	y += m->wy;
	resize(c, x, y, w, h, 1);
}

Client* nexttiled(Client* c) {
	for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
	return c;
}

void pop(Client* c) {
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

void propertynotify(XEvent* e) {
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
			if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
				(c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			c->hintsvalid = 0;
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

void quit(const Arg* arg) {
	size_t i;

	/* kill child processes */
	for (i = 0; i < autostart_len; i++) {
		if (0 < autostart_pids[i]) {
			kill(autostart_pids[i], SIGTERM);
			waitpid(autostart_pids[i], NULL, 0);
		}
	}

	running = 0;
}

Monitor* recttomon(int x, int y, int w, int h) {
	Monitor* m, * r = selmon;
	int a, area = 0;
	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void resize(Client* c, int x, int y, int w, int h, int interact) {
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
	XSync(dpy, False);
}

void resizemouse(const Arg* arg) {
	int ocx, ocy, nw, nh, aw, ah;
	Client* c;
	Monitor* m;
	XEvent ev;
	Time lasttime = 0;
	if (!(c = selmon->sel))
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	do {
		XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
		switch (ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nw = ev.xmotion.x - ocx;
			nh = ev.xmotion.y - ocy;
			aw = MAX(abs(nw), 10);
			ah = MAX(abs(nh), 10);
			resize(
				c,
				nw < 0 ? ocx - aw : ocx, nh < 0 ? ocy - ah : ocy,
				aw, ah,
				1
			);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void restack(Monitor* m) {
	Client* c;
	XEvent ev;
	XWindowChanges wc;

	if (m->restacking)
		return;
	if (!m->sel)
		return;

	m->restacking = 1;

	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow(dpy, m->sel->win);

	/* raise the aot window */
	for (Monitor* m_search = mons; m_search; m_search = m_search->next) {
		for (c = m_search->clients; c; c = c->next) {
			if (c->isalwaysontop) {
				XRaiseWindow(dpy, c->win);
			}
		}
	}

	if (m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling | CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}

	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev)) {}

	m->restacking = 0;
	drawbar(m);
}

void run(void) {
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev))
		if (handler[ev.type])
			handler[ev.type](&ev); /* call handler */
}

void scan(void) {
	unsigned int i, num;
	Window d1, d2, * wins = NULL;
	XWindowAttributes wa;
	Monitor* m;
	if (!XQueryTree(dpy, root, &d1, &d2, &wins, &num)) return;
	for (m = mons; m; m = m->next) m->restacking = 1;
	restacking = 1;
	for (i = 0; i < num; i++) {
		if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
			continue;
		if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
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
	for (m = mons; m; m = m->next) m->restacking = 0;
	restacking = 0;
	drawbars();
}

void sendmon(Client* c, Monitor* m) {
	if (c->mon == m)
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
	attach(c);
	attachstack(c);
	focus(NULL);
	arrange(NULL);
}

void setclientstate(Client* c, long state) {
	long data[] = { state, None };
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
		XSendEvent(dpy, c->win, False, NoEventMask, &ev);
	}
	return exists;
}

void setfocus(Client* c) {
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow],
			XA_WINDOW, 32, PropModeReplace,
			(unsigned char*)&(c->win), 1);
	}
	sendevent(c, wmatom[WMTakeFocus]);
}

void setfullscreen(Client* c, int fullscreen) {
	if (fullscreen && !c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
		c->oldstate = c->isfloating;
		c->oldbw = c->bw;
		c->bw = 0;
		c->isfloating = 1;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	} else if (!fullscreen && c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = 0;
		c->isfloating = c->oldstate;
		c->bw = c->oldbw;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(c->mon);
	}
}

void setgaps(const Arg* arg) {
	if ((arg->i == 0) || (selmon->gappx + arg->i < 0))
		selmon->gappx = 0;
	else
		selmon->gappx += arg->i;
	arrange(selmon);
	drawbars();
}

void setlayout(const Arg* arg) {
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] = (Layout*)arg->v;
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

void setmaster(Client* master) {
	if (!master) return;
	master->isfloating = 0;
	detach(master);
	attach(master);
	focus(master);
	arrange(master->mon);
}

/* arg > 1.0 will set mfact absolutely */
void setmfact(const Arg* arg) {
	float f;
	if (!arg || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
	selmon->mfact = f;
	arrange(selmon);
}

void setup(void) {
	int i;
	pid_t pid;
	XSetWindowAttributes wa;
	Atom utf8string;
	struct sigaction sa;

	/* do not transform children into zombies when they terminate */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, NULL);

	/* clean up any zombies (inherited from .xinitrc etc) immediately */
	while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
		pid_t* p, * lim;

		if (!(p = autostart_pids))
			continue;
		lim = &p[autostart_len];

		for (; p < lim; p++) {
			if (*p == pid) {
				*p = -1;
				break;
			}
		}

	}

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	xinitvisual();
	drw = drw_create(dpy, screen, root, sw, sh, visual, depth, cmap);
	if (!drw_font_create(drw, font))
		die("no fonts could be loaded.");
	textpad = drw->font->h / 2;
	bh = drw->font->h * 1.5;
	updategeom();
	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMIcon] = XInternAtom(dpy, "_NET_WM_ICON", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	/* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);
	/* init appearance */
	scheme = ecalloc(LENGTH(colors), sizeof(Clr*));
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], alphas[i], 3);
	/* init bars */
	updatebars();
	updatestatus();
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char*)&wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char*)"dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char*)&wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char*)netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask
		| ButtonPressMask | PointerMotionMask | EnterWindowMask
		| LeaveWindowMask | StructureNotifyMask | PropertyChangeMask;
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

void showhide(Client* c) {
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void spawn(const Arg* arg) {
	struct sigaction sa;

	if (arg->v == dmenucmd)
		dmenumon[0] = '0' + selmon->num;
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_DFL;
		sigaction(SIGCHLD, &sa, NULL);

		printf("dwm: execvp");
		for (char** p = (char**)arg->v; *p != NULL; ++p) printf(" \"%s\"", *p);
		putchar('\n');

		execvp(((char**)arg->v)[0], (char**)arg->v);
		die("dwm: execvp \"%s\" failed:", ((char**)arg->v)[0]);
	}
}

void tag(const Arg* arg) {
	if (selmon->sel && arg->ui & TAGMASK) {
		selmon->sel->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(selmon);
	}
}

void tagmon(const Arg* arg) {
	if (!selmon->sel || !mons->next)
		return;
	sendmon(selmon->sel, dirtomon(arg->i));
}

void tile(Monitor* m) {
	unsigned int i, n, nm, my, wy, mh, wh;
	Client* c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if (n == 0)
		return;

	if (n == 1) {
		c = nexttiled(m->clients);
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
		return;
	}

	nm = m->nmaster > n ? n : m->nmaster;
	my = m->wy;
	wy = m->wy;
	wh = (m->wh + m->gappx);
	if (n != nm) wh /= n - nm;
	mh = (m->wh + m->gappx);
	if (nm != 0) mh /= nm;
	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), ++i) {
		if (i < nm) {
			resize(c, m->wx, my, m->ww * m->mfact - m->gappx - 2 * c->bw, mh - m->gappx - 2 * c->bw, 0);
			my += mh;
		} else {
			resize(c, m->wx + m->ww * m->mfact, wy, m->ww - m->ww * m->mfact - c->bw, wh - m->gappx - 2 * c->bw, 0);
			wy += wh;
		}
	}
}
void col(Monitor* m) {
	unsigned int i, n, x, y, w, h, ww, mw, nm;
	Client* c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if (n == 0)
		return;

	if (n == 1) {
		c = nexttiled(m->clients);
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
		return;
	}

	nm = m->nmaster > n ? n : m->nmaster;
	x = m->wx;
	y = m->wy;
	w = m->ww + m->gappx;
	h = m->wh;
	mw = m->ww * m->mfact;
	ww = w - mw;
	mw /= nm;
	if (nm != n) ww /= n - nm;
	
	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), ++i) {
		if (i < nm) {
			resize(c, x, y, mw - m->gappx - 2 * c->bw, h - 2 * c->bw, 0);
			x += mw;
		} else {
			resize(c, x, y, ww - m->gappx - 2 * c->bw, h - 2 * c->bw, 0);
			x += ww;
		}
	}
}

void monocle(Monitor* m) {
	Client* c;
	for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void togglebar(const Arg* arg) {
	selmon->showbar = !selmon->showbar;
	updatebarpos(selmon);
	if (selmon->showbar) {
		XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, selmon->bh);
		XMapWindow(dpy, selmon->barwin);
	} else {
		XUnmapWindow(dpy, selmon->barwin);
	}
	arrange(selmon);
}

void togglefloating(const Arg* arg) {
	Client *c;
	if (arg)
		c = (Client*)arg->v;
	else if (selmon->sel)
		c = selmon->sel;
	else
		return;
	c->isfloating = !c->isfloating || c->isfixed;
	if (c->isfloating)
		resize(c, c->x, c->y, c->w, c->h, 0);
	else
		c->isalwaysontop = 0; /* disabled, turn this off too */
	arrange(c->mon);
}

void togglealwaysontop(const Arg* arg) {
	Client *c, *d;
	if (arg)
		c = (Client*)arg->v;
	else if (selmon->sel)
		c = selmon->sel;
	else
		return;
	if (c->isalwaysontop) {
		c->isalwaysontop = 0;
	} else {
		if (arg->ui == 1) {
			for (d = c; d; d = d->next)
				d->isalwaysontop = 0;
		}
		// c->isfloating = 1; // make it float too
		c->isalwaysontop = 1;
	}
	arrange(c->mon);
}

void toggletag(const Arg* arg) {
	unsigned int newtags;
	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		selmon->sel->tags = newtags;
		focus(NULL);
		arrange(selmon);
	}
}

void toggleview(const Arg* arg) {
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;
		focus(NULL);
		arrange(selmon);
	}
}

void freeicon(Client* c) {
	if (c->icon) {
		XRenderFreePicture(dpy, c->icon);
		c->icon = None;
	}
}

void unfocus(Client* c, int setfocus) {
	if (!c)
		return;
	grabbuttons(c, 0);
	XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void unmanage(Client* c, int destroyed) {
	Monitor* m = c->mon;
	XWindowChanges wc;
	detach(c);
	detachstack(c);
	freeicon(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XSelectInput(dpy, c->win, NoEventMask);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	free(c);
	focus(NULL);
	updateclientlist();
	arrange(m);
}

void unmapnotify(XEvent* e) {
	Client* c;
	XUnmapEvent* ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	}
}

void updatebars(void) {
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
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
		XMapRaised(dpy, m->barwin);
		XSetClassHint(dpy, m->barwin, &ch);
	}
}

void updatebarpos(Monitor* m) {
	m->wx = m->mx + m->gappx;
	if (!m->showbar) {
		m->wy = m->my + m->gappx;
		m->ww = m->mw - 2 * m->gappx;
		m->wh = m->mh - 2 * m->gappx;
		m->bx = m->bw = 0;
		m->by = -m->bh;
		return;
	}
	m->bw = m->mw - 2 * m->gappx;
	m->bx = m->mx + m->gappx;
	m->ww = m->mw - 2 * m->gappx;
	m->wh = m->mh - m->bh - 3 * m->gappx;
	if (m->topbar) {
		m->wy = m->my + m->bh + 2 * m->gappx;
		m->by = m->my + m->gappx;
	} else {
		m->wy = m->my + m->gappx;
		m->by = m->my + m->mh - m->bh - m->gappx;
	}
}

void updateclientlist() {
	Client* c;
	Monitor* m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList],
				XA_WINDOW, 32, PropModeAppend,
				(unsigned char*)&(c->win), 1);
}

int updategeom(void) {
	int dirty = 0;

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
				dirty = 1;
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
			dirty = 1;
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
}

void updatesizehints(Client* c) {
	long msize;
	XSizeHints size;
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
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
	c->hintsvalid = 1;
}

void updatestatus(void) {
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		strcpy(stext, "dwm-"VERSION);
	drawbar(selmon);
}

void updatetitle(Client* c) {
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);
}

void updateicon(Client* c) {
	freeicon(c);
	c->icon = geticonprop(c->win, &c->icw, &c->ich);
}

void updatewindowtype(Client* c) {
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);
	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
}

void updatewmhints(Client* c) {
	XWMHints* wmh;
	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}

void view(const Arg* arg) {
	if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK)
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
	focus(NULL);
	arrange(selmon);
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
	Monitor* m;
	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = mons; m; m = m->next)
		if (w == m->barwin)
			return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

/* Selects for the view of the focused window. The list of tags */
/* to be displayed is matched to the focused window tag list. */
void winview(const Arg* arg) {
	Window win, win_r, win_p, * win_c;
	unsigned nc;
	int unused;
	Client* c;
	Arg a;

	if (!XGetInputFocus(dpy, &win, &unused)) return;
	while (XQueryTree(dpy, win, &win_r, &win_p, &win_c, &nc)
		&& win_p != win_r) win = win_p;

	if (!(c = wintoclient(win))) return;

	a.ui = c->tags;
	view(&a);
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int xerror(Display* dpy, XErrorEvent* ee) {
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

int xerrordummy(Display* dpy, XErrorEvent* ee) {
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display* dpy, XErrorEvent* ee) {
	die("dwm: another window manager is already running");
	return -1;
}

void xinitvisual() {
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
}

void zoom(const Arg* arg) {
	Client* c = selmon->sel;
	if (!selmon->lt[selmon->sellt]->arrange || !c || c->isfloating)
		return;
	if (c == nexttiled(selmon->clients) && !(c = nexttiled(c->next)))
		return;
	pop(c);
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
	autostart_exec();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan();
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
