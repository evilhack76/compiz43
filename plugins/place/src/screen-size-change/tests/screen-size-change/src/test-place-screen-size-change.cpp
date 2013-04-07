/*
 * Copyright © 2011 Canonical Ltd.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Canonical Ltd. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Canonical Ltd. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * CANONICAL, LTD. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL CANONICAL, LTD. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authored by: Sam Spilsbury <sam.spilsbury@canonical.com>
 */

#include <test-screen-size-change.h>
#include <screen-size-change.h>
#include <iostream>
#include <stdlib.h>
#include <cstring>

class CompPlaceScreenSizeChangeTestScreenSizeChange :
    public CompPlaceScreenSizeChangeTest
{
};

class MockScreenSizeChangeObject :
    public compiz::place::ScreenSizeChangeObject
{
    public:

	MockScreenSizeChangeObject (const compiz::window::Geometry &);
	~MockScreenSizeChangeObject ();

	const compiz::window::Geometry & getGeometry () const;
	void applyGeometry (compiz::window::Geometry &n,
			    compiz::window::Geometry &o);
	const CompPoint & getViewport () const;
	const CompRect &  getWorkarea (const compiz::window::Geometry &g) const;
	const compiz::window::extents::Extents & getExtents () const;

	void setVp (const CompPoint &);
	void setWorkArea (const CompRect &);
	void setExtents (unsigned int left,
			 unsigned int right,
			 unsigned int top,
			 unsigned int bottom);

	void setGeometry (const compiz::window::Geometry &g);
	compiz::window::Geometry sizeAdjustTest (const CompSize &oldSize,
						 const CompSize &newSize,
						 CompRect &workArea);

    private:

	CompPoint			 mCurrentVp;
	CompRect			 mCurrentWorkArea;
	compiz::window::extents::Extents mCurrentExtents;
	compiz::window::Geometry         mCurrentGeometry;
};

MockScreenSizeChangeObject::MockScreenSizeChangeObject (const compiz::window::Geometry &g) :
    ScreenSizeChangeObject (g),
    mCurrentVp (0, 0),
    mCurrentWorkArea (50, 50, 1000, 1000),
    mCurrentGeometry (g)
{
    memset (&mCurrentExtents, 0, sizeof (compiz::window::extents::Extents));
}

MockScreenSizeChangeObject::~MockScreenSizeChangeObject ()
{
}

const compiz::window::Geometry &
MockScreenSizeChangeObject::getGeometry () const
{
    return mCurrentGeometry;
}

void
MockScreenSizeChangeObject::applyGeometry (compiz::window::Geometry &n,
					   compiz::window::Geometry &o)
{
    EXPECT_EQ (mCurrentGeometry, o);

    std::cout << "DEBUG: new geometry : " << n.x () << " "
					  << n.y () << " "
					  << n.width () << " "
					  << n.height () << " "
					  << n.border () << std::endl;

    std::cout << "DEBUG: old geometry : " << o.x () << " "
					  << o.y () << " "
					  << o.width () << " "
					  << o.height () << " "
					  << o.border () << std::endl;

    mCurrentGeometry = n;
}

const CompPoint &
MockScreenSizeChangeObject::getViewport () const
{
    return mCurrentVp;
}

const CompRect &
MockScreenSizeChangeObject::getWorkarea (const compiz::window::Geometry &g) const
{
    return mCurrentWorkArea;
}

const compiz::window::extents::Extents &
MockScreenSizeChangeObject::getExtents () const
{
    return mCurrentExtents;
}

void
MockScreenSizeChangeObject::setVp (const CompPoint &p)
{
    mCurrentVp = p;
}

void
MockScreenSizeChangeObject::setWorkArea (const CompRect &wa)
{
    mCurrentWorkArea = wa;
}

void
MockScreenSizeChangeObject::setExtents (unsigned int left,
				        unsigned int right,
					unsigned int top,
					unsigned int bottom)
{
    mCurrentExtents.left = left;
    mCurrentExtents.right = right;
    mCurrentExtents.top = top;
    mCurrentExtents.bottom = bottom;
}

