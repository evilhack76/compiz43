/*
 *
 * Compiz application switcher plugin
 *
 * staticswitcher.c
 *
 * Copyright : (C) 2008 by Danny Baumann
 * E-mail    : maniac@compiz-fusion.org
 *
 * Based on switcher.c:
 * Copyright : (C) 2007 David Reveman
 * E-mail    : davidr@novell.com
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

#include "staticswitcher.h"

COMPIZ_PLUGIN_20090315 (staticswitcher, StaticSwitchPluginVTable)

void
StaticSwitchScreen::updatePopupWindow (int count)
{
    unsigned int winWidth, winHeight;
    unsigned int newXCount, newYCount;
    float        aspect;
    double       dCount = count;
    unsigned int w = PREVIEWSIZE, h = PREVIEWSIZE, b = BORDER;
    XSizeHints xsh;
    int x, y;

    /* maximum window size is 2/3 of the current output */
    winWidth  = ::screen->currentOutputDev ().width () * 2 / 3;
    winHeight = ::screen->currentOutputDev ().height () * 2 / 3;

    if (count <= 4)
    {
	/* don't put 4 or less windows in multiple rows */
	newXCount = count;
	newYCount = 1;
    }
    else
    {
	aspect = (float) winWidth / winHeight;
	/* round is available in C99 only, so use a replacement for that */
	newYCount = floor (sqrt (dCount / aspect) + 0.5f);
	newXCount = ceil (dCount / newYCount);
    }

    while ((w + b) * newXCount > winWidth ||
	   (h + b) * newYCount > winHeight)
    {
	/* shrink by 10% until all windows fit */
	w = w * 9 / 10;
	h = h * 9 / 10;
	b = b * 9 / 10;
    }

    winWidth = MIN (count, (int)newXCount);
    winHeight = (count + newXCount - 1) / newXCount;

    winWidth = winWidth * w + (winWidth + 1) * b;
    winHeight = winHeight * h + (winHeight + 1) * b;
    xCount = MIN ((int)newXCount, count);

    previewWidth = w;
    previewHeight = h;
    previewBorder = b;

    x = ::screen->currentOutputDev ().region ()->extents.x1 +
	::screen->currentOutputDev ().width () / 2;
    y = ::screen->currentOutputDev ().region ()->extents.y1 +
	::screen->currentOutputDev ().height () / 2;

    xsh.flags       = PSize | PPosition | PWinGravity;
    xsh.x           = x;
    xsh.y           = y;
    xsh.width       = winWidth;
    xsh.height      = winHeight;
    xsh.win_gravity = StaticGravity;

    XSetWMNormalHints (::screen->dpy (), popupWindow, &xsh);

    CompWindow *popup = screen->findWindow (popupWindow);

    if (popup)
	popup->resize (x - winWidth / 2, y - winHeight / 2,
		       winWidth, winHeight);
    else
	XMoveResizeWindow (::screen->dpy (), popupWindow,
			   x - winWidth / 2, y - winHeight / 2,
			   winWidth, winHeight);
}

void
StaticSwitchScreen::updateWindowList (int count)
{
    pos  = 0;
    move = 0;

    selectedWindow = windows.front ()->id ();

    if (popupWindow)
	updatePopupWindow (count);
}

void
StaticSwitchScreen::createWindowList (int count)
{
    windows.clear ();

    foreach (CompWindow *w, ::screen->windows ())
    {
	if (StaticSwitchWindow::get (w)->isSwitchWin ())
	{
	    SWITCH_WINDOW (w);
	    windows.push_back (w);

	    sw->cWindow->damageRectSetEnabled (sw, true);
	}
    }

    windows.sort (BaseSwitchScreen::compareWindows);

    updateWindowList (count);
}

bool
StaticSwitchWindow::damageRect (bool initial, const CompRect &rect)
{
    return BaseSwitchWindow::damageRect (initial, rect);
}

bool
StaticSwitchScreen::shouldShowIcon ()
{
    return optionGetIcon ();
}

bool
StaticSwitchScreen::getPaintRectangle (CompWindow *w,
				       CompRect   &rect,
				       int        *opacity)
{
    int mode;

    mode = optionGetHighlightRectHidden ();

    if (w->isViewable () || w->shaded ())
    {
    	rect = w->inputRect ();
	return true;
    }
    else if (mode == HighlightRectHiddenTaskbarEntry &&
    	     (w->iconGeometry ().x1 () != 0 ||
	      w->iconGeometry ().y1 () != 0 ||
	      w->iconGeometry ().x2 () != 0 ||
	      w->iconGeometry ().y2 () != 0))
    {
    	rect = w->iconGeometry ();
	return true;
    }
    else if (mode == HighlightRectHiddenOriginalWindowPosition)
    {
    	rect = w->serverInputRect ();

	if (opacity)
	    *opacity /= 4;

	return true;
    }

    return false;
}

