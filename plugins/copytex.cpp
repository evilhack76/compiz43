/*
 * Compiz copy to texture plugin
 *
 * Copyright : (C) 2008 by Dennis Kasprzyk
 * E-mail    : onestone@compiz-fusion.org
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * This plugin uses the "copy to texture" taken from Luminocity
 * http://live.gnome.org/Luminocity
 */

#include "copytex.h"
#include <math.h>

static CompMetadata *copytexMetadata;

static GLTexture::Matrix _identity_matrix = {
    1.0f, 0.0f,
    0.0f, 1.0f,
    0.0f, 0.0f
};

GLTexture::List
CopyPixmap::bindPixmapToTexture (Pixmap pixmap,
				 int width,
				 int height,
				 int depth)
{
    if (depth != 32 && depth != 24)
	return GLTexture::List ();

    CopyPixmap *cp = new CopyPixmap (pixmap, width, height, depth);
    if (!cp->textures.size ())
	delete cp;
    else
    {
	GLTexture::List tl (cp->textures.size ());
	for (unsigned int i = 0; i < cp->textures.size (); i++)
	    tl[i] = cp->textures[i];
        return tl;
    }
    return GLTexture::List ();
}

CopyPixmap::CopyPixmap (Pixmap pixmap,
			int width,
			int height,
			int depth) :
    pixmap (pixmap),
    damage (damage),
    depth (depth)
{
    int maxTS = MIN (MAX_SUB_TEX, GL::maxTextureSize);
    int nWidth = ceil ((float) width / (float) maxTS);
    int nHeight = ceil ((float) height / (float) maxTS);

    textures.resize (nWidth * nHeight);
    for (int x = 0, w = width; x < nWidth; x++, w -= maxTS)
	for (int y = 0, h = height; y < nHeight; y++, h -= maxTS)
	    textures[y + (x * nHeight)] =
		new CopyTexture (this, CompRect (x * maxTS,
				 (x * maxTS) + MIN (w, maxTS),
				 y * maxTS, (y * maxTS) +  MIN (h, maxTS)));


    damage = XDamageCreate (screen->dpy (), pixmap, XDamageReportRawRectangles);
    CopytexScreen::get (screen)->pixmaps[damage] = this;
}

CopyPixmap::~CopyPixmap ()
{
    if (damage)
	XDamageDestroy (screen->dpy (), damage);
    CopytexScreen::get (screen)->pixmaps.erase (damage);
}

CopyTexture::CopyTexture (CopyPixmap *cp, CompRect dim) :
    cp (cp),
    dim (dim),
    damage (0, dim.width(), 0, dim.height ())
{
    GLenum            target;
    GLTexture::Matrix matrix = _identity_matrix;

    if (GL::textureNonPowerOfTwo ||
	(POWER_OF_TWO (dim.width ()) && POWER_OF_TWO (dim.height ())))
    {
	target = GL_TEXTURE_2D;
	matrix.xx = 1.0f / dim.width ();
	matrix.yy = 1.0f / dim.height ();
	matrix.x0 = -dim.x () / dim.width ();
	matrix.y0 = -dim.y () / dim.height ();
    }
    else
    {
	target = GL_TEXTURE_RECTANGLE_NV;
	matrix.xx = 1.0f;
	matrix.yy = 1.0f;
	matrix.x0 = -dim.x ();
	matrix.y0 = -dim.y ();
    }

    setData (target, matrix, false);
    setSize (dim);

    glBindTexture (target, name ());

    if (cp->depth == 32)
	glTexImage2D (target, 0, GL_RGBA, dim.width (), dim.height (), 0,
		      GL_BGRA, GL_UNSIGNED_BYTE, 0);
    else
	glTexImage2D (target, 0, GL_RGB, dim.width (), dim.height (), 0,
		      GL_BGRA, GL_UNSIGNED_BYTE, 0);

    setFilter (GL_NEAREST);
    setWrap (GL_CLAMP_TO_EDGE);
}

CopyTexture::~CopyTexture ()
{
    CopyPixmap::Textures::iterator it = std::find (cp->textures.begin (),
					           cp->textures.end (), this);
    if (it != cp->textures.end ())
    {
	cp->textures.erase (it);
	if (cp->textures.empty ())
	    delete cp;
    }
}

void
CopyTexture::update ()
{
    COPY_SCREEN (screen);

    char   *addr = 0;
    Pixmap tmpPix;
    XImage *image = 0;

    XGCValues gcv;
    GC        gc;

    if (!damage.width () || !damage.height ())
	return;

    gcv.graphics_exposures = FALSE;
    gcv.subwindow_mode = IncludeInferiors;
    gc = XCreateGC (screen->dpy (), cp->pixmap,
		    GCGraphicsExposures | GCSubwindowMode, &gcv);

    if (cs->useShm)
	tmpPix = XShmCreatePixmap (screen->dpy (), cp->pixmap,
				   cs->shmInfo.shmaddr, &cs->shmInfo,
				   damage.width (), damage.height (),
				   cp->depth);
    else
	tmpPix = XCreatePixmap (screen->dpy (), cp->pixmap, damage.width (),
				damage.height (), cp->depth);

    XCopyArea(screen->dpy (), cp->pixmap, tmpPix, gc, dim.x () + damage.x (),
	      dim.y () + damage.y (), damage.width (), damage.height (), 0, 0);
    XSync(screen->dpy (), FALSE);

    if (cs->useShm)
	addr = cs->shmInfo.shmaddr;
    else
    {
	image = XGetImage(screen->dpy (), tmpPix, 0, 0, damage.width(),
			  damage.height (), AllPlanes, ZPixmap);
	if (image)
	    addr = image->data;
    }

    glBindTexture (target (), name ());
    glTexSubImage2D(target (), 0, damage.x (), damage.y (), damage.width (),
		    damage.height (), GL_BGRA,
#if IMAGE_BYTE_ORDER == MSBFirst
		    GL_UNSIGNED_INT_8_8_8_8_REV,
#else
                    GL_UNSIGNED_BYTE,
#endif
		    addr);

    glBindTexture (target (), 0);
    XFreePixmap(screen->dpy (), tmpPix);
    XFreeGC(screen->dpy (), gc);
    if (image)
	XDestroyImage(image);

    damage.setGeometry (0, 0, 0, 0);
}

