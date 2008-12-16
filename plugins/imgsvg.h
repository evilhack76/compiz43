/*
 * Copyright © 2006 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#include <stdlib.h>
#include <string.h>

#include <cairo/cairo-xlib.h>
#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>

#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#include <core/core.h>
#include <core/privatehandler.h>
#include <composite/composite.h>
#include <opengl/opengl.h>
#include <decoration.h>

#include <iostream>
#include <fstream>

#define SVG_OPTION_SET 0
#define SVG_OPTION_NUM 1

#define SVG_SCREEN(s) SvgScreen *ss = SvgScreen::get (s)
#define SVG_WINDOW(w) SvgWindow *sw = SvgWindow::get (w)

class SvgScreen :
    public ScreenInterface,
    public PrivateHandler<SvgScreen, CompScreen>
{
    public:
	SvgScreen (CompScreen *screen);
	~SvgScreen ();

	CompOption::Vector & getOptions ();
	bool setOption (const char *name, CompOption::Value &value);

	bool fileToImage (CompString &path, CompSize &size,
			  int &stride, void *&data);
	void handleCompizEvent (const char *plugin, const char *event,
				CompOption::Vector &options);

	CompRect zoom;

    private:
	CompOption::Vector opt;

	bool readSvgToImage (const char *file, CompSize &size, void *& data);
};

class SvgWindow :
    public WindowInterface,
    public GLWindowInterface,
    public PrivateHandler<SvgWindow, CompWindow>
{
    public:
	SvgWindow (CompWindow *window);
	~SvgWindow ();

	bool glDraw (const GLMatrix &transform, GLFragment::Attrib &fragment,
		     const CompRegion &region, unsigned int mask);
	void moveNotify (int dx, int dy, bool immediate);
	void resizeNotify (int dx, int dy, int dwidth, int dheight);

	void setSvg (CompString &data, decor_point_t p[2]);

    private:
	typedef struct {
	    decor_point_t p1;
	    decor_point_t p2;

	    RsvgHandle	      *svg;
	    RsvgDimensionData dimension;
	} SvgSource;

	typedef struct {
	    GLTexture::List       textures;
	    GLTexture::MatrixList matrices;
	    cairo_t           *cr;
	    Pixmap            pixmap;
	    CompSize          size;
	} SvgTexture;

	typedef struct {
	    SvgSource  *source;
	    CompRegion box;
	    SvgTexture texture[2];
	    CompRect   rect;
	    CompSize   size;
	} SvgContext;

	SvgSource  *source;
	SvgContext *context;

	SvgScreen  *sScreen;
	GLScreen   *gScreen;

	CompWindow *window;
	GLWindow   *gWindow;

	void updateSvgMatrix ();
	void updateSvgContext ();

	void renderSvg (SvgSource *source, SvgTexture &texture, CompSize size,
			float x1, float y1, float x2, float y2);
	bool initTexture (SvgSource *source, SvgTexture &texture, CompSize size);
	void finiTexture (SvgTexture &texture);
};

class SvgPluginVTable :
    public CompPlugin::VTableForScreenAndWindow<SvgScreen, SvgWindow>
{
    public:

	bool init ();
	void fini ();

	PLUGIN_OPTION_HELPER (SvgScreen);
};