void
StaticSwitchScreen::doWindowDamage (CompWindow *w)
{
    if (w->isViewable () || w->shaded ())
    {
    	CompositeWindow::get (w)->addDamage ();
    }
    else
    {
	CompRect box;
	if (getPaintRectangle (w, box, NULL))
	{
	    CompRect boxExtended (box.x () - 2,
	    			  box.y () - 2,
	    			  box.width () + 4,
	    			  box.height () + 4);

	    cScreen->damageRegion (CompRegion (boxExtended));
	}
    }
}

void
StaticSwitchScreen::handleSelectionChange (bool toNext, int nextIdx)
{
    move = nextIdx;
    moreAdjust = true;
}

int
StaticSwitchScreen::countWindows ()
{
    int count = 0;

    foreach (CompWindow *w, ::screen->windows ())
	if (StaticSwitchWindow::get (w)->isSwitchWin ())
	    count++;

    return count;
}

bool
StaticSwitchScreen::showPopup ()
{
    CompWindow *w = ::screen->findWindow (popupWindow);
    if (w && (w->state () & CompWindowStateHiddenMask))
    	w->show ();
    else
	XMapWindow (::screen->dpy (), popupWindow);

    cScreen->damageScreen ();

    popupDelayTimer.stop ();

    return false;
}

Cursor
StaticSwitchScreen::getCursor (bool mouseSelectOn)
{
    if (mouseSelectOn)
	return ::screen->normalCursor ();

    return ::screen->invisibleCursor ();
}

void
StaticSwitchScreen::initiate (SwitchWindowSelection selection,
			      bool                  shouldShowPopup)
{
    int  count;
    bool newMouseSelect;

    if (::screen->otherGrabExist ("switcher", "scale", "cube", 0))
	return;

    this->selection      = selection;
    selectedWindow = None;

    count = countWindows ();
    if (count < 1)
	return;

    if (!popupWindow && shouldShowPopup)
    {
	Display		     *dpy = ::screen->dpy ();
	XWMHints	     xwmh;
	XClassHint           xch;
	Atom		     state[4];
	int		     nState = 0;
	XSetWindowAttributes attr;
	Visual		     *visual;

	visual = findArgbVisual (dpy, ::screen->screenNum ());
	if (!visual)
	    return;

	xwmh.flags = InputHint;
	xwmh.input = 0;

	xch.res_name  = (char *)"compiz";
	xch.res_class = (char *)"switcher-window";

	attr.background_pixel = 0;
	attr.border_pixel     = 0;
	attr.colormap	      = XCreateColormap (dpy, ::screen->root (), visual,
						 AllocNone);

	popupWindow =
	    XCreateWindow (dpy, ::screen->root (),
	    		   -1, -1, 1, 1, 0,
			   32, InputOutput, visual,
			   CWBackPixel | CWBorderPixel | CWColormap, &attr);

	XSetWMProperties (dpy, popupWindow, NULL, NULL,
			  programArgv, programArgc,
			  NULL, &xwmh, &xch);

	state[nState++] = Atoms::winStateAbove;
	state[nState++] = Atoms::winStateSticky;
	state[nState++] = Atoms::winStateSkipTaskbar;
	state[nState++] = Atoms::winStateSkipPager;

	XChangeProperty (dpy, popupWindow,
			 Atoms::winState,
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *) state, nState);

	XChangeProperty (dpy, popupWindow,
			 Atoms::winType,
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *) &Atoms::winTypeUtil, 1);

	::screen->setWindowProp (popupWindow, Atoms::winDesktop, 0xffffffff);

	setSelectedWindowHint ();
    }

    newMouseSelect = optionGetMouseSelect () &&
    	             selection != Panels && shouldShowPopup;

    if (!grabIndex)
	grabIndex = ::screen->pushGrab (getCursor (newMouseSelect), "switcher");
    else if (newMouseSelect != mouseSelect)
	::screen->updateGrab (grabIndex, getCursor (newMouseSelect));

    mouseSelect = newMouseSelect;

    if (grabIndex)
    {
	if (!switching)
	{
	    lastActiveNum = ::screen->activeNum ();

	    createWindowList (count);

	    if (popupWindow && shouldShowPopup)
	    {
		unsigned int delay;

		delay = optionGetPopupDelay () * 1000;
		if (delay)
		{
		    if (popupDelayTimer.active ())
			popupDelayTimer.stop ();

		    popupDelayTimer.start
			(boost::bind (&StaticSwitchScreen::showPopup, this),
			 delay, delay * 1.2);
		}
		else
		{
		    showPopup ();
		}

		setSelectedWindowHint ();
	    }

	    activateEvent (true);
	}

	cScreen->damageScreen ();

	switching  = true;
	moreAdjust = true;

	::screen->handleEventSetEnabled (this, true);
	cScreen->preparePaintSetEnabled (this, true);
	cScreen->donePaintSetEnabled (this, true);
	gScreen->glPaintOutputSetEnabled (this, true);

	foreach (CompWindow *w, ::screen->windows ())
	{
	    SWITCH_WINDOW (w);

	    sw->gWindow->glPaintSetEnabled (sw, true);
	}
    }
}

