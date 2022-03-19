/* See LICENSE file for copyright and license details. */

/* definitions */
#define SCREENEDP "eDP-1"
#define SCREENHDM "HDMI-1-0"
#define MODKEY Mod4Mask
#define ALTKEY Mod1Mask
#define GAP 10
#define BH 18
#define W 1920
#define H (1080-BH-(GAP*2))

/* appearance */
static const char *fonts[]          = { "SourceCodePro-ExtraLight:pixelsize=15:antialias=false:autohint=true" };
static const char col_bg []         = "#000000";
static const char col_fg []         = "#ff00ff";
static const char col_txt[]         = "#ffffff";
static const unsigned int alpha     = 0xff * 0.7;

static const char *colors[][2]      = {
	/*               fg       bg     */
	[SchemeNorm] = { col_txt, col_bg },
	[SchemeSel]  = { col_bg , col_fg },
};

static const unsigned int alphas[][2]      = {
	/*               fg      bg    */
	[SchemeNorm] = { OPAQUE, alpha },
	[SchemeSel]  = { OPAQUE, alpha },
};

/* autostart */

static const char *const autostart[] = {
	"setxkbmap", "-model", "USB Keyboard", "-layout", "gb", NULL,
	"xrandr", "--output", SCREENHDM, "--primary", NULL,
	"xrandr", "--output", SCREENHDM, "--preferred", NULL,
	"xrandr", "--output", SCREENEDP, "--off", NULL,
	"status", NULL,
	"/home/solly/Backgrounds/fehbg", NULL,
	/*
		"picom",
			"--experimental-backends", "--backend", "glx", 
			"--glx-no-stencil", "--glx-no-rebind-pixmap", 
			"--blur-method", "dual_kawase", "--blur-strength", "3", 
			"--vsync", "--no-fading-openclose", 
			//"--transparent-clipping",
			"-r", "5", 
			"--corner-radius", "5", 
		"xrandr", "-s", "1920x1080", NULL, 
	*/
	NULL
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

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
   for (int i = 0; i < alt_tab_count; ++i) focus_restack(get_nth_client(alt_tab_count));
   int visible = count_visible();
   if (visible == 0) return;
   alt_tab_count = (alt_tab_count + 1) % visible;
   focus_restack(get_nth_client(alt_tab_count));
}

/* commands */
static const char *termcmd[]      = { "st",                                        NULL };
static const char *fmcmd[]        = { "pcmanfm",                                   NULL };
static const char *pavucmd[]      = { "/home/install/util/scripts/pavucontrol.sh", NULL };

static const char *ffcmd[]        = { "firefox",                                   NULL };
static const char *dccmd[]        = { "discord",                                   NULL };
static const char *mccmd[]        = { "multimc",                                   NULL };

static const char *screencmd[]    = { "screenshot",                                NULL};
static const char *xkillcmd[]     = { "xkill",                                     NULL };

