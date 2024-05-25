/* See LICENSE file for copyright and license details. */


static const unsigned int focusonwheel = 0;

/* appearance */
static const unsigned int borderpx  = 0;        /* border pixel of windows */
static const unsigned int gappx     = 10;       /* gaps between windows */
static const unsigned int snap      = 0;        /* snap pixel */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
#define ICONSIZE (bh - 2) /* icon size */
#define FONT_FAMILY "Iosevka Nerd"
#define FONT_SIZE "17"
static const char font[]            = FONT_FAMILY " " FONT_SIZE;
static const char dmenufont[]       = FONT_FAMILY ":size=" FONT_SIZE;
static const char col_bg []         = "#000000";
static const char col_fg []         = "#ffddff";
static const char col_txt[]         = "#ffffff";
static const unsigned int alpha = 0xff * 0.7;
#define DMENUALPHA "178" // 0xff * 0.7
static const char *const colors[][3]      = {
	/*               fg       bg      border   */
	[SchemeNorm] = { col_txt, col_bg, 0 },
	[SchemeSel]  = { col_bg , col_fg, 0 },
};
static const unsigned int alphas[][3]      = {
    /*               fg      bg        border*/
    [SchemeNorm] = { OPAQUE, alpha, OPAQUE },
	[SchemeSel]  = { OPAQUE, alpha, OPAQUE },
};
/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ NULL,       NULL,       NULL,       0,            False,       -1 },
	// { "Gimp",     NULL,       NULL,       0,            1,           -1 },
	// { "Firefox",  NULL,       NULL,       1 << 8,       0,           -1 },
};

/* alt tab */
static int alt_tab_count = 0;
static void focus_restack(Client *c) { if (c) { focus(c); restack(selmon); } }
static void start_alt_tab(const Arg *arg) { alt_tab_count = 0; }
static Client *next_visible(Client *c) {
	for(/* DO_NOTHING */; c && !ISVISIBLE(c); c = c->snext);
	return c;
}
static int count_visible(void) {
	int count = 0;
	for (Client *c = next_visible(selmon->stack); c; c = next_visible(c->snext)) ++count;
	return count;
}
static Client *get_nth_client(int n) {
	Client *c;
	for (c = next_visible(selmon->stack); c && n--; c = next_visible(c->snext));
	return c;
}
static void alt_tab(const Arg *arg) {
	if (!selmon) return;
	if (selmon->restacking) return;
	selmon->restacking = 1;
	for (int i = 0; i < alt_tab_count; ++i) focus_restack(get_nth_client(alt_tab_count));
	int visible = count_visible();
	if (visible == 0) return;
	alt_tab_count = (alt_tab_count + 1) % visible;
	focus_restack(get_nth_client(alt_tab_count));
	selmon->restacking = 0;
	drawbar(selmon);
}

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 0; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "󰙀",      tile },    /* first entry is default */
	{ "󰕭",      col },     /* even sized columns */
	{ "",      NULL },    /* no layout function means floating behavior */
	// { "󰊓",      monocle }, /* only 1 window fullscreen */
};

/* commands */
#define TERM "kitty"
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = {
	"dmenu_run", "-m", dmenumon,
	"-i", "-fn", dmenufont,
	"-a", DMENUALPHA,
	"-nb", colors[SchemeNorm][1], "-nf", colors[SchemeNorm][0],
	"-sb", colors[SchemeSel][1], "-sf", colors[SchemeSel][0],
	"-nhb", colors[SchemeNorm][1], /* "-nhf", colors[SchemeSel][0], */
	"-shb", colors[SchemeSel][1], /* "-shf", colors[SchemeNorm][0], */
	NULL
};
static const char *termcmd[]      = { TERM,                                          NULL };
static const char *fmcmd[]        = { "pcmanfm",                                     NULL };

static const char *screencmd[]    = { "screenshot",                                  NULL };
static const char *screenallcmd[] = { "screenshot", "1",                             NULL };
static const char *xkillcmd[]     = { "xkill",                                       NULL };

/* Auto start */
#define AUTOSTART "~/.dwm/autostart.sh"
static const char *const autostart[] = {
	"/bin/sh", "-c", AUTOSTART, NULL,
	TERM, NULL,
	NULL /* terminate */
};

/* Key Binds */