static bool
switchTerminate (CompAction         *action,
	         CompAction::State  state,
	         CompOption::Vector &options)
{
    Window     xid;

    xid = CompOption::getIntOptionNamed (options, "root");

    if (action)
	action->setState (action->state () & ~(CompAction::StateTermKey |
					       CompAction::StateTermButton));

    if (xid && xid != ::screen->root ())
	return false;

    SWITCH_SCREEN (screen);

    if (ss->grabIndex)
    {
	CompWindow *w;

	if (ss->popupDelayTimer.active ())
	    ss->popupDelayTimer.stop ();

	if (ss->popupWindow)
	{
	    w = ::screen->findWindow (ss->popupWindow);
	    if (w && w->managed () && w->mapNum ())
	    {
		w->hide ();
	    }
	    else
	    {
		XUnmapWindow (::screen->dpy (), ss->popupWindow);
	    }
	}

	ss->switching = false;

	if (state & CompAction::StateCancel)
	    ss->selectedWindow = None;

	if (state && ss->selectedWindow)
	{
	    w = ::screen->findWindow (ss->selectedWindow);
	    if (w)
		::screen->sendWindowActivationRequest (w->id ());
	}

	::screen->removeGrab (ss->grabIndex, 0);
	ss->grabIndex = NULL;

	if (!ss->popupWindow)
	    ::screen->handleEventSetEnabled (ss, false);

	ss->selectedWindow = None;

	ss->activateEvent (false);
	ss->setSelectedWindowHint ();

	ss->lastActiveNum = 0;

	ss->cScreen->damageScreen ();
    }

    return false;
}

static bool
switchInitiateCommon (CompAction            *action,
		      CompAction::State     state,
		      CompOption::Vector    &options,
		      SwitchWindowSelection selection,
		      bool                  shouldShowPopup,
		      bool                  nextWindow)
{
    Window     xid;

    xid = CompOption::getIntOptionNamed (options, "root");

    if (xid != ::screen->root ())
	return false;

    SWITCH_SCREEN (::screen);

    if (!ss->switching)
    {
	if (selection == Group)
	{
	    CompWindow *w;
	    Window     xid;

	    xid = CompOption::getIntOptionNamed (options, "window");
	    w = ::screen->findWindow (xid);
	    if (w)
		ss->clientLeader = (w->clientLeader ()) ?
				   w->clientLeader () : xid;
	    else
		ss->clientLeader = None;
	}

	ss->initiate (selection, shouldShowPopup);

	if (state & CompAction::StateInitKey)
	    action->setState (action->state () | CompAction::StateTermKey);

	if (state & CompAction::StateInitEdge)
	    action->setState (action->state () | CompAction::StateTermEdge);
	else if (state & CompAction::StateInitButton)
	    action->setState (action->state () | CompAction::StateTermButton);
    }

    ss->switchToWindow (nextWindow, ss->optionGetAutoChangeVp ());

    return false;
}

void
StaticSwitchScreen::getMinimizedAndMatch (bool &minimizedOption,
					  CompMatch *&matchOption)
{
    minimizedOption = optionGetMinimized ();
    matchOption = &optionGetWindowMatch ();
}

void
StaticSwitchScreen::windowRemove (Window id)
{
    CompWindow *w;

    w = ::screen->findWindow (id);
    if (w)
    {
	bool   inList = false;
	int    count;
	Window selected, old;

	SWITCH_WINDOW (w);

	if (!sw->isSwitchWin ())
	    return;

	sw->cWindow->damageRectSetEnabled (sw, false);
	sw->gWindow->glPaintSetEnabled (sw, false);

	old = selected = selectedWindow;

	CompWindowList::iterator it = windows.begin ();
	while (it != windows.end ())
	{
	    if (*it == w)
	    {
		inList = true;

		if (w->id () == selected)
		{
		    it++;
		    if (it == windows.end ())
			selected = windows.front ()->id ();
		    else
			selected = (*it)->id ();
		    it--;
		}

		CompWindowList::iterator del = it;
		it++;
		windows.erase (del);
	    }
	    else
		it++;
	}

	if (!inList)
	    return;

	count = windows.size ();

	if (windows.size () == 0)
	{
	    CompOption::Vector o (0);
	    o.push_back (CompOption ("root", CompOption::TypeInt));
	    o[0].value ().set ((int) ::screen->root ());

	    switchTerminate (NULL, 0, o);
	    return;
	}

	if (!grabIndex)
	    return;

	updateWindowList (count);

	int i = 0;
	foreach (CompWindow *w, windows)
	{
	    selectedWindow = w->id ();
	    move = pos = i;

	    if (selectedWindow == selected)
		break;
	    i++;
	}

	if (popupWindow)
	{
	    CompWindow *popup;

	    popup = ::screen->findWindow (popupWindow);
	    if (popup)
		CompositeWindow::get (popup)->addDamage ();

	    setSelectedWindowHint ();
	}

	if (old != selectedWindow)
	{
	    doWindowDamage (w);

	    w = ::screen->findWindow (old);
	    if (w)
		doWindowDamage (w);

	    moreAdjust = true;
	}
    }
}

