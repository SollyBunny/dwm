/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include <Imlib2.h>
#include <pango/pango.h>
#include <pango/pangoxft.h>

#include "drw.h"
#include "util.h"

Drw * drw_create(Display *dpy, int screen, Window root, unsigned int w, unsigned int h, Visual *visual, unsigned int depth, Colormap cmap) {
	Drw *drw = ecalloc(1, sizeof(Drw));

	drw->dpy = dpy;
	drw->screen = screen;
	drw->root = root;
	drw->w = w;
	drw->h = h;
	drw->visual = visual;
	drw->depth = depth;
	drw->cmap = cmap;
	drw->drawable = XCreatePixmap(dpy, root, w, h, depth);
	drw->picture = XRenderCreatePicture(dpy, drw->drawable, XRenderFindVisualFormat(dpy, visual), 0, NULL);
	drw->gc = XCreateGC(dpy, drw->drawable, 0, NULL);
	XSetLineAttributes(dpy, drw->gc, 1, LineSolid, CapButt, JoinMiter);

	return drw;
}

void drw_resize(Drw *drw, unsigned int w, unsigned int h) {
	if (!drw)
		return;
	drw->w = w;
	drw->h = h;
	if (drw->picture)
		XRenderFreePicture(drw->dpy, drw->picture);
	if (drw->drawable)
		XFreePixmap(drw->dpy, drw->drawable);
	drw->drawable = XCreatePixmap(drw->dpy, drw->root, w, h, drw->depth);
	drw->picture = XRenderCreatePicture(drw->dpy, drw->drawable, XRenderFindVisualFormat(drw->dpy, drw->visual), 0, NULL);
}

void drw_free(Drw *drw) {
	XRenderFreePicture(drw->dpy, drw->picture);
	XFreePixmap(drw->dpy, drw->drawable);
	XFreeGC(drw->dpy, drw->gc);
	drw_font_free(drw->font);
	free(drw);
}

/* This function is an implementation detail. Library users should use
 * drw_font_create instead.
 */
static Fnt * xfont_create(Drw *drw, const char *fontname) {
	Fnt *font;
	PangoFontMap *fontmap;
	PangoContext *context;
	PangoFontDescription *desc;
	PangoFontMetrics *metrics;

	if (!fontname) {
		die("no font specified.");
	}

	font = ecalloc(1, sizeof(Fnt));
	font->dpy = drw->dpy;

	fontmap = pango_xft_get_font_map(drw->dpy, drw->screen);
	context = pango_font_map_create_context(fontmap);
	desc = pango_font_description_from_string(fontname);
	font->layout = pango_layout_new(context);
	pango_layout_set_font_description(font->layout, desc);

	metrics = pango_context_get_metrics(context, desc, NULL);
	font->h = pango_font_metrics_get_height(metrics) / PANGO_SCALE;

	pango_font_metrics_unref(metrics);
	g_object_unref(context);

	return font;
}

static void xfont_free(Fnt *font) {
	if (!font)
		return;
	if (font->layout)
		g_object_unref(font->layout);
	free(font);
}

Fnt* drw_font_create(Drw* drw, const char font[]) {
	Fnt *fnt = NULL;

	if (!drw || !font)
		return NULL;

	fnt = xfont_create(drw, font);

	return (drw->font = fnt);
}

void drw_font_free(Fnt *font) {
	if (font) {
		xfont_free(font);
	}
}

void drw_clr_create(Drw *drw, Clr *dest, const char *clrname, unsigned int alpha) {
	if (!drw || !dest || !clrname)
		return;
	if (!XftColorAllocName(drw->dpy, DefaultVisual(drw->dpy, drw->screen),
                           DefaultColormap(drw->dpy, drw->screen),
                           clrname, dest))
		die("error, cannot allocate color '%s'", clrname);
	dest->pixel = (dest->pixel & 0x00ffffffU) | (alpha << 24);
}

/* Wrapper to create color schemes. The caller has to call free(3) on the
 * returned color scheme when done using it. */
