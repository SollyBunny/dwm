/* See LICENSE file for copyright and license details. */

/* focus rules */
static const bool focusonhover      = 0;    /* 0 means focus only on click, otherwise */
static const bool focusonwheel      = 0;    /* if focusonhover is 0, whether to count scrolling as click */
static const bool focusmononhover   = 1;    /* 0 means focus only on click, otherwise */
static const bool focusmononwheel   = 0;    /* if focusmononhover is 0, whether to count scrolling as click */
static const bool resizemousewarp   = 0;    /* if 1 warp pointer to corner when resizing */

/* layout */
static const unsigned int gapwindow = 10;   /* gaps between windows (in layouts) */
static const unsigned int gapbar    = 0;    /* gap between bar and window area edge */
static const unsigned int gapedge   = 10;   /* gap between windows and window area edge (in layouts) */
static const unsigned int snap      = 5;    /* pixels away from snap location to snap to */
static const float opacityfocus     = 1.0;  /* opacity of focused window (set both to -1 to disable) */
static const float opacityunfocus   = 0.9;  /* opacity of unfocused window */
static const bool opacityfullscreen = 1.0;  /* opacity of fullscreen windows, focused or not (set to -1 to disable) */

static const float mfact            = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster            = 1;    /* number of clients in master area */
static const int resizehints        = 1;    /* 1 means respect size hints in tiled resizals */

static const char* ignorehintsmatch[] = { "steam", "Steam" };
static const char* ignorehintscontains[] = { "steam_app_", "osu", "VSC", "Chrom", "iscord", "manfm" };

static const Layout layouts[] = {
	/* symbol, arrange function */
	{ "󰕰",     ltgrid    }, /* grid (default) */
	{ "󰙀",     ltrow     }, /* even sized rows */
	{ "󰕭",     ltcol     }, /* even sized columns */
	{ "",     ltmonocle }, /* only 1 window fullscreen */
	{ "󰅡",     NULL      }, /* floating behavior */
};

/* bar */
static const char broken[] = "broken";       /* text for windows with no title */
static const bool showbar  = true;           /* false means no bar */
static const bool topbar   = true;           /* false means bottom bar */
#define ICONSIZE (bh * 0.6)                  /* icon size */
static const char* tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

/* font */
#define FONT_FAMILY "Jetbrains Mono Nerd Font"
#define FONT_SIZE "17"
#define FONT FONT_FAMILY ":size=" FONT_SIZE
static const char* fonts[] = { FONT };
static const unsigned int borderwidth = 1;   /* border width of windows */
static const char col_bg [] = "#000000";
static const char col_fg [] = "#ffddff";
static const char col_txt[] = "#ffffff";
static const unsigned int alpha = 0xff * 0.7;
#define DMENUALPHA "178" // 0xff * 0.7
static const char *const colors[][3]      = {
	/*               fg       bg      border */
	[SchemeNorm] = { col_txt, col_bg, col_bg },
	[SchemeSel]  = { col_bg , col_fg, col_fg },
};
static const unsigned int alphas[][3]      = {
	/*               fg       bg      border */
	[SchemeNorm] = { OPAQUE,  alpha,  alpha },
	[SchemeSel]  = { OPAQUE,  alpha,  alpha },
};

/* rules */
static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     position               monitor */
	/* { "Gimp",     NULL,       NULL,       0,            PositionN,             -1 }, */
	/* { "Firefox",  NULL,       NULL,       1 << 8,       PositionFullscreen,    -1 }, */
	{ NULL,       NULL,       NULL,       0,            PositionTiled,          -1 }, /* default rule */
};

/* alt tab */
static int alt_tab_count = 0;
static void start_alt_tab(const Arg arg) { alt_tab_count = 0; }
static Client *next_visible(Client *c) {
	for(/* DO_NOTHING */; c && !ISVISIBLE(c); c = c->snext);
	return c;
}
static int count_visible(void) {
	Client* c;
	int count = 0;
	for (c = next_visible(selmon->stack); c; c = next_visible(c->snext)) ++count;
	return count;
}
static Client *get_nth_client(int n) {
	Client* c;
	for (c = next_visible(selmon->stack); c && n--; c = next_visible(c->snext));
	return c;
}
static void alt_tab(const Arg arg) {
	/* put all of the windows back in their original focus/stack position */
	Client* c;
	int i;
	int visible = count_visible();
	if (visible == 0) return;
	for (i = 0; i < alt_tab_count; ++i) {
		c = get_nth_client(alt_tab_count);
		detachstack(c);
		attachstack(c);
	}
	/* focus and restack the nth window */
	alt_tab_count = (alt_tab_count + 1) % visible;
	c = get_nth_client(alt_tab_count);
	focus(c);
	restack(c->mon);
}

