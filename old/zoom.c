/*
 * Copyright © 2005 Novell, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <compiz.h>

#define ZOOM_POINTER_SENSITIVITY_FACTOR 0.001f

static CompMetadata zoomMetadata;

static int displayPrivateIndex;

#define ZOOM_DISPLAY_OPTION_INITIATE 0
#define ZOOM_DISPLAY_OPTION_IN	     1
#define ZOOM_DISPLAY_OPTION_OUT	     2
#define ZOOM_DISPLAY_OPTION_NUM	     3

typedef struct _ZoomDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompOption opt[ZOOM_DISPLAY_OPTION_NUM];
} ZoomDisplay;

#define ZOOM_SCREEN_OPTION_POINTER_INVERT_Y    0
#define ZOOM_SCREEN_OPTION_POINTER_SENSITIVITY 1
#define ZOOM_SCREEN_OPTION_SPEED	       2
#define ZOOM_SCREEN_OPTION_TIMESTEP	       3
#define ZOOM_SCREEN_OPTION_ZOOM_FACTOR         4
#define ZOOM_SCREEN_OPTION_FILTER_LINEAR       5
#define ZOOM_SCREEN_OPTION_NUM		       6

typedef struct _ZoomScreen {
    PreparePaintScreenProc	 preparePaintScreen;
    DonePaintScreenProc		 donePaintScreen;
    PaintScreenProc		 paintScreen;
    SetScreenOptionForPluginProc setScreenOptionForPlugin;

    CompOption opt[ZOOM_SCREEN_OPTION_NUM];

    float pointerSensitivity;

    int grabIndex;

    GLfloat currentZoom;
    GLfloat newZoom;

    GLfloat xVelocity;
    GLfloat yVelocity;
    GLfloat zVelocity;

    GLfloat xTranslate;
    GLfloat yTranslate;

    GLfloat xtrans;
    GLfloat ytrans;
    GLfloat ztrans;

    XPoint savedPointer;
    Bool   grabbed;

    float maxTranslate;

    int zoomOutput;
} ZoomScreen;

#define GET_ZOOM_DISPLAY(d)				      \
    ((ZoomDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define ZOOM_DISPLAY(d)		           \
    ZoomDisplay *zd = GET_ZOOM_DISPLAY (d)

#define GET_ZOOM_SCREEN(s, zd)				         \
    ((ZoomScreen *) (s)->privates[(zd)->screenPrivateIndex].ptr)

#define ZOOM_SCREEN(s)						        \
    ZoomScreen *zs = GET_ZOOM_SCREEN (s, GET_ZOOM_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static CompOption *
zoomGetScreenOptions (CompPlugin *plugin,
		      CompScreen *screen,
		      int	 *count)
{
    ZOOM_SCREEN (screen);

    *count = NUM_OPTIONS (zs);
    return zs->opt;
}

static Bool
zoomSetScreenOption (CompPlugin      *plugin,
		     CompScreen      *screen,
		     char	     *name,
		     CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    ZOOM_SCREEN (screen);

    o = compFindOption (zs->opt, NUM_OPTIONS (zs), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case ZOOM_SCREEN_OPTION_POINTER_SENSITIVITY:
	if (compSetFloatOption (o, value))
	{
	    zs->pointerSensitivity = o->value.f *
		ZOOM_POINTER_SENSITIVITY_FACTOR;
	    return TRUE;
	}
	break;
    default:
	return compSetScreenOption (screen, o, value);
    }

    return FALSE;
}

static int
adjustZoomVelocity (ZoomScreen *zs)
{
    float d, adjust, amount;

    d = (zs->newZoom - zs->currentZoom) * 75.0f;

    adjust = d * 0.002f;
    amount = fabs (d);
    if (amount < 1.0f)
	amount = 1.0f;
    else if (amount > 5.0f)
	amount = 5.0f;

    zs->zVelocity = (amount * zs->zVelocity + adjust) / (amount + 1.0f);

    return (fabs (d) < 0.1f && fabs (zs->zVelocity) < 0.005f);
}

static void
zoomPreparePaintScreen (CompScreen *s,
			int	   msSinceLastPaint)
{
    ZOOM_SCREEN (s);

    if (zs->grabIndex)
    {
	int   steps;
	float amount, chunk;

	amount = msSinceLastPaint * 0.05f *
	    zs->opt[ZOOM_SCREEN_OPTION_SPEED].value.f;
	steps  = amount / (0.5f * zs->opt[ZOOM_SCREEN_OPTION_TIMESTEP].value.f);
	if (!steps) steps = 1;
	chunk  = amount / (float) steps;

	while (steps--)
	{
	    zs->xVelocity /= 1.25f;
	    zs->yVelocity /= 1.25f;

	    if (fabs (zs->xVelocity) < 0.001f)
		zs->xVelocity = 0.0f;
	    if (fabs (zs->yVelocity) < 0.001f)
		zs->yVelocity = 0.0f;

	    zs->xTranslate += zs->xVelocity * chunk;
	    if (zs->xTranslate < -zs->maxTranslate)
	    {
		zs->xTranslate = -zs->maxTranslate;
		zs->xVelocity  = 0.0f;
	    }
	    else if (zs->xTranslate > zs->maxTranslate)
	    {
		zs->xTranslate = zs->maxTranslate;
		zs->xVelocity  = 0.0f;
	    }

	    zs->yTranslate += zs->yVelocity * chunk;
	    if (zs->yTranslate < -zs->maxTranslate)
	    {
		zs->yTranslate = -zs->maxTranslate;
		zs->yVelocity  = 0.0f;
	    }
	    else if (zs->yTranslate > zs->maxTranslate)
	    {
		zs->yTranslate = zs->maxTranslate;
		zs->yVelocity  = 0.0f;
	    }

	    if (adjustZoomVelocity (zs))
	    {
		zs->currentZoom = zs->newZoom;
		zs->zVelocity = 0.0f;
	    }
	    else
	    {
		zs->currentZoom += (zs->zVelocity * msSinceLastPaint) /
		    s->redrawTime;
	    }

	    zs->ztrans = DEFAULT_Z_CAMERA * zs->currentZoom;
	    if (zs->ztrans <= 0.1f)
	    {
		zs->zVelocity = 0.0f;
		zs->ztrans = 0.1f;
	    }

	    zs->xtrans = -zs->xTranslate * (1.0f - zs->currentZoom);
	    zs->ytrans = zs->yTranslate * (1.0f - zs->currentZoom);

	    if (!zs->grabbed)
	    {
		if (zs->currentZoom == 1.0f && zs->zVelocity == 0.0f)
		{
		    zs->xVelocity = zs->yVelocity = 0.0f;

		    removeScreenGrab (s, zs->grabIndex, &zs->savedPointer);
		    zs->grabIndex = FALSE;
		    break;
		}
	    }
	}
    }

    UNWRAP (zs, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (zs, s, preparePaintScreen, zoomPreparePaintScreen);
}

static void
zoomDonePaintScreen (CompScreen *s)
{
    ZOOM_SCREEN (s);

    if (zs->grabIndex)
    {
	if (zs->currentZoom != zs->newZoom ||
	    zs->xVelocity || zs->yVelocity || zs->zVelocity)
	    damageScreen (s);
    }

    UNWRAP (zs, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (zs, s, donePaintScreen, zoomDonePaintScreen);
}

static Bool
zoomPaintScreen (CompScreen		 *s,
		 const ScreenPaintAttrib *sAttrib,
		 const CompTransform	 *transform,
		 Region		         region,
		 int			 output,
		 unsigned int		 mask)
{
    Bool status;

    ZOOM_SCREEN (s);

    if (zs->grabIndex)
    {
	mask &= ~PAINT_SCREEN_REGION_MASK;
	mask |= PAINT_SCREEN_CLEAR_MASK;
    }

    if (zs->grabIndex && zs->zoomOutput == output)
    {
	ScreenPaintAttrib sa = *sAttrib;
	int		  saveFilter;

	sa.xTranslate += zs->xtrans;
	sa.yTranslate += zs->ytrans;

	sa.zCamera = -zs->ztrans;

	/* hack to get sides rendered correctly */
	if (zs->xtrans > 0.0f)
	    sa.xRotate += 0.000001f;
	else
	    sa.xRotate -= 0.000001f;

	mask |= PAINT_SCREEN_TRANSFORMED_MASK;

	saveFilter = s->filter[SCREEN_TRANS_FILTER];

	if (zs->opt[ZOOM_SCREEN_OPTION_ZOOM_FACTOR].value.f == 2.0f &&
	    (zs->opt[ZOOM_SCREEN_OPTION_FILTER_LINEAR].value.b ||
	     zs->zVelocity != 0.0f))
	    s->filter[SCREEN_TRANS_FILTER] = COMP_TEXTURE_FILTER_GOOD;
	else
	    s->filter[SCREEN_TRANS_FILTER] = COMP_TEXTURE_FILTER_FAST;

	UNWRAP (zs, s, paintScreen);
	status = (*s->paintScreen) (s, &sa, transform, region, output, mask);
	WRAP (zs, s, paintScreen, zoomPaintScreen);

	s->filter[SCREEN_TRANS_FILTER] = saveFilter;
    }
    else
    {
	UNWRAP (zs, s, paintScreen);
	status = (*s->paintScreen) (s, sAttrib, transform, region, output,
				    mask);
	WRAP (zs, s, paintScreen, zoomPaintScreen);
    }

    return status;
}