void
MockScreenSizeChangeObject::setGeometry (const compiz::window::Geometry &g)
{
    mCurrentGeometry = g;
}

void
reserveStruts (CompRect &workArea)
{
    workArea.setLeft (workArea.left () + 24);
    workArea.setTop (workArea.top () + 24);
    workArea.setBottom (workArea.bottom () - 24);
}

compiz::window::Geometry
MockScreenSizeChangeObject::sizeAdjustTest (const CompSize &oldSize,
					    const CompSize &newSize,
					    CompRect &workArea)
{
    /* Reserve top, bottom and left parts of the screen for
     * fake "24px" panels */
    reserveStruts (workArea);

    setWorkArea (workArea);

    compiz::window::Geometry g = adjustForSize (oldSize, newSize);

    return g;
}


TEST_F(CompPlaceScreenSizeChangeTestScreenSizeChange, TestScreenSizeChange)
{
    CompSize		     current, old;
    compiz::window::Geometry g (200, 250, 300, 400, 0);
    compiz::window::Geometry expected;

    MockScreenSizeChangeObject ms (g);

    current = CompSize (1280, 800);

    CompRect workArea;

    /* First test that changing the screen size
     * to something smaller here doesn't cause our
     * (small) window to be moved */

    old = current;
    current = CompSize (1024, 768);
    workArea = CompRect (0, 0, current.width (), current.height ());

    expected = compiz::window::Geometry (200, 250, 300, 400, 0);

    g = ms.sizeAdjustTest (old, current, workArea);

    EXPECT_EQ (expected, g);

    /* Making the screen size bigger with no
     * saved geometry should cause the window not to move */
    old = current;
    current = CompSize (2048, 768);
    workArea = CompRect (0, 0, current.width (), current.height ());

    g = ms.sizeAdjustTest (old, current, workArea);

    EXPECT_EQ (expected, g);

    /* Move the window to the other "monitor" */
    ms.setGeometry (compiz::window::Geometry (1025, 250, 300, 400, 0));

    /* Unplug a "monitor" */
    old = current;
    current = CompSize (1024, 768);
    workArea = CompRect (0, 0, current.width (), current.height ());

    expected = compiz::window::Geometry (724, 250, 300, 400, 0);

    g = ms.sizeAdjustTest (old, current, workArea);

    EXPECT_EQ (expected, g);

    /* Re-plug the monitor - window should go back
     * to the same position */
    old = current;
    current = CompSize (2048, 768);
    workArea = CompRect (0, 0, current.width (), current.height ());

    expected = compiz::window::Geometry (1025, 250, 300, 400, 0);

    g = ms.sizeAdjustTest (old, current, workArea);

    EXPECT_EQ (expected, g);

    /* Plug 2 monitors downwards, no change */
    old = current;
    current = CompSize (2048, 1536);
    workArea = CompRect (0, 0, current.width (), current.height ());

    g = ms.sizeAdjustTest (old, current, workArea);

    EXPECT_EQ (expected, g);

    /* Move the window to the bottom "monitor" */
    ms.setGeometry (compiz::window::Geometry (1025, 791, 300, 400, 0));

    /* Unplug bottom "monitor" */
    old = current;
    current = CompSize (2048, 768);
    workArea = CompRect (0, 0, current.width (), current.height ());

    expected = compiz::window::Geometry (1025, 344, 300, 400, 0);

    g = ms.sizeAdjustTest (old, current, workArea);

    EXPECT_EQ (expected, g);

    /* Re-plug bottom "monitor" */
    old = current;
    current = CompSize (2048, 1356);
    workArea = CompRect (0, 0, current.width (), current.height ());

    expected = compiz::window::Geometry (1025, 791, 300, 400, 0);

    g = ms.sizeAdjustTest (old, current, workArea);

    EXPECT_EQ (expected, g);

}