/* commands */
#define TERM "term"
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = {
	"dmenu_run", "-m", dmenumon,
	"-i", "-fn", FONT,
	"-a", DMENUALPHA,
	"-nb", colors[SchemeNorm][1], "-nf", colors[SchemeNorm][0],
	"-sb", colors[SchemeSel][1], "-sf", colors[SchemeSel][0],
	"-nhb", colors[SchemeNorm][1], /* "-nhf", colors[SchemeSel][0], */
	"-shb", colors[SchemeSel][1], /* "-shf", colors[SchemeNorm][0], */
	NULL
};
static const char *termcmd[]           = { TERM,                NULL };
static const char *brightnessupcmd[]   = { "brightness", "+10", NULL };
static const char *brightnessdowncmd[] = { "brightness", "-10", NULL };
static const char *volumeupcmd[]       = { "volume", "+5",      NULL };
static const char *volumedowncmd[]     = { "volume", "-5",      NULL };
static const char *volumemutecmd[]     = { "volume", "mute",    NULL };
static const char *screencmd[]         = { "screenshot",        NULL };
static const char *screenallcmd[]      = { "screenshot", "1",   NULL };
static const char *xkillcmd[]          = { "xkill",             NULL };

/* autostart */
#define AUTOSTART "~/.dwm/autostart.sh"
static const char *const autostart[] = {
	AUTOSTARTDISOWN, TERM, NULL,
	"/bin/sh", "-c", AUTOSTART, NULL,
	NULL /* terminate */
};