static Bool
zoomIn (CompDisplay     *d,
	CompAction      *action,
	CompActionState state,
	CompOption      *option,
	int		nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	ZOOM_SCREEN (s);

	if (otherScreenGrabExist (s, "zoom", "scale", 0))
	    return FALSE;

	if (!zs->grabIndex)
	{
	    zs->grabIndex = pushScreenGrab (s, s->invisibleCursor, "zoom");

	    zs->savedPointer.x = pointerX;
	    zs->savedPointer.y = pointerY;

	    zs->zoomOutput = outputDeviceForPoint (s, pointerX, pointerY);
	}

	if (zs->grabIndex)
	{
	    float zoomFactor = zs->opt[ZOOM_SCREEN_OPTION_ZOOM_FACTOR].value.f;
	    int   x, y;

	    x = getIntOptionNamed (option, nOption, "x", 0);
	    y = getIntOptionNamed (option, nOption, "y", 0);

	    zs->grabbed = TRUE;

	    if (zs->newZoom / zoomFactor * DEFAULT_Z_CAMERA >= 0.1f)
	    {
		zs->newZoom /= zoomFactor;

		damageScreen (s);

		if (zs->currentZoom == 1.0f)
		{
		    CompOutput *o = &s->outputDev[zs->zoomOutput];

		    zs->xTranslate =
			((x - o->region.extents.x1) - o->width  / 2) /
			(s->width  * zoomFactor);
		    zs->yTranslate =
			((y - o->region.extents.y1) - o->height / 2) /
			(s->height * zoomFactor);

		    zs->xTranslate /= zs->newZoom;
		    zs->yTranslate /= zs->newZoom;
		}
	    }
	}
    }

    return FALSE;
}

