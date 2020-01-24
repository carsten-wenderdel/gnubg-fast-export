/*
 * Copyright (C) 2019 Jon Kinsey <jonkinsey@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * $Id: DrawOGL.c,v 1.10 2020/01/24 21:05:46 Superfly_Jon Exp $
 */

#include "config.h"
#include "legacyGLinc.h"
#include "fun3d.h"
#include "BoardDimensions.h"
#include "gtkboard.h"

extern void drawTableGrayed(const ModelManager* modelHolder, const BoardData3d* bd3d, renderdata tmp);
extern void WorkOutViewArea(const BoardData* bd, viewArea* pva, float* pHalfRadianFOV, float aspectRatio);
extern float getAreaRatio(const viewArea* pva);
extern float getViewAreaHeight(const viewArea* pva);
static void drawNumbers(const BoardData* bd, int MAA);
static void MAAtidyEdges(const renderdata* prd);
extern void drawDC(const ModelManager* modelHolder, const BoardData* bd, const BoardData3d* bd3d, const renderdata* prd);
extern void drawPieces(const ModelManager* modelHolder, const BoardData* bd, const BoardData3d* bd3d, const renderdata* prd);
extern void drawDie(const ModelManager* modelHolder, const BoardData* bd, const BoardData3d* bd3d, const renderdata* prd, int num);
static void drawSpecialPieces(const ModelManager* modelHolder, const BoardData* bd, const BoardData3d* bd3d, const renderdata* prd);
static void drawFlag(const ModelManager* modelHolder, const BoardData* bd, const BoardData3d* bd3d, const renderdata* prd);

///////////////////////////////////////
// Legacy Opengl board rendering code

#include "Shapes.inc"

void LegacyStartAA(float width)
{
	glLineWidth(.5f);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
}

void LegacyEndAA()
{
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
}

void RenderCharAA(unsigned int glyph)
{
	glPushMatrix();
		glLoadMatrixf(GetModelViewMatrix());
		glCallList(glyph);
	glPopMatrix();
}

void
drawBoard(const BoardData* bd, const BoardData3d* bd3d, const renderdata* prd)
{
	const ModelManager* modelHolder = &bd3d->modelHolder;

	if (!bd->grayBoard)
		drawTable(modelHolder, bd3d, prd);
	else
		drawTableGrayed(modelHolder, bd3d, *prd);

	if (prd->fLabels)
	{
		drawNumbers(bd, 0);

#ifndef USE_GTK3
		LegacyStartAA(0.5f);
		drawNumbers(bd, 1);
		LegacyEndAA();
#endif
	}

#ifndef USE_GTK3
	MAAtidyEdges(prd);
#endif

	if (bd->cube_use && !bd->crawford_game)
		drawDC(modelHolder, bd, bd3d, prd);

	if (prd->showMoveIndicator)
	{
		glDisable(GL_DEPTH_TEST);
		ShowMoveIndicator(modelHolder, bd);
		glEnable(GL_DEPTH_TEST);
	}

	/* Draw things in correct order so transparency works correctly */
	/* First pieces, then dice, then moving pieces */

	int transparentPieces = (prd->ChequerMat[0].alphaBlend) || (prd->ChequerMat[1].alphaBlend);
	if (transparentPieces) {	/* Draw back of piece separately */
		glCullFace(GL_FRONT);
		glEnable(GL_BLEND);
		drawPieces(modelHolder, bd, bd3d, prd);
		glCullFace(GL_BACK);
	}
	drawPieces(modelHolder, bd, bd3d, prd);
	if (transparentPieces)
		glDisable(GL_BLEND);

	if (bd->DragTargetHelp) {   /* highlight target points */
		glPolygonMode(GL_FRONT, GL_LINE);
		SetColour3d(0.f, 1.f, 0.f, 0.f);        /* Nice bright green... */

		for (int i = 0; i <= 3; i++) {
			int target = bd->iTargetHelpPoints[i];
			if (target != -1) { /* Make sure texturing is disabled */
				int separateTop = (prd->ChequerMat[0].pTexture && prd->pieceTextureType == PTT_TOP);
#ifndef USE_GTK3
				if (prd->ChequerMat[0].pTexture)
					glDisable(GL_TEXTURE_2D);
#endif
				drawPiece(modelHolder, bd3d, (unsigned int)target, Abs(bd->points[target]) + 1, TRUE, (prd->pieceType == PT_ROUNDED), prd->curveAccuracy, separateTop);
			}
		}
		glPolygonMode(GL_FRONT, GL_FILL);
	}

	if (DiceShowing(bd)) {
		drawDie(modelHolder, bd, bd3d, prd, 0);
		drawDie(modelHolder, bd, bd3d, prd, 1);
	}

	if (bd3d->moving || bd->drag_point >= 0)
		drawSpecialPieces(modelHolder, bd, bd3d, prd);

	if (bd->resigned)
		drawFlag(modelHolder, bd, bd3d, prd);
}