int
StaticSwitchScreen::getRowXOffset (int y)
{
    int retval = 0;

    if (windows.size () - (y * xCount) >= xCount)
	return 0;

    switch (optionGetRowAlign ()) {
    case RowAlignLeft:
	break;
    case RowAlignCentered:
	retval = (xCount - windows.size () + (y * xCount)) *
	         (previewWidth + previewBorder) / 2;
	break;
    case RowAlignRight:
	retval = (xCount - windows.size () + (y * xCount)) *
	         (previewWidth + previewBorder);
	break;
    }

    return retval;
}

void
StaticSwitchScreen::getWindowPosition (unsigned int index,
				       int          *x,
				       int          *y)
{
    int row, column;

    if (index >= windows.size ())
	return;

    column = index % xCount;
    row    = index / xCount;

    *x = column * previewWidth + (column + 1) * previewBorder;
    *x += getRowXOffset (row);

    *y = row * previewHeight + (row + 1) * previewBorder;
}

Window
StaticSwitchScreen::findWindowAt (int x,
				  int y)
{
    CompWindow *popup;

    popup = ::screen->findWindow (popupWindow);
    if (popup)
    {
	int i = 0;
	foreach (CompWindow *w, windows)
	{
	    int x1, x2, y1, y2;

	    getWindowPosition (i, &x1, &y1);

	    x1 += popup->geometry ().x ();
	    y1 += popup->geometry ().y ();

	    x2 = x1 + previewWidth;
	    y2 = y1 + previewHeight;

	    if (x >= x1 && x < x2 && y >= y1 && y < y2)
		return w->id ();

	    i++;
	}
    }

    return None;
}

void
StaticSwitchScreen::handleEvent (XEvent *event)
{
    BaseSwitchScreen::handleEvent (event);

    switch (event->type)
    {
    case ButtonPress:
	if (grabIndex && mouseSelect)
	{
	    Window selected;

	    selected = findWindowAt (event->xbutton.x_root,
	    			     event->xbutton.y_root);
	    if (selected)
	    {
	    	selectedWindow = selected;

	    	CompOption::Vector o (0);
	    	o.push_back (CompOption ("root", CompOption::TypeInt));
	    	o[0].value ().set ((int) ::screen->root ());

	    	switchTerminate (NULL, CompAction::StateTermButton, o);
	    }
	}
	break;
    default:
	break;
    }
}

bool
StaticSwitchScreen::adjustVelocity ()
{
    float dx, adjust, amount;

    dx = move - pos;
    if (abs (dx) > abs (dx + windows.size ()))
	dx += windows.size ();
    if (abs (dx) > abs (dx - windows.size ()))
	dx -= windows.size ();

    adjust = dx * 0.15f;
    amount = fabs (dx) * 1.5f;
    if (amount < 0.2f)
	amount = 0.2f;
    else if (amount > 2.0f)
	amount = 2.0f;

    mVelocity = (amount * mVelocity + adjust) / (amount + 1.0f);

    if (fabs (dx) < 0.001f && fabs (mVelocity) < 0.001f)
    {
	mVelocity = 0.0f;
	return false;
    }

    return true;
}

void
StaticSwitchScreen::preparePaint (int msSinceLastPaint)
{
    if (moreAdjust)
    {
	int   steps;
	float amount, chunk;

	amount = msSinceLastPaint * 0.05f * optionGetSpeed ();
	steps  = amount / (0.5f * optionGetTimestep ());
	if (!steps) steps = 1;
	chunk  = amount / (float) steps;

	while (steps--)
	{
	    moreAdjust = adjustVelocity ();
	    if (!moreAdjust)
	    {
		pos = move;
		break;
	    }

	    pos += mVelocity * chunk;
	    pos = fmod (pos, windows.size ());
	    if (pos < 0.0)
		pos += windows.size ();
	}
    }

    cScreen->preparePaint (msSinceLastPaint);
}