static Bool
zoomInitiate (CompDisplay     *d,
	      CompAction      *action,
	      CompActionState state,
	      CompOption      *option,
	      int	      nOption)
{
    zoomIn (d, action, state, option, nOption);

    if (state & CompActionStateInitKey)
	action->state |= CompActionStateTermKey;

    if (state & CompActionStateInitButton)
	action->state |= CompActionStateTermButton;

    return FALSE;
}

static Bool
zoomOut (CompDisplay     *d,
	 CompAction      *action,
	 CompActionState state,
	 CompOption      *option,
	 int	         nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	ZOOM_SCREEN (s);

	if (zs->grabIndex)
	{
	    zs->newZoom *= zs->opt[ZOOM_SCREEN_OPTION_ZOOM_FACTOR].value.f;
	    if (zs->newZoom > DEFAULT_Z_CAMERA - (DEFAULT_Z_CAMERA / 10.0f))
	    {
		zs->grabbed = FALSE;
		zs->newZoom = 1.0f;
	    }

	    damageScreen (s);
	}
    }

    return FALSE;
}

static Bool
zoomTerminate (CompDisplay     *d,
	       CompAction      *action,
	       CompActionState state,
	       CompOption      *option,
	       int	       nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    for (s = d->screens; s; s = s->next)
    {
	ZOOM_SCREEN (s);

	if (xid && s->root != xid)
	    continue;

	if (zs->grabIndex)
	{
	    zs->newZoom = 1.0f;
	    zs->grabbed = FALSE;

	    damageScreen (s);
	}
    }

    action->state &= ~(CompActionStateTermKey | CompActionStateTermButton);

    return FALSE;
}

