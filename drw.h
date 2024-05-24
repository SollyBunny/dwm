/* See LICENSE file for copyright and license details. */

enum { ColFg, ColBg, ColBorder }; /* Clr scheme index */

typedef struct Cur Cur;
struct Cur {
	Cursor cursor;
};

typedef struct Fnt Fnt;
struct Fnt {
	Display *dpy;
	unsigned int h;
	PangoLayout *layout;
};

typedef XftColor Clr;

typedef struct Drw Drw;
struct Drw {
	unsigned int w, h;
	Display *dpy;
	int screen;
	Window root;
    Visual *visual;
    unsigned int depth;
    Colormap cmap;
	Drawable drawable;
	Picture picture;
	GC gc;
	Clr *scheme;
	Fnt *font;
};

/* Drawable abstraction */
Drw* drw_create(Display *dpy, int screen, Window win, unsigned int w, unsigned int h, Visual *visual, unsigned int depth, Colormap cmap);
void drw_resize(Drw *drw, unsigned int w, unsigned int h);
void drw_free(Drw *drw);

/* Fnt abstraction */
Fnt* drw_font_create(Drw* drw, const char font[]);
void drw_font_free(Fnt* set);
unsigned int drw_font_getwidth(Drw *drw, const char *text, Bool markup);
unsigned int drw_font_getwidth_clamp(Drw *drw, const char *text, unsigned int n, Bool markup);
void drw_font_getexts(Fnt *font, const char *text, unsigned int len, unsigned int *w, unsigned int *h, Bool markup);

/* Colorscheme abstraction */
void drw_clr_create(Drw *drw, Clr *dest, const char *clrname, unsigned int alpha);
Clr* drw_scm_create(Drw *drw, const char *const clrnames[], const unsigned int alphas[], size_t clrcount);

/* Cursor abstraction */
Cur* drw_cur_create(Drw *drw, int shape);
void drw_cur_free(Drw *drw, Cur *cursor);

/* Drawing context manipulation */
void drw_setscheme(Drw *drw, Clr *scm);

Picture drw_picture_create_resized(Drw *drw, char *src, unsigned int src_w, unsigned int src_h, unsigned int dst_w, unsigned int dst_h);

/* Drawing functions */
void drw_rect(Drw *drw, int x, int y, unsigned int w, unsigned int h, int filled, int invert);
int drw_text(Drw *drw, int x, int y, unsigned int w, unsigned int h, unsigned int lpad, const char *text, int invert, Bool markup);
void drw_pic(Drw *drw, int x, int y, unsigned int w, unsigned int h, Picture pic);

/* Map functions */
void drw_map(Drw *drw, Window win, int x, int y, unsigned int w, unsigned int h);