void
StaticSwitchScreen::paintRect (CompRect &box,
			       unsigned int offset,
			       unsigned short *color,
			       int opacity)
{
    glColor4us (color[0], color[1], color[2], color[3] * opacity / 100);
    glBegin (GL_LINE_LOOP);
    glVertex2i (box.x1 () + offset, box.y1 () + offset);
    glVertex2i (box.x2 () - offset, box.y1 () + offset);
    glVertex2i (box.x2 () - offset, box.y2 () - offset);
    glVertex2i (box.x1 () + offset, box.y2 () - offset);
    glEnd ();
}

bool
StaticSwitchScreen::glPaintOutput (const GLScreenPaintAttrib &sAttrib,
				   const GLMatrix            &transform,
				   const CompRegion          &region,
				   CompOutput                *output,
				   unsigned int              mask)
{
    bool status;

    if (grabIndex)
    {
	int        mode;
	CompWindow *switcher, *zoomed;
	Window	   zoomedAbove = None;

	if (!popupDelayTimer.active ())
	    mode = optionGetHighlightMode ();
	else
	    mode = HighlightModeNone;

	if (mode == HighlightModeBringSelectedToFront)
	{
	    zoomed = ::screen->findWindow (selectedWindow);
	    if (zoomed)
	    {
		CompWindow *w;

		for (w = zoomed->prev; w && w->id () <= 1; w = w->prev)
		    ;
		zoomedAbove = (w) ? w->id () : None;

		::screen->unhookWindow (zoomed);
		::screen->insertWindow (zoomed,
					::screen->windows ().back ()->id ());
	    }
	}
	else
	{
	    zoomed = NULL;
	}

	ignoreSwitcher = true;

	status = gScreen->glPaintOutput (sAttrib, transform, region, output,
					 mask);

	if (zoomed)
	{
	    ::screen->unhookWindow (zoomed);
	    ::screen->insertWindow (zoomed, zoomedAbove);
	}

	ignoreSwitcher = false;

	switcher = ::screen->findWindow (popupWindow);

	if (switcher || mode == HighlightModeShowRectangle)
	{
	    GLMatrix   sTransform (transform);

	    sTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);

	    glPushMatrix ();
	    glLoadMatrixf (sTransform.getMatrix ());

	    if (mode == HighlightModeShowRectangle)
	    {
		CompWindow *w;

		if (zoomed)
		    w = zoomed;
		else
		    w = ::screen->findWindow (selectedWindow);

		if (w)
		{
		    CompRect box;
		    int      opacity = 100;

		    if (getPaintRectangle (w, box, &opacity))
		    {
			unsigned short *color;
			GLushort       r, g, b, a;

			glEnable (GL_BLEND);

			/* fill rectangle */
			r = optionGetHighlightColorRed ();
			g = optionGetHighlightColorGreen ();
			b = optionGetHighlightColorBlue ();
			a = optionGetHighlightColorAlpha ();
			a = a * opacity / 100;

			glColor4us (r, g, b, a);
			glRecti (box.x1 (), box.y2 (), box.x2 (), box.y1 ());

			/* draw outline */
			glLineWidth (1.0);
			glDisable (GL_LINE_SMOOTH);

			color = optionGetHighlightBorderColor ();
			paintRect (box, 0, color, opacity);
			paintRect (box, 2, color, opacity);
			color = optionGetHighlightBorderInlayColor ();
			paintRect (box, 1, color, opacity);

			/* clean up */
			glColor4usv (defaultColor);
			glDisable (GL_BLEND);
		    }
		}
	    }

	    if (switcher)
	    {
	    	SWITCH_WINDOW (switcher);

		if (!switcher->destroyed () &&
		    switcher->isViewable () &&
		    sw->cWindow->damaged ())
		{
		    sw->gWindow->glPaint (sw->gWindow->paintAttrib (),
					  sTransform, infiniteRegion, 0);
		}
	    }

	    glPopMatrix ();
	}
    }
    else
    {
	status = gScreen->glPaintOutput (sAttrib, transform, region, output,
					 mask);
    }

    return status;
}

void
StaticSwitchScreen::donePaint ()
{
    if (grabIndex && moreAdjust)
    {
	CompWindow *w;

	w = ::screen->findWindow (popupWindow);
	if (w)
	    CompositeWindow::get (w)->addDamage ();
    }
    else if (!grabIndex && !moreAdjust)
    {
	cScreen->preparePaintSetEnabled (this, false);
	cScreen->donePaintSetEnabled (this, false);
	gScreen->glPaintOutputSetEnabled (this, false);

	foreach (CompWindow *w, ::screen->windows ())
	{
	    SWITCH_WINDOW (w);
	    sw->cWindow->damageRectSetEnabled (sw, false);
	    sw->gWindow->glPaintSetEnabled (sw, false);
	}
    }

    cScreen->donePaint ();
}