static void
zoomHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    CompScreen *s;

    ZOOM_DISPLAY (d);

    switch (event->type) {
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	{
	    ZOOM_SCREEN (s);

	    if (zs->grabIndex && zs->grabbed)
	    {
		GLfloat pointerDx;
		GLfloat pointerDy;

		pointerDx = pointerX - lastPointerX;
		pointerDy = pointerY - lastPointerY;

		if (event->xmotion.x_root < 50	           ||
		    event->xmotion.y_root < 50	           ||
		    event->xmotion.x_root > s->width  - 50 ||
		    event->xmotion.y_root > s->height - 50)
		{
		    warpPointer (s,
				 (s->width  / 2) - pointerX,
				 (s->height / 2) - pointerY);
		}

		if (zs->opt[ZOOM_SCREEN_OPTION_POINTER_INVERT_Y].value.b)
		    pointerDy = -pointerDy;

		zs->xVelocity += pointerDx * zs->pointerSensitivity;
		zs->yVelocity += pointerDy * zs->pointerSensitivity;

		damageScreen (s);
	    }
	}
    default:
	break;
    }

    UNWRAP (zd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (zd, d, handleEvent, zoomHandleEvent);
}

static void
zoomUpdateCubeOptions (CompScreen *s)
{
    CompPlugin *p;

    ZOOM_SCREEN (s);

    p = findActivePlugin ("cube");
    if (p && p->vTable->getScreenOptions)
    {
	CompOption *options, *option;
	int	   nOptions;

	options = (*p->vTable->getScreenOptions) (p, s, &nOptions);
	option = compFindOption (options, nOptions, "in", 0);
	if (option)
	    zs->maxTranslate = option->value.b ? 0.85f : 1.5f;
    }
}

static Bool
zoomSetScreenOptionForPlugin (CompScreen      *s,
			      char	      *plugin,
			      char	      *name,
			      CompOptionValue *value)
{
    Bool status;

    ZOOM_SCREEN (s);

    UNWRAP (zs, s, setScreenOptionForPlugin);
    status = (*s->setScreenOptionForPlugin) (s, plugin, name, value);
    WRAP (zs, s, setScreenOptionForPlugin, zoomSetScreenOptionForPlugin);

    if (status && strcmp (plugin, "cube") == 0)
	zoomUpdateCubeOptions (s);

    return status;
}

static CompOption *
zoomGetDisplayOptions (CompPlugin  *plugin,
		       CompDisplay *display,
		       int	   *count)
{
    ZOOM_DISPLAY (display);

    *count = NUM_OPTIONS (zd);
    return zd->opt;
}

static Bool
zoomSetDisplayOption (CompPlugin      *plugin,
		      CompDisplay     *display,
		      char	      *name,
		      CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    ZOOM_DISPLAY (display);

    o = compFindOption (zd->opt, NUM_OPTIONS (zd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case ZOOM_DISPLAY_OPTION_OUT:
	if (compSetActionOption (o, value))
	    return TRUE;
	break;
    default:
	return compSetDisplayOption (display, o, value);
    }

    return FALSE;
}

static const CompMetadataOptionInfo zoomDisplayOptionInfo[] = {
    { "initiate", "action", 0, zoomInitiate, zoomTerminate },
    { "zoom_in", "action", 0, zoomIn, 0 },
    { "zoom_out", "action", 0, zoomOut, 0 }
};

static Bool
zoomInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    ZoomDisplay *zd;

    zd = malloc (sizeof (ZoomDisplay));
    if (!zd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &zoomMetadata,
					     zoomDisplayOptionInfo,
					     zd->opt,
					     ZOOM_DISPLAY_OPTION_NUM))
    {
	free (zd);
	return FALSE;
    }

    zd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (zd->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, zd->opt, ZOOM_DISPLAY_OPTION_NUM);
	free (zd);
	return FALSE;
    }

    WRAP (zd, d, handleEvent, zoomHandleEvent);

    d->privates[displayPrivateIndex].ptr = zd;

    return TRUE;
}