TEST_F(CompPlaceScreenSizeChangeTestScreenSizeChange, TestScreenChangeWindowsOnSecondViewport)
{
    CompSize		     current, old;
    compiz::window::Geometry g (1025, 791, 300, 400, 0);
    compiz::window::Geometry expected;

    MockScreenSizeChangeObject ms (g);

    current = CompSize (2048, 1356);

    CompRect workArea;

    /* Move the entire window right a viewport */
    g.setPos (g.pos () + CompPoint (current.width (), 0));
    ms.setGeometry (g);

    /* Now change the screen resolution again - the window should
     * move to be within the constrained size of its current
     * viewport */

    /* Unplug a "monitor" */
    old = current;
    current = CompSize (1024, 1356);
    workArea = CompRect (0, 0, current.width (), current.height ());

    expected = compiz::window::Geometry (current.width () + 724, 791, 300, 400, 0);

    g = ms.sizeAdjustTest (old, current, workArea);

    EXPECT_EQ (expected, g);

    /* Replug the monitor, make sure that the geometry is restored */
    old = current;
    current = CompSize (2048, 1356);
    workArea = CompRect (0, 0, current.width (), current.height ());

    expected = compiz::window::Geometry (current.width () + 1025, 791, 300, 400, 0);

    g = ms.sizeAdjustTest (old, current, workArea);

    EXPECT_EQ (expected, g);

    /* Replug the monitor and move the window to where it fits on the first
     * monitor on the second viewport, then make sure it doesn't move */
    old = CompSize (2048, 1356);
    current = CompSize (1024, 1356);
    workArea = CompRect (0, 0, current.width (), current.height ());

    g.setPos (CompPoint (old.width () + 25, 791));
    ms.setGeometry (g);
    /* clear the saved geometry */
    ms.unset();

    expected = compiz::window::Geometry (current.width () + 25, 791, 300, 400, 0);

    g = ms.sizeAdjustTest (old, current, workArea);

    EXPECT_EQ (expected, g);

    old = current;
    current = CompSize (2048, 1356);
    workArea = CompRect (0, 0, current.width (), current.height ());

    expected = compiz::window::Geometry (current.width () + 25, 791, 300, 400, 0);

    g = ms.sizeAdjustTest (old, current, workArea);

    EXPECT_EQ (expected, g);
}

TEST_F(CompPlaceScreenSizeChangeTestScreenSizeChange, TestScreenChangeWindowsOnPreviousViewport)
{
    CompSize		     current, old;
    compiz::window::Geometry g (0, 0, 300, 400, 0);
    compiz::window::Geometry expected;

    MockScreenSizeChangeObject ms (g);

    current = CompSize (2048, 1356);

    CompRect workArea;

    /* Deal with the case where the position is negative, which means
     * it's actually wrapped around to the rightmost viewport
     */
    g.setPos (CompPoint (-300, 200));
    ms.setGeometry (g);

    expected = compiz::window::Geometry (-300, 200, 300, 400, 0);

    /* Unplug the right "monitor" */
    old = current;
    current = CompSize (1024, 1356);
    workArea = CompRect (0, 0, current.width (), current.height ());

    g = ms.sizeAdjustTest (old, current, workArea);

    EXPECT_EQ (expected, g);

    /* Re-plug the right "monitor" */
    old = current;
    current = CompSize (2048, 1356);
    workArea = CompRect (0, 0, current.width (), current.height ());

    g = ms.sizeAdjustTest (old, current, workArea);

    EXPECT_EQ (expected, g);

    /* Move the window to the left monitor, verify that it survives an
     * unplug/plug cycle
     */
    g.setPos (CompPoint (-1324, 200));
    ms.setGeometry (g);

    old = current;
    current = CompSize (1024, 1356);
    workArea = CompRect (0, 0, current.width (), current.height ());

    expected = compiz::window::Geometry (-300, 200, 300, 400, 0);

    g = ms.sizeAdjustTest (old, current, workArea);

    EXPECT_EQ (expected, g);

    old = current;
    current = CompSize (2048, 1356);
    workArea = CompRect (0, 0, current.width (), current.height ());

    expected = compiz::window::Geometry (-1324, 200, 300, 400, 0);

    g = ms.sizeAdjustTest (old, current, workArea);

    EXPECT_EQ (expected, g);

}
