/* See LICENSE file for copyright and license details. */
enum { ColFg, ColBg, ColBorder }; /* Clr scheme index */

#ifdef NODRW
typedef struct {
	unsigned long pixel;
} Clr;

void drw_clr_create(Clr* dest, const char* clrname, unsigned int alpha);
Clr* drw_scm_create(const char* const clrnames[], const unsigned int alphas[], size_t clrcount);

#else

typedef XftColor Clr;

typedef struct Fnt Fnt;
struct Fnt {
	Display* dpy;
	unsigned int h;
	XftFont* xfont;
	FcPattern* pattern;
	struct Fnt* next;
};

typedef struct Drw Drw;
struct Drw {
	unsigned int w, h;
	Display* dpy;
	int screen;
	Window root;
	Visual* visual;
	unsigned int depth;
	Colormap cmap;
	Drawable drawable;
	Picture picture;
	GC gc;
	Clr* scheme;
	Fnt* fonts;
};

/* Drawable abstraction */
Drw* drw_create(Display* dpy, int screen, Window root, unsigned int w, unsigned int h, Visual* visual, unsigned int depth, Colormap cmap);
void drw_resize(Drw* drw, unsigned int w, unsigned int h);
void drw_free(Drw* drw);

/* Fnt abstraction */
Fnt* drw_fontset_create(Drw* drw, const char* fonts[], size_t fontcount);
void drw_fontset_free(Fnt* set);
unsigned int drw_fontset_getwidth(Drw* drw, const char* text);
unsigned int drw_fontset_getwidth_clamp(Drw* drw, const char* text, unsigned int n);
void drw_font_getexts(Fnt* font, const char* text, unsigned int len, unsigned int* w, unsigned int* h);

/* Colorscheme abstraction */
void drw_clr_create(Drw* drw, Clr* dest, const char* clrname, unsigned int alpha);
Clr* drw_scm_create(Drw* drw, const char* const clrnames[], const unsigned int alphas[], size_t clrcount);

/* Drawing context manipulation */
void drw_setscheme(Drw* drw, Clr* scm);

Picture drw_picture_create_resized(Drw* drw, char* src, unsigned int src_w, unsigned int src_h, unsigned int dst_w, unsigned int dst_h);

/* Drawing functions */
void drw_rect(Drw* drw, int x, int y, unsigned int w, unsigned int h, bool filled, bool invert);
int drw_text(Drw* drw, int x, int y, unsigned int w, unsigned int h, unsigned int lpad, const char* text, bool invert);
void drw_pic(Drw* drw, int x, int y, unsigned int w, unsigned int h, Picture pic);

/* Map functions */
void drw_map(Drw* drw, Window win, int x, int y, unsigned int w, unsigned int h);

#endif /* NODRW */