void
CopyTexture::enable (Filter filter)
{
    update ();
    GLTexture::enable (filter);
}

void
CopyTexture::disable ()
{
    GLTexture::disable ();
}

void
CopytexScreen::handleEvent (XEvent *event)
{
    screen->handleEvent (event);
    if (event->type == damageNotify)
    {
	XDamageNotifyEvent *de = (XDamageNotifyEvent *) event;

	std::map<Damage, CopyPixmap*>::iterator it =
	    pixmaps.find (de->damage);
	if (it != pixmaps.end ())
	{
	    CopyPixmap *cp = it->second;
	    int x1, x2, y1, y2;

	    foreach (CopyTexture *t, cp->textures)
	    {
		x1 = MAX (de->area.x, t->dim.x1 ()) - t->dim.x1 ();
		x2 = MIN (de->area.x + de->area.width, t->dim.x2 ()) -
		     t->dim.x1 ();
		y1 = MAX (de->area.y, t->dim.y1 ()) - t->dim.y1 ();
		y2 = MIN (de->area.y + de->area.height, t->dim.y2 ()) -
		     t->dim.y1 ();

		if (t->damage.width () && t->damage.height ())
		{
		    x1 = MIN (x1, t->damage.x1 ());
		    x2 = MAX (x2, t->damage.x2 ());
		    y1 = MIN (y1, t->damage.y1 ());
		    y2 = MAX (y2, t->damage.y2 ());
		}
		
		if (x1 < x2 && y1 < y2)
		    t->damage.setGeometry (x1, x2, y1, y2);

	    }
	}
    }
}


CopytexScreen::CopytexScreen (CompScreen *screen) :
    PrivateHandler<CopytexScreen,CompScreen> (screen)
{
    useShm = false;
    if (XShmQueryExtension (screen->dpy ()))
    {
	int  i;
	Bool b;
	XShmQueryVersion (screen->dpy (), &i, &i, &b);
	if (b)
	    useShm = true;
    }
	
    if (useShm)
    {
	shmInfo.shmid = shmget (IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0600);
	if (shmInfo.shmid < 0)
	{
	    compLogMessage ("copytex", CompLogLevelError,
			    "Can't create shared memory\n");
	    useShm = false;
	}
    }

    if (useShm)
    {
	shmInfo.shmaddr = (char *) shmat (shmInfo.shmid, 0, 0);
	if (shmInfo.shmaddr == ((char *)-1))
	{
	    shmctl (shmInfo.shmid, IPC_RMID, 0);
	    compLogMessage ("copytex", CompLogLevelError,
			    "Can't attach shared memory\n");
	    useShm = false;
	}
    }

    if (useShm)
    {
	shmInfo.readOnly = FALSE;
	if (!XShmAttach(screen->dpy (), &shmInfo))
	{
	    shmdt (shmInfo.shmaddr);
	    shmctl (shmInfo.shmid, IPC_RMID, 0);
	    compLogMessage ("copytex", CompLogLevelError,
			    "Can't attach X shared memory\n");
	    useShm = false;
	}
    }

    damageNotify = CompositeScreen::get (screen)->damageEvent () +
	           XDamageNotify;

    ScreenInterface::setHandler (screen);

    hnd = GLScreen::get (screen)->
	registerBindPixmap (CopyPixmap::bindPixmapToTexture);
}

CopytexScreen::~CopytexScreen ()
{
    if (useShm)
    {
	XShmDetach(screen->dpy (), &shmInfo);
	shmdt (shmInfo.shmaddr);
	shmctl (shmInfo.shmid, IPC_RMID, 0); 
    }
    GLScreen::get (screen)->unregisterBindPixmap (hnd);
}

bool
CopytexPluginVTable::init ()
{
    if (!CompPlugin::checkPluginABI ("core", CORE_ABIVERSION) ||
        !CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI))
	 return false;

    copytexMetadata = new CompMetadata (name ());

    if (!copytexMetadata)
	return false;

    copytexMetadata->addFromFile (name ());

    return true;
}

void
CopytexPluginVTable::fini ()
{
    delete copytexMetadata;
}

CompMetadata *
CopytexPluginVTable::getMetadata ()
{
    return copytexMetadata;
}

CopytexPluginVTable copytexVTable;

CompPlugin::VTable *
getCompPluginInfo20080805 (void)
{
    return &copytexVTable;
}