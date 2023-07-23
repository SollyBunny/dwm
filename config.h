/* See LICENSE file for copyright and license details. */

/* definitions */
#define SCREENEDP "eDP-1"
//#define SCREENHDM "HDMI-1-1" // Noevau
#define SCREENHDM "HDMI-1-0"
#define MODKEY Mod4Mask
// #define ALTKEY Mod1Mask
#define GAP 10
#define BH 18
#define W 1920
#define H (1080-BH-(GAP*2))

/* appearance */
static const char *fonts[]          = { "SourceCodePro-ExtraLight:pixelsize=15:antialias=false:autohint=true" };
static const char col_bg []         = "#000000";
static const char col_fg []         = "#ffddff";
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
	"/home/install/util/theme/run.sh", NULL,
	"dunst", NULL,
	//"xcompmgr", NULL,
	/*
picom --backend glx --glx-no-stencil --glx-no-rebind-pixmap --blur-method dual_kawase --blur-strength 5 --vsync --no-fading-openclose -r 10 -c --corner-radius 5 --shadow-offset-x -10 --shadow-offset-y -10 --shadow-red 1 --shadow-green 0.8 --shadow-blue 1
	*/
	"picom",
		"--backend", "glx", 
		"--glx-no-stencil", "--glx-no-rebind-pixmap", 
		"--blur-method", "dual_kawase", "--blur-strength", "5", 
		"--vsync", "--no-fading-openclose", 
		"--corner-radius", "5", 
		"-c",
		"-r", "10",
		"--shadow-offset-x", "-10", "--shadow-offset-y", "-10",
		"--shadow-red", "1", "--shadow-green", "0.8", "--shadow-blue", "1",
	NULL,
	"browser", NULL,
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

#define TERM "kitty"

static const char *termcmd[]      = { TERM,                                          NULL };
static const char *fmcmd[]        = { "pcmanfm",                                     NULL };
static const char *pavucmd[]      = { TERM, "-e", "pulsemixer",                      NULL };

static const char *brcmd[]        = { "browser",                                     NULL };
static const char *mccmd[]        = { "prism",                                       NULL };
static const char *stcmd[]        = { "steam",                                       NULL };
static const char *twcmd[]        = { "ddnet",                                       NULL };

static const char *screencmd[]    = { "screenshot",                                  NULL };
static const char *screenallcmd[] = { "screenshot", "1",                             NULL };
static const char *xkillcmd[]     = { "xkill",                                       NULL };
static const char *sleepcmd[]     = { "doas", "/home/install/util/scripts/sleep.sh", NULL };
static const char *goodstartcmd[] = { "/home/install/util/scripts/goodstart.sh",     NULL };

static Key keys[] = {
	/* modifier             key        function            argument */

	{ MODKEY,		        XK_t,	   spawn,         	  { .v = termcmd      } },
	{ MODKEY,				XK_e,      spawn,			  { .v = fmcmd        } },
	{ MODKEY,		        XK_v,	   spawn,         	  { .v = pavucmd      } },

	{ MODKEY,				XK_f,      spawn,			  { .v = brcmd        } },
	{ MODKEY,				XK_m,      spawn,			  { .v = mccmd        } },
	{ MODKEY,				XK_s,      spawn,			  { .v = stcmd        } },
	{ MODKEY,				XK_d,      spawn,			  { .v = twcmd        } },

	{ MODKEY,				XK_Print,  spawn,			  { .v = screenallcmd } },
	{ 0,					XK_Print,  spawn,			  { .v = screencmd    } },

	{ MODKEY|ControlMask,   XK_Delete, quit,              { 0 			      } },
	{ MODKEY,             	XK_F4,     killclient,        { 0                 } },
	{ MODKEY|ShiftMask,	    XK_F4,     spawn,			  { .v = xkillcmd     } },

	{ 0,                    XK_Super_L, start_alt_tab,    { 0                 } },
    { MODKEY,               XK_Tab,    alt_tab,           { 0                 } },

	{ MODKEY,		        XK_p,	   spawn,         	  { .v = sleepcmd     } },
	{ MODKEY,		        XK_g,	   spawn,         	  { .v = goodstartcmd } },
    
    { MODKEY,               XK_space,  togglealwaysontop, { 0                 } },
	
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

	{ MODKEY|ShiftMask,     XK_q,      moveresize,        { .v = (int []){ GAP                  , GAP                  , W/2 - (GAP*1.5), H/2 - GAP } } },
	{ MODKEY|ShiftMask,     XK_a,      moveresize,        { .v = (int []){ GAP                  , GAP                  , W/2 - (GAP*1.5), H   - GAP } } },
	{ MODKEY|ShiftMask,     XK_z,      moveresize,        { .v = (int []){ GAP                  , H/2 + GAP            , W/2 - (GAP*1.5), H/2 - GAP } } },

	{ MODKEY,               XK_q,      moveresize,        { .v = (int []){ GAP                  , GAP                  , W   - (GAP*2  ), H/2 - GAP } } },
	{ MODKEY,               XK_a,      moveresize,        { .v = (int []){ GAP                  , GAP                  , W   - (GAP*2  ), H   - GAP } } },
	{ MODKEY,               XK_z,      moveresize,        { .v = (int []){ GAP                  , H/2 + GAP            , W   - (GAP*2  ), H/2 - GAP } } },

	{ MODKEY|ControlMask,   XK_q,      moveresize,        { .v = (int []){ (W/2) + (GAP*0.5)    , GAP                  , W/2 - (GAP*1.5), H/2 - GAP } } },
	{ MODKEY|ControlMask,   XK_a,      moveresize,        { .v = (int []){ (W/2) + (GAP*0.5)    , GAP                  , W/2 - (GAP*1.5), H   - GAP } } },
	{ MODKEY|ControlMask,   XK_z,      moveresize,        { .v = (int []){ (W/2) + (GAP*0.5)    , H/2 + GAP            , W/2 - (GAP*1.5), H/2 - GAP } } },

	{ MODKEY|ControlMask|ShiftMask, XK_q, moveresize,     { .v = (int []){ W/2 - W/4 + (GAP*.75), H/2 - H/4 - (GAP*0.5), W/2 - (GAP*1.5), H/2 - GAP } } },	
	{ MODKEY|ControlMask|ShiftMask, XK_a, moveresize,     { .v = (int []){ 0                    , 0                    , 1920           , 1080      } } },
	{ MODKEY|ControlMask|ShiftMask, XK_z, moveresize,     { .v = (int []){ -1920                , -1020                , 1920*2         , 1080*2    } } }
	
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	
	{ ClkStatusText,        0,              Button1,        spawn,          {.v = termcmd } },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkStatusText,        0,              Button3,        spawn,          {.v = termcmd } },
	
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	
};