void
StaticSwitchScreen::paintSelectionRect (int          x,
					int          y,
					float        dx,
					float        dy,
					unsigned int opacity)
{
    int            i;
    float          color[4], op;
    unsigned int   w, h;

    w = previewWidth + previewBorder;
    h = previewHeight + previewBorder;

    glEnable (GL_BLEND);

    if (dx > xCount - 1)
	op = 1.0 - MIN (1.0, dx - (xCount - 1));
    else if (dx + (dy * xCount) > windows.size () - 1)
	op = 1.0 - MIN (1.0, dx - (windows.size () - 1 - (dy * xCount)));
    else if (dx < 0.0)
	op = 1.0 + MAX (-1.0, dx);
    else
	op = 1.0;

    for (i = 0; i < 4; i++)
	color[i] = (float)fgColor[i] * opacity * op / 0xffffffff;

    glColor4fv (color);
    glPushMatrix ();
    glTranslatef (x + previewBorder / 2 + (dx * w),
		  y + previewBorder / 2 + (dy * h), 0.0f);

    glBegin (GL_QUADS);
    glVertex2i (-1, -1);
    glVertex2i (-1, 1);
    glVertex2i (w + 1, 1);
    glVertex2i (w + 1, -1);
    glVertex2i (-1, h - 1);
    glVertex2i (-1, h + 1);
    glVertex2i (w + 1, h + 1);
    glVertex2i (w + 1, h - 1);
    glVertex2i (-1, 1);
    glVertex2i (-1, h - 1);
    glVertex2i (1, h - 1);
    glVertex2i (1, 1);
    glVertex2i (w - 1, 1);
    glVertex2i (w - 1, h - 1);
    glVertex2i (w + 1, h - 1);
    glVertex2i (w + 1, 1);
    glEnd ();

    glPopMatrix ();
    glColor4usv (defaultColor);
    glDisable (GL_BLEND);
}

bool
StaticSwitchWindow::isSwitchWin ()
{
    bool baseIsSwitchWin = BaseSwitchWindow::isSwitchWin ();

    if (baseIsSwitchWin && sScreen->selection == Group)
    {
	if (sScreen->clientLeader != window->clientLeader () &&
	    sScreen->clientLeader != window->id ())
	    return false;
    }

    return baseIsSwitchWin;
}

void
StaticSwitchWindow::updateIconTexturedWindow (GLWindowPaintAttrib  &sAttrib,
					      int                  &wx,
					      int                  &wy,
					      int                  x,
					      int                  y,
					      GLTexture            *icon)
{
    float xScale, yScale;

    xScale = (icon->width () > ICON_SIZE) ?
    	(float) ICON_SIZE / icon->width () : 1.0;
    yScale = (icon->height () > ICON_SIZE) ?
    	(float) ICON_SIZE / icon->height () : 1.0;

    if (xScale < yScale)
	yScale = xScale;
    else
	xScale = yScale;

    sAttrib.xScale = (float) sScreen->previewWidth * xScale / PREVIEWSIZE;
    sAttrib.yScale = (float) sScreen->previewWidth * yScale / PREVIEWSIZE;

    wx = x + sScreen->previewWidth - (sAttrib.xScale * icon->width ());
    wy = y + sScreen->previewHeight - (sAttrib.yScale * icon->height ());
}

void
StaticSwitchWindow::updateIconNontexturedWindow (GLWindowPaintAttrib  &sAttrib,
						 int                  &wx,
						 int                  &wy,
						 float                &width,
						 float                &height,
						 int                  x,
						 int                  y,
						 GLTexture            *icon)
{
    float iw, ih;

    iw = width;
    ih = height;

    if (icon->width () < (iw / 2) || icon->width () > iw)
	sAttrib.xScale = (iw / icon->width ());
    else
	sAttrib.xScale = 1.0f;

    if (icon->height () < (ih / 2) || icon->height () > ih)
	sAttrib.yScale = (ih / icon->height ());
    else
	sAttrib.yScale = 1.0f;

    if (sAttrib.xScale < sAttrib.yScale)
	sAttrib.yScale = sAttrib.xScale;
    else
	sAttrib.xScale = sAttrib.yScale;

    width  = icon->width ()  * sAttrib.xScale;
    height = icon->height () * sAttrib.yScale;

    wx = x + (sScreen->previewWidth / 2) - (width / 2);
    wy = y + (sScreen->previewHeight / 2) - (height / 2);
}

void
StaticSwitchWindow::updateIconPos (int   &wx,
				   int   &wy,
				   int   x,
				   int   y,
				   float width,
				   float height)
{
    wx = x + (sScreen->previewWidth / 2) - (width / 2);
    wy = y + (sScreen->previewHeight / 2) - (height / 2);
}