static Key keys[] = {
	/* modifier                        key        function        argument */

	{ MODKEY,		        XK_t,	   spawn,         	  { .v = termcmd   } },
	{ MODKEY,				XK_e,      spawn,			  { .v = fmcmd     } },
	{ MODKEY,		        XK_v,	   spawn,         	  { .v = pavucmd   } },

	{ MODKEY,				XK_f,      spawn,			  { .v = ffcmd     } },
	{ MODKEY,				XK_d,      spawn,			  { .v = dccmd     } },
	{ MODKEY,				XK_m,      spawn,			  { .v = mccmd     } },

	{ 0,					XK_Print,  spawn,			  { .v = screencmd } },
	{ MODKEY,				XK_F4,     spawn,			  { .v = xkillcmd  } },

	{ ALTKEY|ControlMask,   XK_Delete, quit,              { 0 			   } },
	{ ALTKEY,             	XK_F4,     killclient,        { 0              } },

	{ 0,                    XK_Alt_L,  start_alt_tab,     { 0              } },
    { ALTKEY,               XK_Tab,    alt_tab,           { 0              } },
    
    { ALTKEY,               XK_space,  togglealwaysontop, { 0              } },
	
	{ ALTKEY,               XK_1,      view,              { .ui = 1        } },
	{ ALTKEY,               XK_2,      view,              { .ui = 2        } },
	{ ALTKEY,               XK_3,      view,              { .ui = 4        } },
	{ ALTKEY,               XK_4,      view,              { .ui = 8        } },
	{ ALTKEY,               XK_5,      view,              { .ui = 16       } },
	{ ALTKEY,               XK_6,      view,              { .ui = 32       } },
	{ ALTKEY,               XK_7,      view,              { .ui = 64       } },
	{ ALTKEY,               XK_8,      view,              { .ui = 128      } },
	{ ALTKEY,               XK_9,      view,              { .ui = 256      } },

	{ ALTKEY|ShiftMask,     XK_1,      tag,               { .i = 1         } },
	{ ALTKEY|ShiftMask,     XK_2,      tag,               { .i = 2         } },
	{ ALTKEY|ShiftMask,     XK_3,      tag,               { .i = 4         } },
	{ ALTKEY|ShiftMask,     XK_4,      tag,               { .i = 8         } },
	{ ALTKEY|ShiftMask,     XK_5,      tag,               { .i = 16        } },
	{ ALTKEY|ShiftMask,     XK_6,      tag,               { .i = 32        } },
	{ ALTKEY|ShiftMask,     XK_7,      tag,               { .i = 64        } },
	{ ALTKEY|ShiftMask,     XK_8,      tag,               { .i = 128       } },
	{ ALTKEY|ShiftMask,     XK_9,      tag,               { .i = 256       } },

	{ ALTKEY,				XK_q,	   moveresize,		  { .v = (int []){ GAP                  , GAP                  , W/2 - (GAP*1.5), H/2 - GAP } } },
	{ ALTKEY,				XK_w,	   moveresize,		  { .v = (int []){ GAP                  , GAP                  , W   - (GAP*2  ), H/2 - GAP } } },
	{ ALTKEY,				XK_e,	   moveresize,		  { .v = (int []){ (W/2) + (GAP*0.5)    , GAP                  , W/2 - (GAP*1.5), H/2 - GAP } } },

	{ ALTKEY,				XK_a,	   moveresize,		  { .v = (int []){ GAP                  , GAP                  , W/2 - (GAP*1.5), H   - GAP } } },
	{ ALTKEY,				XK_s,	   moveresize,		  { .v = (int []){ GAP                  , GAP                  , W   - (GAP*2  ), H   - GAP } } },
	{ ALTKEY,				XK_d,	   moveresize,		  { .v = (int []){ (W/2) + (GAP*0.5)    , GAP                  , W/2 - (GAP*1.5), H   - GAP } } },

	{ ALTKEY,				XK_z,	   moveresize,		  { .v = (int []){ GAP                  , H/2 + GAP            , W/2 - (GAP*1.5), H/2 - GAP } } },
	{ ALTKEY,				XK_x,	   moveresize,		  { .v = (int []){ GAP                  , H/2 + GAP            , W   - (GAP*2  ), H/2 - GAP } } },
	{ ALTKEY,				XK_c,	   moveresize,		  { .v = (int []){ (W/2) + (GAP*0.5)    , H/2 + GAP            , W/2 - (GAP*1.5), H/2 - GAP } } },
	
	{ ALTKEY,				XK_f,	   moveresize,		  { .v = (int []){ W/2 - W/4 + (GAP*.75), H/2 - H/4 - (GAP*0.5), W/2 - (GAP*1.5), H/2 - GAP } } },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	
	{ ClkStatusText,        0,              Button1,        spawn,          {.v = termcmd } },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkStatusText,        0,              Button3,        spawn,          {.v = termcmd } },
	
	{ ClkClientWin,         ALTKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         ALTKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	
};