/* Define position of dots on dice */
static int dots1[] = { 2, 2, 0 };
static int dots2[] = { 1, 1, 3, 3, 0 };
static int dots3[] = { 1, 3, 2, 2, 3, 1, 0 };
static int dots4[] = { 1, 1, 1, 3, 3, 1, 3, 3, 0 };
static int dots5[] = { 1, 1, 1, 3, 2, 2, 3, 1, 3, 3, 0 };
static int dots6[] = { 1, 1, 1, 3, 2, 1, 2, 3, 3, 1, 3, 3, 0 };
static int* dots[6] = { dots1, dots2, dots3, dots4, dots5, dots6 };
static int dot_pos[] = { 0, 20, 50, 80 };       /* percentages across face */

static void
drawDots(const BoardData3d* bd3d, float diceSize, float dotOffset, const diceTest* dt, int showFront)
{
	int dot;
	int c;
	int* dp;
	float radius;
	float ds = (diceSize * 5.0f / 7.0f);
	float hds = (ds / 2);
	float x, y;
	float dotSize = diceSize / 10.0f;
	/* Remove specular effects */
	float zero[4] = { 0, 0, 0, 0 };
	glMaterialfv(GL_FRONT, GL_SPECULAR, zero);

	radius = diceSize / 7.0f;

	glPushMatrix();
	for (c = 0; c < 6; c++) {
		int nd;

		if (c < 3)
			dot = c;
		else
			dot = 8 - c;

		/* Make sure top dot looks nice */
		nd = !bd3d->shakingDice && (dot == dt->top);

		if (bd3d->shakingDice || (showFront && dot != dt->bottom && dot != dt->side[0])
			|| (!showFront && dot != dt->top && dot != dt->side[2])) {
			if (nd)
				glDisable(GL_DEPTH_TEST);
			glPushMatrix();
			glTranslatef(0.f, 0.f, hds + radius);

			glNormal3f(0.f, 0.f, 1.f);

			/* Show all the dots for this number */
			dp = dots[dot];
			do {
				x = (dot_pos[dp[0]] * ds) / 100;
				y = (dot_pos[dp[1]] * ds) / 100;

				glPushMatrix();
				glTranslatef(x - hds, y - hds, 0.f);

				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, bd3d->dotTexture);

				glBegin(GL_QUADS);
				glTexCoord2f(0.f, 1.f);
				glVertex3f(dotSize, dotSize, dotOffset);
				glTexCoord2f(1.f, 1.f);
				glVertex3f(-dotSize, dotSize, dotOffset);
				glTexCoord2f(1.f, 0.f);
				glVertex3f(-dotSize, -dotSize, dotOffset);
				glTexCoord2f(0.f, 0.f);
				glVertex3f(dotSize, -dotSize, dotOffset);
				glEnd();

				glPopMatrix();

				dp += 2;
			} while (*dp);

			glPopMatrix();
			if (nd)
				glEnable(GL_DEPTH_TEST);
		}

		if (c % 2 == 0)
			glRotatef(-90.f, 0.f, 1.f, 0.f);
		else
			glRotatef(90.f, 1.f, 0.f, 0.f);
	}
	glPopMatrix();
}

extern void DrawNumbers(const OGLFont* numberFont, unsigned int sides, int swapNumbers, int MAA);

