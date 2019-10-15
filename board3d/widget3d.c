/*
 * widget3d.c
 * by Jon Kinsey, 2003
 *
 * GtkGLExt widget for 3d drawing
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: widget3d.c,v 1.57 2019/10/15 20:13:18 Superfly_Jon Exp $
 */

#include "config.h"
#include "inc3d.h"
#include "tr.h"
#include "gtklocdefs.h"

gboolean gtk_gl_init_success = FALSE;

static void
configure_3dCB(GtkWidget * widget, BoardData *bd)
{
    if (display_is_3d(bd->rd)) {
        static int curHeight = -1, curWidth = -1;
        GtkAllocation allocation;
        int width, height;
        gtk_widget_get_allocation(widget, &allocation);
        width = allocation.width;
        height = allocation.height;
        if (width != curWidth || height != curHeight)
		{
            glViewport(0, 0, width, height);
            SetupViewingVolume3d(bd, bd->bd3d, bd->rd);

            RestrictiveRedraw();
            RerenderBase(bd->bd3d);

            curWidth = width;
            curHeight = height;
        }
    }
}

static void
realize_3dCB(BoardData* bd)
{
    InitGL(bd);
    GetTextures(bd->bd3d, bd->rd);
    preDraw3d(bd, bd->bd3d, bd->rd);
    /* Make sure viewing area is correct (in preview) */
    SetupViewingVolume3d(bd, bd->bd3d, bd->rd);
#ifdef WIN32
    if (fResetSync) {
        fResetSync = FALSE;
        (void) setVSync(fSync);
    }
#endif
}

void
MakeCurrent3d(const BoardData3d * bd3d)
{
    GLWidgetMakeCurrent(bd3d->drawing_area3d);
}

void
UpdateShadows(BoardData3d * bd3d)
{
    bd3d->shadowsOutofDate = TRUE;
}

static gboolean
expose_3dCB(GtkWidget* widget, GdkEventExpose* exposeEvent, const BoardData * bd)
{
    if (bd->bd3d->shadowsOutofDate) {   /* Update shadow positions */
        bd->bd3d->shadowsOutofDate = FALSE;
        updateOccPos(bd);
    }
#ifdef TEST_HARNESS
    TestHarnessDraw(bd);
	return TRUE;
#endif
	if (bd->rd->quickDraw) {    /* Quick drawing mode */
        if (numRestrictFrames >= 0) {
            if (numRestrictFrames == 0) {       /* Redraw obscured part of window - need to flip y co-ord */
                GtkAllocation allocation;
                gtk_widget_get_allocation(widget, &allocation);
                RestrictiveDrawFrameWindow(exposeEvent->area.x,
                                           allocation.height - (exposeEvent->area.y + exposeEvent->area.height),
                                           exposeEvent->area.width, exposeEvent->area.height);
            }

            /* Draw updated regions directly to screen */
            glDrawBuffer(GL_FRONT);
            RestrictiveRender(bd, bd->bd3d, bd->rd);
            glFlush();
			return FALSE;
		} else {                /* Full screen redraw (to back buffer and then swap) */
            glDrawBuffer(GL_BACK);
            numRestrictFrames = 0;
            drawBoard(bd, bd->bd3d, bd->rd);
			return TRUE;
		}
    }
	else
	{	// Normal drawing
		Draw3d(bd);
		return TRUE;
	}
}

extern int
CreateGLWidget(BoardData * bd)
{
    GtkWidget *p3dWidget;
    bd->bd3d = (BoardData3d *) malloc(sizeof(BoardData3d));
    InitBoard3d(bd, bd->bd3d);
    /* Drawing area for OpenGL */
	p3dWidget = bd->bd3d->drawing_area3d = GLWidgetCreate(realize_3dCB, configure_3dCB, expose_3dCB, bd);
    if (p3dWidget == NULL)
        return FALSE;

    /* set up events and signals for OpenGL widget */
    gtk_widget_set_events(p3dWidget, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK);
    g_signal_connect(G_OBJECT(p3dWidget), "button_press_event", G_CALLBACK(board_button_press), bd);
    g_signal_connect(G_OBJECT(p3dWidget), "button_release_event", G_CALLBACK(board_button_release), bd);
    g_signal_connect(G_OBJECT(p3dWidget), "motion_notify_event", G_CALLBACK(board_motion_notify), bd);

    return TRUE;
}

void
InitGTK3d(int *argc, char ***argv)
{
	gtk_gl_init_success = GLInit(argc, argv);
    /* Call LoadTextureInfo to get texture details from textures.txt */
    LoadTextureInfo();
    SetupFlag();
}

gboolean
RenderToBuffer3d(GtkWidget* widget, GdkEventExpose* eventData, const RenderToBufferData* renderToBufferData)
{
    TRcontext *tr;
    GtkAllocation allocation;
	BoardData3d *bd3d = renderToBufferData->bd->bd3d;
	int fSaveBufs = bd3d->fBuffers;

    /* Sort out tile rendering stuff */
    tr = trNew();
#define BORDER 10
    gtk_widget_get_allocation(widget, &allocation);
    trTileSize(tr, allocation.width, allocation.height, BORDER);
    trImageSize(tr, renderToBufferData->width, renderToBufferData->height);
    trImageBuffer(tr, GL_RGB, GL_UNSIGNED_BYTE, renderToBufferData->puch);

    /* Sort out viewing perspective */
    glViewport(0, 0, (int)renderToBufferData->width, (int)renderToBufferData->height);
    SetupViewingVolume3d(renderToBufferData->bd, bd3d, renderToBufferData->bd->rd);

    if (renderToBufferData->bd->rd->planView)
        trOrtho(tr, -bd3d->horFrustrum, bd3d->horFrustrum, -bd3d->vertFrustrum, bd3d->vertFrustrum, 0.0, 5.0);
    else
        trFrustum(tr, -bd3d->horFrustrum, bd3d->horFrustrum, -bd3d->vertFrustrum, bd3d->vertFrustrum, zNear, zFar);

    bd3d->fBuffers = FALSE;     /* Turn this off whilst drawing */

    /* Draw tiles */
    do {
        trBeginTile(tr);
        Draw3d(renderToBufferData->bd);
    } while (trEndTile(tr));

    bd3d->fBuffers = fSaveBufs;

    trDelete(tr);

    /* Reset viewing volume for main screen */
    SetupViewingVolume3d(renderToBufferData->bd, bd3d, renderToBufferData->bd->rd);

	return FALSE;	// Don't swap buffers
}