Clr* drw_scm_create(Drw *drw, const char *const clrnames[], const unsigned int alphas[], size_t clrcount) {
	size_t i;
	Clr *ret;

	/* need at least two colors for a scheme */
	if (!drw || !clrnames || clrcount < 2 || !(ret = ecalloc(clrcount, sizeof(XftColor))))
		return NULL;

	for (i = 0; i < clrcount; i++)
		drw_clr_create(drw, &ret[i], clrnames[i], alphas[i]);
	return ret;
}

void drw_setscheme(Drw *drw, Clr *scm) {
	if (drw)
		drw->scheme = scm;
}

Picture drw_picture_create_resized(Drw *drw, char *src, unsigned int srcw, unsigned int srch, unsigned int dstw, unsigned int dsth) {
	Pixmap pm;
	Picture pic;
	GC gc;

	if (srcw <= (dstw << 1u) && srch <= (dsth << 1u)) {
		XImage img = {
			srcw, srch, 0, ZPixmap, src,
			ImageByteOrder(drw->dpy), BitmapUnit(drw->dpy), BitmapBitOrder(drw->dpy), 32,
			32, 0, 32,
			0, 0, 0
		};
		XInitImage(&img);

		pm = XCreatePixmap(drw->dpy, drw->root, srcw, srch, 32);
		gc = XCreateGC(drw->dpy, pm, 0, NULL);
		XPutImage(drw->dpy, pm, gc, &img, 0, 0, 0, 0, srcw, srch);
		XFreeGC(drw->dpy, gc);

		pic = XRenderCreatePicture(drw->dpy, pm, XRenderFindStandardFormat(drw->dpy, PictStandardARGB32), 0, NULL);
		XFreePixmap(drw->dpy, pm);

		XRenderSetPictureFilter(drw->dpy, pic, FilterBilinear, NULL, 0);
		XTransform xf;
		xf.matrix[0][0] = (srcw << 16u) / dstw; xf.matrix[0][1] = 0; xf.matrix[0][2] = 0;
		xf.matrix[1][0] = 0; xf.matrix[1][1] = (srch << 16u) / dsth; xf.matrix[1][2] = 0;
		xf.matrix[2][0] = 0; xf.matrix[2][1] = 0; xf.matrix[2][2] = 65536;
		XRenderSetPictureTransform(drw->dpy, pic, &xf);
	} else {
		Imlib_Image origin = imlib_create_image_using_data(srcw, srch, (DATA32 *)src);
		if (!origin) return None;
		imlib_context_set_image(origin);
		imlib_image_set_has_alpha(1);
		Imlib_Image scaled = imlib_create_cropped_scaled_image(0, 0, srcw, srch, dstw, dsth);
		imlib_free_image_and_decache();
		if (!scaled) return None;
		imlib_context_set_image(scaled);
		imlib_image_set_has_alpha(1);

		XImage img = {
		    dstw, dsth, 0, ZPixmap, (char *)imlib_image_get_data_for_reading_only(),
		    ImageByteOrder(drw->dpy), BitmapUnit(drw->dpy), BitmapBitOrder(drw->dpy), 32,
		    32, 0, 32,
		    0, 0, 0
		};
		XInitImage(&img);

		pm = XCreatePixmap(drw->dpy, drw->root, dstw, dsth, 32);
		gc = XCreateGC(drw->dpy, pm, 0, NULL);
		XPutImage(drw->dpy, pm, gc, &img, 0, 0, 0, 0, dstw, dsth);
		imlib_free_image_and_decache();
		XFreeGC(drw->dpy, gc);

		pic = XRenderCreatePicture(drw->dpy, pm, XRenderFindStandardFormat(drw->dpy, PictStandardARGB32), 0, NULL);
		XFreePixmap(drw->dpy, pm);
	}

	return pic;
}

void drw_rect(Drw *drw, int x, int y, unsigned int w, unsigned int h, int filled, int invert) {
	if (!drw || !drw->scheme)
		return;
	XSetForeground(drw->dpy, drw->gc, invert ? drw->scheme[ColBg].pixel : drw->scheme[ColFg].pixel);
	if (filled)
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w, h);
	else
		XDrawRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);
}