static void
drawNumbers(const BoardData* bd, int MAA)
{
	int swapNumbers = (bd->turn == -1 && bd->rd->fDynamicLabels);

	/* No need to depth test as on top of board (depth test could cause alias problems too) */
	glDisable(GL_DEPTH_TEST);
	/* Draw inside then anti-aliased outline of numbers */
	setMaterial(&bd->rd->PointNumberMat);

	DrawNumbers(&bd->bd3d->numberFont, 1, swapNumbers, MAA);
	DrawNumbers(&bd->bd3d->numberFont, 2, swapNumbers, MAA);

	glEnable(GL_DEPTH_TEST);
}

static void
MAAtidyEdges(const renderdata* prd)
{                               /* Anti-alias board edges */
	setMaterial(&prd->BoxMat);

	LegacyStartAA(1.0f);
	glDepthMask(GL_FALSE);

	glNormal3f(0.f, 0.f, 1.f);

	glBegin(GL_LINES);
	if (prd->roundedEdges) {
		/* bar */
		glNormal3f(-1.f, 0.f, 0.f);
		glVertex3f(TRAY_WIDTH + BOARD_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
		glVertex3f(TRAY_WIDTH + BOARD_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

		glNormal3f(1.f, 0.f, 0.f);
		glVertex3f(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
		glVertex3f(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT,
			BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

		/* left bear off tray */
		glNormal3f(-1.f, 0.f, 0.f);
		glVertex3f(0.f, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
		glVertex3f(0.f, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

		glVertex3f(TRAY_WIDTH - EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
		glVertex3f(TRAY_WIDTH - EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
		glVertex3f(TRAY_WIDTH - EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
		glVertex3f(TRAY_WIDTH - EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

		/* right bear off tray */
		glNormal3f(1.f, 0.f, 0.f);
		glVertex3f(TOTAL_WIDTH, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
		glVertex3f(TOTAL_WIDTH, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT,
			BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT,
			BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

	}
	else {
		/* bar */
		glVertex3f(TRAY_WIDTH + BOARD_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TRAY_WIDTH + BOARD_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		glVertex3f(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		/* left bear off tray */
		glVertex3f(0.f, 0.f, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(0.f, TOTAL_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		glVertex3f(EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		glVertex3f(TRAY_WIDTH - EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TRAY_WIDTH - EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TRAY_WIDTH - EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TRAY_WIDTH - EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		glVertex3f(TRAY_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TRAY_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		/* right bear off tray */
		glVertex3f(TOTAL_WIDTH, 0.f, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH, TOTAL_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		glVertex3f(TOTAL_WIDTH - EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
	}

	/* inner edges (sides) */
	glNormal3f(1.f, 0.f, 0.f);
	glVertex3f(EDGE_WIDTH + LIFT_OFF, EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
	glVertex3f(EDGE_WIDTH + LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
	glVertex3f(TRAY_WIDTH + LIFT_OFF, EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
	glVertex3f(TRAY_WIDTH + LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);

	glNormal3f(-1.f, 0.f, 0.f);
	glVertex3f(TOTAL_WIDTH - EDGE_WIDTH + LIFT_OFF, EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
	glVertex3f(TOTAL_WIDTH - EDGE_WIDTH + LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
	glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + LIFT_OFF, EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
	glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
	glEnd();
	glDepthMask(GL_TRUE);
	LegacyEndAA();
}

extern void
MAAmoveIndicator(void)
{
	glPushMatrix();
	glLoadMatrixf(GetModelViewMatrix());
	/* Outline arrow */
	SetColour3d(0.f, 0.f, 0.f, 1.f);    /* Black outline */

	LegacyStartAA(0.5f);

	glBegin(GL_LINE_LOOP);
	glVertex2f(-ARROW_UNIT * 2, -ARROW_UNIT);
	glVertex2f(-ARROW_UNIT * 2, ARROW_UNIT);
	glVertex2f(0.f, ARROW_UNIT);
	glVertex2f(0.f, ARROW_UNIT * 2);
	glVertex2f(ARROW_UNIT * 2, 0.f);
	glVertex2f(0.f, -ARROW_UNIT * 2);
	glVertex2f(0.f, -ARROW_UNIT);
	glEnd();

	LegacyEndAA();
	glPopMatrix();
}

extern void MAAdie(const renderdata* prd)
{
	/* Anti-alias dice edges */
	LegacyStartAA(1.0f);
	glDepthMask(GL_FALSE);

	float size = getDiceSize(prd);
	float radius = size / 2.0f;
	int c;

	glPushMatrix();
	glLoadMatrixf(GetModelViewMatrix());

	for (c = 0; c < 6; c++) {
		circleOutline(radius, radius + LIFT_OFF, prd->curveAccuracy);

		if (c % 2 == 0)
			glRotatef(-90.f, 0.f, 1.f, 0.f);
		else
			glRotatef(90.f, 1.f, 0.f, 0.f);
	}
	glPopMatrix();

	glDepthMask(GL_TRUE);
	LegacyEndAA();
}

void DrawBackDice(const ModelManager* modelHolder, const BoardData3d* bd3d, const renderdata* prd, diceTest* dt, int diceCol)
{
	glCullFace(GL_FRONT);
	glEnable(GL_BLEND);

	/* Draw dice */
	OglModelDraw(modelHolder, MT_DICE, &prd->DiceMat[diceCol]);

	/* Place back dots inside dice */
	setMaterial(&prd->DiceDotMat[diceCol]);
	glEnable(GL_BLEND);     /* NB. Disabled in diceList */
	glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
	drawDots(bd3d, getDiceSize(prd), -LIFT_OFF, dt, 0);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glCullFace(GL_BACK);
	// NB. GL_BLEND still enabled here (so front blends with back)
}

void DrawDots(const ModelManager* modelHolder, const BoardData3d* bd3d, const renderdata* prd, diceTest* dt, int diceCol)
{
	Material whiteMat;
	SetupSimpleMat(&whiteMat, 1.f, 1.f, 1.f);

	glPushMatrix();
	glLoadMatrixf(GetModelViewMatrix());

	/* Draw (front) dots */
	glEnable(GL_BLEND);
	/* First blank out space for dots */
	setMaterial(&whiteMat);
	glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	drawDots(bd3d, getDiceSize(prd), LIFT_OFF, dt, 1);

	/* Now fill space with coloured dots */
	setMaterial(&prd->DiceDotMat[diceCol]);
	glBlendFunc(GL_ONE, GL_ONE);
	drawDots(bd3d, getDiceSize(prd), LIFT_OFF, dt, 1);

	/* Restore blending defaults */
	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glPopMatrix();
}

void renderPiece(const ModelManager* modelHolder, int separateTop)
{
	const Material* mat = currentMat;
	if (separateTop)
	{
#ifndef USE_GTK3
		glEnable(GL_TEXTURE_2D);
#endif
		OglModelDraw(modelHolder, MT_PIECETOP, mat);
#ifndef USE_GTK3
		glDisable(GL_TEXTURE_2D);
#endif
		mat = NULL;
	}

	OglModelDraw(modelHolder, MT_PIECE, mat);
}

extern void
renderSpecialPieces(const ModelManager* modelHolder, const BoardData* bd, const BoardData3d* bd3d, const renderdata* prd);

static void
drawSpecialPieces(const ModelManager* modelHolder, const BoardData* bd, const BoardData3d* bd3d, const renderdata* prd)
{                               /* Draw animated or dragged pieces */
	int transparentPieces = (prd->ChequerMat[0].alphaBlend) || (prd->ChequerMat[1].alphaBlend);

	if (transparentPieces) {                /* Draw back of piece separately */
		glCullFace(GL_FRONT);
		glEnable(GL_BLEND);
		renderSpecialPieces(modelHolder, bd, bd3d, prd);
		glCullFace(GL_BACK);
		glEnable(GL_BLEND);
	}
	renderSpecialPieces(modelHolder, bd, bd3d, prd);

	if (transparentPieces)
		glDisable(GL_BLEND);
}

void MAApiece(int roundPiece, int curveAccuracy)
{
	/* Anti-alias piece edges */
	float lip = 0;
	float radius = PIECE_HOLE / 2.0f;
	float discradius = radius * 0.8f;

	if (roundPiece)
		lip = radius - discradius;

	GLboolean textureEnabled;
	glGetBooleanv(GL_TEXTURE_2D, &textureEnabled);

	GLboolean blendEnabled;
	glGetBooleanv(GL_BLEND, &blendEnabled);

	if (textureEnabled)
		glDisable(GL_TEXTURE_2D);

	LegacyStartAA(1.0f);
	glDepthMask(GL_FALSE);

	glPushMatrix();
	glLoadMatrixf(GetModelViewMatrix());
		circleOutlineOutward(radius, PIECE_DEPTH - lip, curveAccuracy);
		circleOutlineOutward(radius, lip, curveAccuracy);
	glPopMatrix();

	glDepthMask(GL_TRUE);
	LegacyEndAA();

	if (blendEnabled)
		glEnable(GL_BLEND);

	if (textureEnabled)
		glEnable(GL_TEXTURE_2D);
}

extern void
drawPieces(const ModelManager* modelHolder, const BoardData* bd, const BoardData3d* bd3d, const renderdata* prd)
{
	unsigned int i;
	unsigned int j;
	int separateTop = (prd->ChequerMat[0].pTexture && prd->pieceTextureType == PTT_TOP);

	setMaterial(&prd->ChequerMat[0]);
	for (i = 0; i < 28; i++) {
		if (bd->points[i] < 0) {
			unsigned int numPieces = (unsigned int)(-bd->points[i]);
			for (j = 1; j <= numPieces; j++) {
				drawPiece(modelHolder, bd3d, i, j, TRUE, (prd->pieceType == PT_ROUNDED), prd->curveAccuracy, separateTop);
			}
		}
	}
	setMaterial(&prd->ChequerMat[1]);
	for (i = 0; i < 28; i++) {
		if (bd->points[i] > 0) {
			unsigned int numPieces = (unsigned int)bd->points[i];
			for (j = 1; j <= numPieces; j++) {
				drawPiece(modelHolder, bd3d, i, j, TRUE, (prd->pieceType == PT_ROUNDED), prd->curveAccuracy, separateTop);
			}
		}
	}
}

extern void
drawPointLegacy(const renderdata* prd, float tuv, unsigned int i, int p, int outline)
{                               /* Draw point with correct texture co-ords */
	float w = PIECE_HOLE;
	float h = POINT_HEIGHT;
	float x, y;

	if (p) {
		x = TRAY_WIDTH - EDGE_WIDTH + PIECE_HOLE * i;
		y = -LIFT_OFF;
	}
	else {
		x = TRAY_WIDTH - EDGE_WIDTH + BOARD_WIDTH - (PIECE_HOLE * i);
		y = TOTAL_HEIGHT - EDGE_HEIGHT * 2 + LIFT_OFF;
		w = -w;
		h = -h;
	}

	glPushMatrix();
	if (prd->bgInTrays)
		glTranslatef(EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH);
	else {
		x -= TRAY_WIDTH - EDGE_WIDTH;
		glTranslatef(TRAY_WIDTH, EDGE_HEIGHT, BASE_DEPTH);
	}

	if (prd->roundedPoints) {   /* Draw rounded point ends */
		float xoff;

		w = w * TAKI_WIDTH;
		y += w / 2.0f;
		h -= w / 2.0f;

		if (p)
			xoff = x + (PIECE_HOLE / 2.0f);
		else
			xoff = x - (PIECE_HOLE / 2.0f);

		/* Draw rounded semi-circle end of point (with correct texture co-ords) */
		{
			unsigned int j;
			float angle, step;
			float radius = w / 2.0f;

			step = (2 * F_PI) / prd->curveAccuracy;
			angle = -step * (int)(prd->curveAccuracy / 4);
			glNormal3f(0.f, 0.f, 1.f);
			glBegin(outline ? GL_LINE_STRIP : GL_TRIANGLE_FAN);
			glTexCoord2f(xoff * tuv, y * tuv);

			glVertex3f(xoff, y, 0.f);
			for (j = 0; j <= prd->curveAccuracy / 2; j++) {
				glTexCoord2f((xoff + sinf(angle) * radius) * tuv, (y + cosf(angle) * radius) * tuv);
				glVertex3f(xoff + sinf(angle) * radius, y + cosf(angle) * radius, 0.f);
				angle -= step;
			}
			glEnd();
		}
		/* Move rest of point in slighlty */
		if (p)
			x -= -((PIECE_HOLE * (1 - TAKI_WIDTH)) / 2.0f);
		else
			x -= ((PIECE_HOLE * (1 - TAKI_WIDTH)) / 2.0f);
	}

	glBegin(outline ? GL_LINE_STRIP : GL_TRIANGLES);
	glNormal3f(0.f, 0.f, 1.f);
	glTexCoord2f((x + w) * tuv, y * tuv);
	glVertex3f(x + w, y, 0.f);
	glTexCoord2f((x + w / 2) * tuv, (y + h) * tuv);
	glVertex3f(x + w / 2, y + h, 0.f);
	glTexCoord2f(x * tuv, y * tuv);
	glVertex3f(x, y, 0.f);
	glEnd();

	glPopMatrix();
}

void MAApoints(const renderdata* prd)
{
	float tuv;

	setMaterial(&prd->PointMat[0]);

	if (prd->PointMat[0].pTexture)
		tuv = (TEXTURE_SCALE) / prd->PointMat[0].pTexture->width;
	else
		tuv = 0;

	LegacyStartAA(1.0f);
	LegacyEndAA();

	drawPointLegacy(prd, tuv, 0, 0, 1);
	drawPointLegacy(prd, tuv, 0, 1, 1);
	drawPointLegacy(prd, tuv, 2, 0, 1);
	drawPointLegacy(prd, tuv, 2, 1, 1);
	drawPointLegacy(prd, tuv, 4, 0, 1);
	drawPointLegacy(prd, tuv, 4, 1, 1);

	setMaterial(&prd->PointMat[1]);

	if (prd->PointMat[1].pTexture)
		tuv = (TEXTURE_SCALE) / prd->PointMat[1].pTexture->width;
	else
		tuv = 0;

	drawPointLegacy(prd, tuv, 1, 0, 1);
	drawPointLegacy(prd, tuv, 1, 1, 1);
	drawPointLegacy(prd, tuv, 3, 0, 1);
	drawPointLegacy(prd, tuv, 3, 1, 1);
	drawPointLegacy(prd, tuv, 5, 0, 1);
	drawPointLegacy(prd, tuv, 5, 1, 1);

	LegacyEndAA();
}

extern void drawTableBase(const ModelManager* modelHolder, const BoardData3d* bd3d, const renderdata* prd);
extern void drawHinges(const ModelManager* modelHolder, const BoardData3d* bd3d, const renderdata* prd);

void drawTable(const ModelManager* modelHolder, const BoardData3d* bd3d, const renderdata* prd)
{
	glClear(GL_DEPTH_BUFFER_BIT);
	glDepthFunc(GL_ALWAYS);

	drawTableBase(modelHolder, bd3d, prd);

#ifndef USE_GTK3
	MAApoints(prd);
	glPushMatrix();
	glTranslatef(TOTAL_WIDTH, TOTAL_HEIGHT, 0.f);
	glRotatef(180.f, 0.f, 0.f, 1.f);
	MAApoints(prd);
	glPopMatrix();
#endif

	glDepthFunc(GL_LEQUAL);

	OglModelDraw(modelHolder, MT_TABLE, &prd->BoxMat);

	if (prd->fHinges3d)
	{
		/* Shade in gap between boards */
		glDepthFunc(GL_ALWAYS);
		OglModelDraw(modelHolder, MT_HINGEGAP, &bd3d->gapColour);
		glDepthFunc(GL_LEQUAL);

		drawHinges(modelHolder, bd3d, prd);
	}
}

static void
getProjectedPos(int x, int y, float atDepth, float pos[3])
{                               /* Work out where point (x, y) projects to at depth atDepth */
	int viewport[4];
	GLdouble mvmatrix[16], projmatrix[16];
	double nearX, nearY, nearZ, farX, farY, farZ, zRange;

	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix);
	glGetDoublev(GL_PROJECTION_MATRIX, projmatrix);

	if ((gluUnProject((GLdouble)x, (GLdouble)y, 0.0, mvmatrix, projmatrix, viewport, &nearX, &nearY, &nearZ) ==
		GL_FALSE)
		|| (gluUnProject((GLdouble)x, (GLdouble)y, 1.0, mvmatrix, projmatrix, viewport, &farX, &farY, &farZ) ==
			GL_FALSE)) {
		/* Maybe a g_assert_not_reached() would be appropriate here
		 * Don't leave output parameters undefined anyway */
		pos[0] = pos[1] = pos[2] = 0.0f;
		g_print("gluUnProject failed!\n");
		return;
	}

	zRange = (fabs(nearZ) - atDepth) / (fabs(farZ) + fabs(nearZ));
	pos[0] = (float)(nearX - (-farX + nearX) * zRange);
	pos[1] = (float)(nearY - (-farY + nearY) * zRange);
	pos[2] = (float)(nearZ - (-farZ + nearZ) * zRange);
}

void
getProjectedPieceDragPos(int x, int y, float pos[3])
{
	getProjectedPos(x, y, BASE_DEPTH + EDGE_DEPTH + DOUBLECUBE_SIZE + LIFT_OFF * 3, pos);
}

void
calculateBackgroundSize(BoardData3d* bd3d, const int viewport[4])
{
	float pos1[3], pos2[3], pos3[3];

	getProjectedPos(0, viewport[1] + viewport[3], 0.f, pos1);
	getProjectedPos(viewport[2], viewport[1] + viewport[3], 0.f, pos2);
	getProjectedPos(0, viewport[1], 0.f, pos3);

	bd3d->backGroundPos[0] = pos1[0];
	bd3d->backGroundPos[1] = pos3[1];
	bd3d->backGroundSize[0] = pos2[0] - pos1[0];
	bd3d->backGroundSize[1] = pos1[1] - pos3[1];
}

void gluNurbFlagRender(int curveAccuracy)
{
	/* Simple knots i.e. no weighting */
	float s_knots[S_NUMKNOTS] = { 0, 0, 0, 0, 1, 1, 1, 1 };
	float t_knots[T_NUMKNOTS] = { 0, 0, 1, 1 };

	if (flag.flagNurb == NULL)
		flag.flagNurb = gluNewNurbsRenderer();

	glPushMatrix();
	glLoadMatrixf(GetModelViewMatrix());

	/* Set size of polygons */
	gluNurbsProperty(flag.flagNurb, GLU_SAMPLING_TOLERANCE, 500.0f / curveAccuracy);

	gluBeginSurface(flag.flagNurb);
	gluNurbsSurface(flag.flagNurb, S_NUMKNOTS, s_knots, T_NUMKNOTS, t_knots, 3 * T_NUMPOINTS, 3,
		&flag.ctlpoints[0][0][0], S_NUMPOINTS, T_NUMPOINTS, GL_MAP2_VERTEX_3);
	gluEndSurface(flag.flagNurb);

	glPopMatrix();
}

void renderFlagNumbers(const BoardData3d* bd3d, int resignedValue)
{
	/* Draw number on flag */
	setMaterial(&bd3d->flagNumberMat);

	MoveToFlagMiddle();

	glDisable(GL_DEPTH_TEST);
	/* No specular light */
	float specular[4];
	float zero[4] = { 0, 0, 0, 0 };
	glGetLightfv(GL_LIGHT0, GL_SPECULAR, specular);
	glLightfv(GL_LIGHT0, GL_SPECULAR, zero);

	/* Draw number */
	char flagValue[2] = " ";

	flagValue[0] = '0' + (char)abs(resignedValue);

	float oldScale = bd3d->cubeFont.scale;
	((BoardData3d*)bd3d)->cubeFont.scale *= 1.3f;

	glLineWidth(.5f);
	glPrintCube(&bd3d->cubeFont, flagValue, /*MAA*/0);
	((BoardData3d*)bd3d)->cubeFont.scale = oldScale;

	glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
	glEnable(GL_DEPTH_TEST);
}

static void
drawFlag(const ModelManager* modelHolder, const BoardData* bd, const BoardData3d* bd3d, const renderdata* prd)
{
	int isStencil = glIsEnabled(GL_STENCIL_TEST);

	if (isStencil)
		glDisable(GL_STENCIL_TEST);

	waveFlag(bd3d->flagWaved);

	renderFlag(modelHolder, bd3d, bd->rd->curveAccuracy, bd->turn, bd->resigned);

	if (isStencil)
		glEnable(GL_STENCIL_TEST);
}