/* keybinds */
#define MODKEY Mod4Mask
#define AltMask Mod1Mask
#define BINDTAG(n) \
	{ MODKEY,                       XK_##n,                    cmdview,              { .ui = 1 << (n - 1)                       } }, \
	{ MODKEY|ShiftMask,             XK_##n,                    cmdtag,               { .ui = 1 << (n - 1)                       } }, \
	{ MODKEY|ControlMask,           XK_##n,                    cmdtoggleview,        { .ui = 1 << (n - 1)                       } }, \
	{ MODKEY|ShiftMask|ControlMask, XK_##n,                    cmdtoggletag,         { .ui = 1 << (n - 1)                       } }, \
	{ MODKEY|AltMask,               XK_##n,                    cmdsendmon,           { .i  = n                                  } }
static const Key keys[] = {
	BINDTAG(1), BINDTAG(2), BINDTAG(3), BINDTAG(4), BINDTAG(5), BINDTAG(6), BINDTAG(7), BINDTAG(8), BINDTAG(9),
	/* modifier                     key                        function              argument */
	
	{ MODKEY,                       XK_t,                      cmdspawn,             { .v = termcmd                             } },
	{ MODKEY,                       XK_r,                      cmdspawn,             { .v = dmenucmd                            } },
	{ MODKEY,                       XK_Print,                  cmdspawn,             { .v = screenallcmd                        } },
	{ 0,                            XK_Print,                  cmdspawn,             { .v = screencmd                           } },

	{ MODKEY|ControlMask,           XK_Delete,                 cmdquit,              { 0                                        } },
	{ MODKEY,                       XK_F4,                     cmdkillclient,        { 0                                        } },
	{ MODKEY|ShiftMask,             XK_F4,                     cmdspawn,             { .v = xkillcmd                            } },

	{ 0,                            XK_Super_L,                start_alt_tab,        { 0                                        } },
	{ MODKEY,                       XK_Tab,                    alt_tab,              { 0                                        } },
	
	{ MODKEY,                       XK_space,                  cmdtogglealwaysontop, { .b = false                               } },
	{ MODKEY|ShiftMask,             XK_space,                  cmdtogglealwaysontop, { .b = true                                } },

	{ 0,                            XF86XK_MonBrightnessUp,    cmdspawn,             { .v = brightnessupcmd                     } },
	{ 0,                            XF86XK_MonBrightnessDown,  cmdspawn,             { .v = brightnessdowncmd                   } },
	{ 0,                            XF86XK_AudioLowerVolume,   cmdspawn,             { .v = volumeupcmd                         } },
	{ 0,                            XF86XK_AudioRaiseVolume,   cmdspawn,             { .v = volumedowncmd                       } },
	{ 0,                            XF86XK_AudioMute,          cmdspawn,             { .v = volumemutecmd                       } },

	{ MODKEY,                       XK_q,                      cmdsetposition,       { .i = PositionNW                          } },
	{ MODKEY|ShiftMask,             XK_q,                      cmdsetposition,       { .i = PositionNW         | PositionForced } },
	{ MODKEY,                       XK_a,                      cmdsetposition,       { .i = PositionW                           } },
	{ MODKEY|ShiftMask,             XK_a,                      cmdsetposition,       { .i = PositionW          | PositionForced } },
	{ MODKEY,                       XK_z,                      cmdsetposition,       { .i = PositionSW                          } },
	{ MODKEY|ShiftMask,             XK_z,                      cmdsetposition,       { .i = PositionSW         | PositionForced } },
	{ MODKEY,                       XK_w,                      cmdsetposition,       { .i = PositionN                           } },
	{ MODKEY|ShiftMask,             XK_w,                      cmdsetposition,       { .i = PositionN          | PositionForced } },
	{ MODKEY,                       XK_s,                      cmdsetposition,       { .i = PositionFill                        } },
	{ MODKEY|ShiftMask,             XK_s,                      cmdsetposition,       { .i = PositionFill       | PositionForced } },
	{ MODKEY,                       XK_x,                      cmdsetposition,       { .i = PositionS                           } },
	{ MODKEY|ShiftMask,             XK_x,                      cmdsetposition,       { .i = PositionS          | PositionForced } },
	{ MODKEY,                       XK_e,                      cmdsetposition,       { .i = PositionNE                          } },
	{ MODKEY|ShiftMask,             XK_e,                      cmdsetposition,       { .i = PositionNE         | PositionForced } },
	{ MODKEY,                       XK_d,                      cmdsetposition,       { .i = PositionE                           } },
	{ MODKEY|ShiftMask,             XK_d,                      cmdsetposition,       { .i = PositionE          | PositionForced } },
	{ MODKEY,                       XK_c,                      cmdsetposition,       { .i = PositionSE                          } },
	{ MODKEY|ShiftMask,             XK_c,                      cmdsetposition,       { .i = PositionSE         | PositionForced } },
	{ MODKEY|ControlMask,           XK_s,                      cmdsetposition,       { .i = PositionCenter                      } },
	{ MODKEY|ShiftMask|ControlMask, XK_s,                      cmdsetposition,       { .i = PositionCenter     | PositionForced } },
	{ MODKEY|ShiftMask,             XK_s,                      cmdsetposition,       { .i = PositionFullscreen | PositionForced } },

	{ MODKEY|ShiftMask,             XK_F1,                     cmdsetgapwindow,      { .i = -10                                 } },
	{ MODKEY|ShiftMask,             XK_F2,                     cmdsetgapbar,         { .i = -10                                 } },
	{ MODKEY|ShiftMask,             XK_F3,                     cmdsetgapedge,        { .i = -10                                 } },
	{ MODKEY|ControlMask,           XK_F1,                     cmdsetgapwindow,      { .i = 10                                  } },
	{ MODKEY|ControlMask,           XK_F2,                     cmdsetgapbar,         { .i = 10                                  } },
	{ MODKEY|ControlMask,           XK_F3,                     cmdsetgapedge,        { .i = 10                                  } },

	{ MODKEY|ShiftMask,             XK_F4,                     cmdsetmfact,          { .f = -0.05                               } },
	{ MODKEY|ShiftMask,             XK_F5,                     cmdincnmaster,        { .i = -1                                  } },
	{ MODKEY|ControlMask,           XK_F4,                     cmdsetmfact,          { .f = 0.05                                } },
	{ MODKEY|ControlMask,           XK_F5,                     cmdincnmaster,        { .i = 1                                   } }

};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click                event mask          button          function           argument */
	{ ClkLtSymbol,          0,                  Button1,        cmdinclayout,      { .i =  1          } },
	{ ClkLtSymbol,          0,                  Button2,        cmdsetlayout,      { .v = &layouts[0] } },
	{ ClkLtSymbol,          0,                  Button3,        cmdinclayout,      { .i = -1          } },
	{ ClkWinTitle,          0,                  Button1,        cmdfocusclient,    { 0                } },
	{ ClkWinTitle,          0,                  Button2,        cmdtogglefloating, { 0                } },
	{ ClkWinTitle,          ShiftMask,          Button3,        cmdkillclient,     { 0                } },
	{ ClkWinTitle,          0,                  Button3,        cmdfocusclient,    { 0                } },
	{ ClkStatusText,        0,                  Button1,        cmdspawn,          { .v = termcmd     } },
	{ ClkStatusText,        0,                  Button2,        cmdspawn,          { .v = termcmd     } },
	{ ClkStatusText,        0,                  Button3,        cmdspawn,          { .v = termcmd     } },
	{ ClkClientWin,         MODKEY,             Button1,        cmdmovemouse,      { .i = false       } },
	{ ClkClientWin,         MODKEY,             Button3,        cmdresizemouse,    { .i = false       } },
	{ ClkClientWin,         MODKEY|ShiftMask,   Button1,        cmdmovemouse,      { .i = true        } },
	{ ClkClientWin,         MODKEY|ShiftMask,   Button3,        cmdresizemouse,    { .i = true        } },
	{ ClkClientWin,         MODKEY|ShiftMask,   Button2,        cmdtogglefloating, { 0                } },
	{ ClkTagBar,            0,                  Button1,        cmdview,           { 0                } },
	{ ClkTagBar,            0,                  Button3,        cmdtoggleview,     { 0                } },
	{ ClkTagBar,            MODKEY,             Button1,        cmdtag,            { 0                } },
	{ ClkTagBar,            MODKEY,             Button3,        cmdtoggletag,      { 0                } },
};