void
StaticSwitchWindow::paintThumb (const GLWindowPaintAttrib &attrib,
			  const GLMatrix            &transform,
		          unsigned int              mask,
			  int                       x,
			  int                       y)
{
    BaseSwitchWindow::paintThumb (attrib,
    				  transform,
    				  mask,
    				  x,
    				  y,
				  sScreen->previewWidth,
				  sScreen->previewHeight,
				  sScreen->previewWidth * 3 / 4,
				  sScreen->previewHeight * 3 / 4);
}

bool
StaticSwitchWindow::glPaint (const GLWindowPaintAttrib &attrib,
			     const GLMatrix            &transform,
			     const CompRegion          &region,
			     unsigned int              mask)
{
    bool       status;

    if (window->id () == sScreen->popupWindow)
    {
	GLenum         filter;
	int            x, y, offX, i;
	float          px, py, pos;

	CompWindow::Geometry &g = window->geometry ();

	if (mask & PAINT_WINDOW_OCCLUSION_DETECTION_MASK ||
	    sScreen->ignoreSwitcher)
	    return false;

	status = gWindow->glPaint (attrib, transform, region, mask);

	if (!(mask & PAINT_WINDOW_TRANSFORMED_MASK) && region.isEmpty ())
	    return true;

	filter = gScreen->textureFilter ();

	if (sScreen->optionGetMipmap ())
	    gScreen->setTextureFilter (GL_LINEAR_MIPMAP_LINEAR);

	glPushAttrib (GL_SCISSOR_BIT);

	glEnable (GL_SCISSOR_TEST);
	glScissor (g.x (), 0, g.width (), ::screen->height ());

	i = 0;
	foreach (CompWindow *w, sScreen->windows)
	{
	    sScreen->getWindowPosition (i, &x, &y);
	    StaticSwitchWindow::get (w)->paintThumb (
	       gWindow->lastPaintAttrib (), transform,
	       mask, x + g.x (), y + g.y ());
	    i++;
	}

	gScreen->setTextureFilter (filter);

	pos = fmod (sScreen->pos, sScreen->windows.size ());
	px  = fmod (pos, sScreen->xCount);
	py  = floor (pos / sScreen->xCount);

	offX = sScreen->getRowXOffset (py);

	if (pos > sScreen->windows.size () - 1)
	{
	    px = fmod (pos - sScreen->windows.size (), sScreen->xCount);
	    sScreen->paintSelectionRect (g.x (), g.y (), px, 0.0,
	    				 gWindow->lastPaintAttrib ().opacity);

	    px = fmod (pos, sScreen->xCount);
	    sScreen->paintSelectionRect (g.x () + offX, g.y (),
	    				 px, py,
	    				 gWindow->lastPaintAttrib ().opacity);
	}
	if (px > sScreen->xCount - 1)
	{
	    sScreen->paintSelectionRect (g.x (), g.y (), px, py,
	    				 gWindow->lastPaintAttrib ().opacity);

	    py = fmod (py + 1, ceil ((double) sScreen->windows.size () /
	    			     sScreen->xCount));
	    offX = sScreen->getRowXOffset (py);

	    sScreen->paintSelectionRect (g.x () + offX, g.y (),
	    				 px - sScreen->xCount, py,
	    				 gWindow->lastPaintAttrib ().opacity);
	}
	else
	{
	    sScreen->paintSelectionRect (g.x () + offX, g.y (),
	    				 px, py,
	    				 gWindow->lastPaintAttrib ().opacity);
	}
	glDisable (GL_SCISSOR_TEST);
	glPopAttrib ();
    }
    else if (sScreen->switching && !sScreen->popupDelayTimer.active () &&
	     (window->id () != sScreen->selectedWindow))
    {
	GLWindowPaintAttrib sAttrib (attrib);
	GLuint              value;

	value = sScreen->optionGetSaturation ();
	if (value != 100)
	    sAttrib.saturation = sAttrib.saturation * value / 100;

	value = sScreen->optionGetBrightness ();
	if (value != 100)
	    sAttrib.brightness = sAttrib.brightness * value / 100;

	if (window->wmType () & ~(CompWindowTypeDockMask |
				  CompWindowTypeDesktopMask))
	{
	    value = sScreen->optionGetOpacity ();
	    if (value != 100)
		sAttrib.opacity = sAttrib.opacity * value / 100;
	}

	status = gWindow->glPaint (sAttrib, transform, region, mask);
    }
    else
    {
	status = gWindow->glPaint (attrib, transform, region, mask);
    }

    return status;
}