#define MODKEY Mod4Mask
static const Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,		        XK_t,	   spawn,         	  { .v = termcmd      } },
	{ MODKEY,		        XK_r,	   spawn,         	  { .v = dmenucmd     } },
	// { MODKEY,				XK_e,      spawn,			  { .v = fmcmd        } },
	// { MODKEY,		        XK_v,	   spawn,         	  { .v = pavucmd      } },

	// { MODKEY,				XK_f,      spawn,			  { .v = brcmd        } },
	// { MODKEY,				XK_m,      spawn,			  { .v = mccmd        } },
	// { MODKEY,				XK_s,      spawn,			  { .v = stcmd        } },
	// { MODKEY,				XK_d,      spawn,			  { .v = twcmd        } },

	{ MODKEY,				XK_Print,  spawn,			  { .v = screenallcmd } },
	{ 0,					XK_Print,  spawn,			  { .v = screencmd    } },

	{ MODKEY|ControlMask,   XK_Delete, quit,              { 0 			      } },
	{ MODKEY,             	XK_F4,     killclient,        { 0                 } },
	{ MODKEY|ShiftMask,	    XK_F4,     spawn,			  { .v = xkillcmd     } },

	{ 0,                    XK_Super_L, start_alt_tab,    { 0                 } },
    { MODKEY,               XK_Tab,    alt_tab,           { 0                 } },
    
    { MODKEY,               XK_space,  togglealwaysontop, { .ui = 0           } },
    { MODKEY|ShiftMask,     XK_space,  togglealwaysontop, { .ui = 1           } },
	
	{ MODKEY,               XK_1,      view,              { .ui = 1           } },
	{ MODKEY,               XK_2,      view,              { .ui = 2           } },
	{ MODKEY,               XK_3,      view,              { .ui = 4           } },
	{ MODKEY,               XK_4,      view,              { .ui = 8           } },
	{ MODKEY,               XK_5,      view,              { .ui = 16          } },
	{ MODKEY,               XK_6,      view,              { .ui = 32          } },
	{ MODKEY,               XK_7,      view,              { .ui = 64          } },
	{ MODKEY,               XK_8,      view,              { .ui = 128         } },
	{ MODKEY,               XK_9,      view,              { .ui = 256         } },

	{ MODKEY|ShiftMask,     XK_1,      tag,               { .i  = 1           } },
	{ MODKEY|ShiftMask,     XK_2,      tag,               { .i  = 2           } },
	{ MODKEY|ShiftMask,     XK_3,      tag,               { .i  = 4           } },
	{ MODKEY|ShiftMask,     XK_4,      tag,               { .i  = 8           } },
	{ MODKEY|ShiftMask,     XK_5,      tag,               { .i  = 16          } },
	{ MODKEY|ShiftMask,     XK_6,      tag,               { .i  = 32          } },
	{ MODKEY|ShiftMask,     XK_7,      tag,               { .i  = 64          } },
	{ MODKEY|ShiftMask,     XK_8,      tag,               { .i  = 128         } },
	{ MODKEY|ShiftMask,     XK_9,      tag,               { .i  = 256         } },

	{ MODKEY,               XK_q,      moveresizetile,    { .ui = TileNW      } },
	{ MODKEY,               XK_a,      moveresizetile,    { .ui = TileW       } },
	{ MODKEY,               XK_z,      moveresizetile,    { .ui = TileSW      } },

	{ MODKEY,               XK_w,      moveresizetile,    { .ui = TileN       } },
	{ MODKEY,               XK_s,      moveresizetile,    { .ui = TileFill    } },
	{ MODKEY,               XK_x,      moveresizetile,    { .ui = TileS       } },

	{ MODKEY,               XK_e,      moveresizetile,    { .ui = TileNE      } },
	{ MODKEY,               XK_d,      moveresizetile,    { .ui = TileE       } },
	{ MODKEY,               XK_c,      moveresizetile,    { .ui = TileSE      } },

	{ MODKEY|ShiftMask,     XK_s,      moveresizetile,    { .ui = TileFullscreen } },
	{ MODKEY|ControlMask,   XK_s,      moveresizetile,    { .ui = TileCenter  } },
	{ MODKEY|ControlMask|ShiftMask, XK_s, moveresizetile, { .ui = TileDoubleFullscreen } },

};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        inclayout,      { .i =  1 } },
	{ ClkLtSymbol,          0,              Button3,        inclayout,      { .i = -1 } },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          { .v = termcmd } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};