static void
zoomFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    ZOOM_DISPLAY (d);

    freeScreenPrivateIndex (d, zd->screenPrivateIndex);

    UNWRAP (zd, d, handleEvent);

    compFiniDisplayOptions (d, zd->opt, ZOOM_DISPLAY_OPTION_NUM);

    free (zd);
}

static const CompMetadataOptionInfo zoomScreenOptionInfo[] = {
    { "invert_y", "bool", 0, 0, 0 },
    { "sensitivity", "float", "<min>0.01</min>", 0, 0 },
    { "speed", "float", "<min>0.1</min>", 0, 0 },
    { "timestep", "float", "<min>0.1</min>", 0, 0 },
    { "zoom_factor", "float", "<min>1.01</min>", 0, 0 },
    { "filter_linear", "bool", 0, 0, 0 }
};

static Bool
zoomInitScreen (CompPlugin *p,
		CompScreen *s)
{
    ZoomScreen *zs;

    ZOOM_DISPLAY (s->display);

    zs = malloc (sizeof (ZoomScreen));
    if (!zs)
	return FALSE;

    if (!compInitScreenOptionsFromMetadata (s,
					    &zoomMetadata,
					    zoomScreenOptionInfo,
					    zs->opt,
					    ZOOM_SCREEN_OPTION_NUM))
    {
	free (zs);
	return FALSE;
    }

    zs->grabIndex = 0;

    zs->currentZoom = 1.0f;
    zs->newZoom = 1.0f;

    zs->xVelocity = 0.0f;
    zs->yVelocity = 0.0f;
    zs->zVelocity = 0.0f;

    zs->xTranslate = 0.0f;
    zs->yTranslate = 0.0f;

    zs->maxTranslate = 0.85f;

    zs->savedPointer.x = 0;
    zs->savedPointer.y = 0;

    zs->grabbed = FALSE;

    zs->zoomOutput = 0;

    zs->pointerSensitivity =
	zs->opt[ZOOM_SCREEN_OPTION_POINTER_SENSITIVITY].value.f *
	ZOOM_POINTER_SENSITIVITY_FACTOR;

    WRAP (zs, s, preparePaintScreen, zoomPreparePaintScreen);
    WRAP (zs, s, donePaintScreen, zoomDonePaintScreen);
    WRAP (zs, s, paintScreen, zoomPaintScreen);
    WRAP (zs, s, setScreenOptionForPlugin, zoomSetScreenOptionForPlugin);

    s->privates[zd->screenPrivateIndex].ptr = zs;

    zoomUpdateCubeOptions (s);

    return TRUE;
}

static void
zoomFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    ZOOM_SCREEN (s);

    UNWRAP (zs, s, preparePaintScreen);
    UNWRAP (zs, s, donePaintScreen);
    UNWRAP (zs, s, paintScreen);
    UNWRAP (zs, s, setScreenOptionForPlugin);

    compFiniScreenOptions (s, zs->opt, ZOOM_SCREEN_OPTION_NUM);

    free (zs);
}

static Bool
zoomInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&zoomMetadata,
					 p->vTable->name,
					 zoomDisplayOptionInfo,
					 ZOOM_DISPLAY_OPTION_NUM,
					 zoomScreenOptionInfo,
					 ZOOM_SCREEN_OPTION_NUM))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&zoomMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&zoomMetadata, p->vTable->name);

    return TRUE;
}

static void
zoomFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&zoomMetadata);
}

static int
zoomGetVersion (CompPlugin *plugin,
		int	   version)
{
    return ABIVERSION;
}

static CompMetadata *
zoomGetMetadata (CompPlugin *plugin)
{
    return &zoomMetadata;
}

CompPluginVTable zoomVTable = {
    "zoom",
    zoomGetVersion,
    zoomGetMetadata,
    zoomInit,
    zoomFini,
    zoomInitDisplay,
    zoomFiniDisplay,
    zoomInitScreen,
    zoomFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    zoomGetDisplayOptions,
    zoomSetDisplayOption,
    zoomGetScreenOptions,
    zoomSetScreenOption,
    0, /* Deps */
    0, /* nDeps */
    0, /* Features */
    0  /* nFeatures */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &zoomVTable;
}