StaticSwitchScreen::StaticSwitchScreen (CompScreen *screen) :
    BaseSwitchScreen (screen),
    PluginClassHandler<StaticSwitchScreen,CompScreen> (screen),
    clientLeader (None),
    switching (false),
    mVelocity (0.0),
    pos (0),
    move (0),
    mouseSelect (false)
{
#define SWITCHBIND(a,b,c) boost::bind (switchInitiateCommon, _1, _2, _3, a, b, c)

    optionSetNextButtonInitiate (SWITCHBIND (CurrentViewport, true, true));
    optionSetNextButtonTerminate (switchTerminate);
    optionSetNextKeyInitiate (SWITCHBIND (CurrentViewport, true, true));
    optionSetNextKeyTerminate (switchTerminate);
    optionSetPrevButtonInitiate (SWITCHBIND (CurrentViewport, true, false));
    optionSetPrevButtonTerminate (switchTerminate);
    optionSetPrevKeyInitiate (SWITCHBIND (CurrentViewport, true, false));
    optionSetPrevKeyTerminate (switchTerminate);

    optionSetNextAllButtonInitiate (SWITCHBIND (AllViewports, true, true));
    optionSetNextAllButtonTerminate (switchTerminate);
    optionSetNextAllKeyInitiate (SWITCHBIND (AllViewports, true, true));
    optionSetNextAllKeyTerminate (switchTerminate);
    optionSetPrevAllButtonInitiate (SWITCHBIND (AllViewports, true, false));
    optionSetPrevAllButtonTerminate (switchTerminate);
    optionSetPrevAllKeyInitiate (SWITCHBIND (AllViewports, true, false));
    optionSetPrevAllKeyTerminate (switchTerminate);

    optionSetNextGroupButtonInitiate (SWITCHBIND (Group, true, true));
    optionSetNextGroupButtonTerminate (switchTerminate);
    optionSetNextGroupKeyInitiate (SWITCHBIND (Group, true, true));
    optionSetNextGroupKeyTerminate (switchTerminate);
    optionSetPrevGroupButtonInitiate (SWITCHBIND (Group, true, false));
    optionSetPrevGroupButtonTerminate (switchTerminate);
    optionSetPrevGroupKeyInitiate (SWITCHBIND (Group, true, false));
    optionSetPrevGroupKeyTerminate (switchTerminate);

    optionSetNextNoPopupButtonInitiate (SWITCHBIND (CurrentViewport, false, true));
    optionSetNextNoPopupButtonTerminate (switchTerminate);
    optionSetNextNoPopupKeyInitiate (SWITCHBIND (CurrentViewport, false, true));
    optionSetNextNoPopupKeyTerminate (switchTerminate);
    optionSetPrevNoPopupButtonInitiate (SWITCHBIND (CurrentViewport, false, false));
    optionSetPrevNoPopupButtonTerminate (switchTerminate);
    optionSetPrevNoPopupKeyInitiate (SWITCHBIND (CurrentViewport, false, false));
    optionSetPrevNoPopupKeyTerminate (switchTerminate);

    optionSetNextPanelButtonInitiate (SWITCHBIND (Panels, false, true));
    optionSetNextPanelButtonTerminate (switchTerminate);
    optionSetNextPanelKeyInitiate (SWITCHBIND (Panels, false, true));
    optionSetNextPanelKeyTerminate (switchTerminate);
    optionSetPrevPanelButtonInitiate (SWITCHBIND (Panels, false, false));
    optionSetPrevPanelButtonTerminate (switchTerminate);
    optionSetPrevPanelKeyInitiate (SWITCHBIND (Panels, false, false));
    optionSetPrevPanelKeyTerminate (switchTerminate);

#undef SWITCHBIND

    ScreenInterface::setHandler (screen, false);
    CompositeScreenInterface::setHandler (cScreen, false);
    GLScreenInterface::setHandler (gScreen, false);
}


StaticSwitchScreen::~StaticSwitchScreen ()
{
    if (popupDelayTimer.active ())
	popupDelayTimer.stop ();

    if (popupWindow)
	XDestroyWindow (::screen->dpy (), popupWindow);
}

StaticSwitchWindow::StaticSwitchWindow (CompWindow *window) :
    BaseSwitchWindow (dynamic_cast<BaseSwitchScreen *>
    		      (StaticSwitchScreen::get (screen)), window),
    PluginClassHandler<StaticSwitchWindow,CompWindow> (window),
    sScreen (StaticSwitchScreen::get (screen))
{
    GLWindowInterface::setHandler (gWindow, false);
    CompositeWindowInterface::setHandler (cWindow, false);

    if (sScreen->popupWindow && sScreen->popupWindow == window->id ())
	gWindow->glPaintSetEnabled (this, true);
}

bool
StaticSwitchPluginVTable::init ()
{
    if (!CompPlugin::checkPluginABI ("core", CORE_ABIVERSION) |
        !CompPlugin::checkPluginABI ("composite", COMPIZ_COMPOSITE_ABI) |
        !CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI))
	 return false;

    return true;
}

