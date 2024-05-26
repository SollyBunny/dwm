# 0 "config.h"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "config.h"


static const unsigned int focusonhover = 0;
static const unsigned int focusonwheel = 0;


static const unsigned int borderpx = 0;
static const unsigned int gappx = 10;
static const unsigned int snap = 0;
static const int showbar = 1;
static const int topbar = 1;



static const char font[] = "Iosevka Nerd" " " "17";
static const char dmenufont[] = "Iosevka Nerd" ":size=" "17";
static const char col_bg [] = "#000000";
static const char col_fg [] = "#ffddff";
static const char col_txt[] = "#ffffff";
static const unsigned int alpha = 0xff * 0.7;

static const char *const colors[][3] = {

 [SchemeNorm] = { col_txt, col_bg, 0 },
 [SchemeSel] = { col_bg , col_fg, 0 },
};
static const unsigned int alphas[][3] = {

 [SchemeNorm] = { OPAQUE, alpha, OPAQUE },
 [SchemeSel] = { OPAQUE, alpha, OPAQUE },
};

static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {





 { NULL, NULL, NULL, 0, False, -1 },


};


static int alt_tab_count = 0;
static void focus_restack(Client *c) { if (c) { focus(c); restack(selmon); } }
static void start_alt_tab(const Arg *arg) { alt_tab_count = 0; }
static Client *next_visible(Client *c) {
 for( ; c && !ISVISIBLE(c); c = c->snext);
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


static const float mfact = 0.55;
static const int nmaster = 1;
static const int resizehints = 1;
static const int lockfullscreen = 0;

static const Layout layouts[] = {

 { "󰙀", tile },
 { "󰕭", col },
 { "", NULL },

};



static char dmenumon[2] = "0";
static const char *dmenucmd[] = {
 "dmenu_run", "-m", dmenumon,
 "-i", "-fn", dmenufont,
 "-a", "178",
 "-nb", colors[SchemeNorm][1], "-nf", colors[SchemeNorm][0],
 "-sb", colors[SchemeSel][1], "-sf", colors[SchemeSel][0],
 "-nhb", colors[SchemeNorm][1],
 "-shb", colors[SchemeSel][1],
 NULL
};
static const char *termcmd[] = { "kitty", NULL };

static const char *screencmd[] = { "screenshot", NULL };
static const char *screenallcmd[] = { "screenshot", "1", NULL };
static const char *xkillcmd[] = { "xkill", NULL };



static const char *const autostart[] = {
 "/bin/sh", "-c", "~/.dwm/autostart.sh", NULL,
 "kitty", NULL,
 NULL
};
# 126 "config.h"
static const Key keys[] = {
 { Mod4Mask, XK_1, view, { .ui = 1 << (1 - 1) } }, { Mod4Mask|ShiftMask, XK_1, tag, { .ui = 1 << (1 - 1) } }, { Mod4Mask|ControlMask, XK_1, toggleview, { .ui = 1 << (1 - 1) } }, { Mod4Mask|ShiftMask|ControlMask, XK_1, toggletag, { .ui = 1 << (1 - 1) } }, { Mod4Mask, XK_2, view, { .ui = 1 << (2 - 1) } }, { Mod4Mask|ShiftMask, XK_2, tag, { .ui = 1 << (2 - 1) } }, { Mod4Mask|ControlMask, XK_2, toggleview, { .ui = 1 << (2 - 1) } }, { Mod4Mask|ShiftMask|ControlMask, XK_2, toggletag, { .ui = 1 << (2 - 1) } }, { Mod4Mask, XK_3, view, { .ui = 1 << (3 - 1) } }, { Mod4Mask|ShiftMask, XK_3, tag, { .ui = 1 << (3 - 1) } }, { Mod4Mask|ControlMask, XK_3, toggleview, { .ui = 1 << (3 - 1) } }, { Mod4Mask|ShiftMask|ControlMask, XK_3, toggletag, { .ui = 1 << (3 - 1) } }, { Mod4Mask, XK_4, view, { .ui = 1 << (4 - 1) } }, { Mod4Mask|ShiftMask, XK_4, tag, { .ui = 1 << (4 - 1) } }, { Mod4Mask|ControlMask, XK_4, toggleview, { .ui = 1 << (4 - 1) } }, { Mod4Mask|ShiftMask|ControlMask, XK_4, toggletag, { .ui = 1 << (4 - 1) } }, { Mod4Mask, XK_5, view, { .ui = 1 << (5 - 1) } }, { Mod4Mask|ShiftMask, XK_5, tag, { .ui = 1 << (5 - 1) } }, { Mod4Mask|ControlMask, XK_5, toggleview, { .ui = 1 << (5 - 1) } }, { Mod4Mask|ShiftMask|ControlMask, XK_5, toggletag, { .ui = 1 << (5 - 1) } }, { Mod4Mask, XK_6, view, { .ui = 1 << (6 - 1) } }, { Mod4Mask|ShiftMask, XK_6, tag, { .ui = 1 << (6 - 1) } }, { Mod4Mask|ControlMask, XK_6, toggleview, { .ui = 1 << (6 - 1) } }, { Mod4Mask|ShiftMask|ControlMask, XK_6, toggletag, { .ui = 1 << (6 - 1) } }, { Mod4Mask, XK_7, view, { .ui = 1 << (7 - 1) } }, { Mod4Mask|ShiftMask, XK_7, tag, { .ui = 1 << (7 - 1) } }, { Mod4Mask|ControlMask, XK_7, toggleview, { .ui = 1 << (7 - 1) } }, { Mod4Mask|ShiftMask|ControlMask, XK_7, toggletag, { .ui = 1 << (7 - 1) } }, { Mod4Mask, XK_8, view, { .ui = 1 << (8 - 1) } }, { Mod4Mask|ShiftMask, XK_8, tag, { .ui = 1 << (8 - 1) } }, { Mod4Mask|ControlMask, XK_8, toggleview, { .ui = 1 << (8 - 1) } }, { Mod4Mask|ShiftMask|ControlMask, XK_8, toggletag, { .ui = 1 << (8 - 1) } }, { Mod4Mask, XK_9, view, { .ui = 1 << (9 - 1) } }, { Mod4Mask|ShiftMask, XK_9, tag, { .ui = 1 << (9 - 1) } }, { Mod4Mask|ControlMask, XK_9, toggleview, { .ui = 1 << (9 - 1) } }, { Mod4Mask|ShiftMask|ControlMask, XK_9, toggletag, { .ui = 1 << (9 - 1) } },

 { Mod4Mask, XK_t, spawn, { .v = termcmd } },
 { Mod4Mask, XK_r, spawn, { .v = dmenucmd } },

 { Mod4Mask, XK_Print, spawn, { .v = screenallcmd } },
 { 0, XK_Print, spawn, { .v = screencmd } },

 { Mod4Mask|ControlMask, XK_Delete, quit, { 0 } },
 { Mod4Mask, XK_F4, killclient, { 0 } },
 { Mod4Mask|ShiftMask, XK_F4, spawn, { .v = xkillcmd } },

 { 0, XK_Super_L, start_alt_tab, { 0 } },
 { Mod4Mask, XK_Tab, alt_tab, { 0 } },

 { Mod4Mask, XK_space, togglealwaysontop, { .ui = 0 } },
 { Mod4Mask|ShiftMask, XK_space, togglealwaysontop, { .ui = 1 } },

 { Mod4Mask, XK_q, moveresizetile, { .ui = TileNW } },
 { Mod4Mask, XK_a, moveresizetile, { .ui = TileW } },
 { Mod4Mask, XK_z, moveresizetile, { .ui = TileSW } },

 { Mod4Mask, XK_w, moveresizetile, { .ui = TileN } },
 { Mod4Mask, XK_s, moveresizetile, { .ui = TileFill } },
 { Mod4Mask, XK_x, moveresizetile, { .ui = TileS } },

 { Mod4Mask, XK_e, moveresizetile, { .ui = TileNE } },
 { Mod4Mask, XK_d, moveresizetile, { .ui = TileE } },
 { Mod4Mask, XK_c, moveresizetile, { .ui = TileSE } },

 { Mod4Mask|ShiftMask, XK_s, moveresizetile, { .ui = TileFullscreen } },
 { Mod4Mask|ControlMask, XK_s, moveresizetile, { .ui = TileCenter } },
 { Mod4Mask|ControlMask|ShiftMask, XK_s, moveresizetile, { .ui = TileDoubleFullscreen } },

};



static const Button buttons[] = {

 { ClkLtSymbol, 0, Button1, inclayout, { .i = 1 } },
 { ClkLtSymbol, 0, Button2, setlayout, { .v = &layouts[0] } },
 { ClkLtSymbol, 0, Button3, inclayout, { .i = -1 } },
 { ClkWinTitle, 0, Button1, focusclient, {0} },
 { ClkWinTitle, 0, Button2, togglefloating, {0} },
 { ClkWinTitle, 0, Button3, killclient, {0} },
 { ClkStatusText, 0, Button1, spawn, { .v = termcmd } },
 { ClkStatusText, 0, Button2, spawn, { .v = termcmd } },
 { ClkStatusText, 0, Button3, spawn, { .v = termcmd } },
 { ClkClientWin, Mod4Mask, Button1, movemouse, {0} },
 { ClkClientWin, Mod4Mask, Button2, togglefloating, {0} },
 { ClkClientWin, Mod4Mask, Button3, resizemouse, {0} },
 { ClkTagBar, 0, Button1, view, {0} },
 { ClkTagBar, 0, Button3, toggleview, {0} },
 { ClkTagBar, Mod4Mask, Button1, tag, {0} },
 { ClkTagBar, Mod4Mask, Button3, toggletag, {0} },
};