int drw_text(Drw *drw, int x, int y, unsigned int w, unsigned int h, unsigned int lpad, const char *text, int invert, Bool markup) {
	char buf[1024];
	int i, ty, th;
	unsigned int ew = 0, eh = 0;
	XftDraw *d = NULL;
	size_t len;
	int render = x || y || w || h;

	if (!drw || (render && !drw->scheme) || !text || !drw->font)
		return 0;

	if (!render) {
		w = invert ? invert : ~invert;
	} else {
		XSetForeground(drw->dpy, drw->gc, drw->scheme[invert ? ColFg : ColBg].pixel);
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w, h);
		d = XftDrawCreate(drw->dpy, drw->drawable, drw->visual, drw->cmap);
		x += lpad;
		w -= lpad;
	}

	len = strlen(text);

	if (len) {
		drw_font_getexts(drw->font, text, len, &ew, &eh, markup);
		th = eh;
		/* shorten text if necessary */
		for (len = MIN(len, sizeof(buf) - 1); len && ew > w; len--) {
			drw_font_getexts(drw->font, text, len, &ew, &eh, markup);
			if (eh > th)
				th = eh;
		}

		if (len) {
			memcpy(buf, text, len);
			buf[len] = '\0';
			if (len < strlen(text))
				for (i = len; i && i > len - 3; buf[--i] = '.')
					; /* NOP */

			if (render) {
				ty = y + (h - th) / 2;
				if(markup)
					pango_layout_set_markup(drw->font->layout, buf, len);
				else
					pango_layout_set_text(drw->font->layout, buf, len);
				pango_xft_render_layout(d, &drw->scheme[invert ? ColBg : ColFg],
					drw->font->layout, x * PANGO_SCALE, ty * PANGO_SCALE);
				if(markup) /* clear markup attributes */
					pango_layout_set_attributes(drw->font->layout, NULL);
			}
			x += ew;
			w -= ew;
		}
	}

	if (d)
		XftDrawDestroy(d);

	return x + (render ? w : 0);
}

void drw_pic(Drw *drw, int x, int y, unsigned int w, unsigned int h, Picture pic) {
	if (!drw)
		return;
	XRenderComposite(drw->dpy, PictOpOver, pic, None, drw->picture, 0, 0, 0, 0, x, y, w, h);
}

void drw_map(Drw *drw, Window win, int x, int y, unsigned int w, unsigned int h) {
	if (!drw)
		return;
	XCopyArea(drw->dpy, drw->drawable, win, drw->gc, x, y, w, h, x, y);
	XSync(drw->dpy, False);
}

unsigned int drw_font_getwidth(Drw *drw, const char *text, Bool markup) {
	if (!drw || !drw->font || !text)
		return 0;
	return drw_text(drw, 0, 0, 0, 0, 0, text, 0, markup);
}

unsigned int drw_font_getwidth_clamp(Drw *drw, const char *text, unsigned int n, Bool markup) {
	unsigned int tmp = 0;
	if (drw && drw->font && text && n)
		tmp = drw_text(drw, 0, 0, 0, 0, 0, text, n, markup);
	return MIN(n, tmp);
}

void drw_font_getexts(Fnt *font, const char *text, unsigned int len, unsigned int *w, unsigned int *h, Bool markup) {
	if (!font || !text)
		return;

	PangoRectangle r;
	if(markup)
		pango_layout_set_markup(font->layout, text, len);
	else
		pango_layout_set_text(font->layout, text, len);
	pango_layout_get_extents(font->layout, 0, &r);
	if(markup) /* clear markup attributes */
		pango_layout_set_attributes(font->layout, NULL);
	if (w)
		*w = r.width / PANGO_SCALE;
	if (h)
		*h = r.height / PANGO_SCALE;
}

Cur* drw_cur_create(Drw *drw, int shape) {
	Cur *cur;
	if (!drw || !(cur = ecalloc(1, sizeof(Cur))))
		return NULL;
	cur->cursor = XCreateFontCursor(drw->dpy, shape);
	return cur;
}

void drw_cur_free(Drw *drw, Cur *cursor) {
	if (!cursor)
		return;
	XFreeCursor(drw->dpy, cursor->cursor);
	free(cursor);
}
