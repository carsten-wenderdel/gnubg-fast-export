/*
 * Copyright (C) 2019 Jon Kinsey <jonkinsey@gmail.com
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
 * $Id: ShimOGL.c,v 1.3 2019/11/13 21:41:05 Superfly_Jon Exp $
 */

#include "config.h"
#include <memory.h>
#include "render.h"
#include <cglm/affine.h>
#include "ShimOGL.h"
#include "fun3d.h"

typedef struct _MatStack
{
#define MAX_STACK_LEVEL 10	// Probably only using 3 or so...
	mat4 stack[MAX_STACK_LEVEL];
	int level;
} MatStack;

static void MatStackInit(MatStack* matStack)
{
	matStack->level = 0;
	glm_mat4_identity(matStack->stack[0]);
}

static void MatStackPush(MatStack* matStack)
{
	matStack->level++;
	glm_mat4_copy(matStack->stack[matStack->level - 1], matStack->stack[matStack->level]);
}

static void MatStackPop(MatStack* matStack)
{
	matStack->level--;
}

MatStack mvMatStack, txMatStack, pjMatStack;
GLenum curMatStack;

void InitMatStacks()
{
	MatStackInit(&mvMatStack);
	MatStackInit(&txMatStack);
	MatStackInit(&pjMatStack);
	curMatStack = GL_MODELVIEW;
}

static MatStack* GetCurMatStack()
{
	switch (curMatStack)
	{
	case GL_MODELVIEW:
		return &mvMatStack;
	case GL_TEXTURE:
		return &txMatStack;
	case GL_PROJECTION:
		return &pjMatStack;
	}
	return NULL;
}

static mat4* GetCurMatStackMat()
{
	return &GetCurMatStack()->stack[GetCurMatStack()->level];
}

float curNormal[3], curTexture[2];

OglModel* curModel = NULL;
GLenum curMode;
int modeVerts;

void SHIMsetMaterial(const Material* pMat)
{
	curModel->modelUsesTexture = (pMat && pMat->pTexture != 0);
}

void SHIMglBegin(GLenum mode)
{
	modeVerts = 0;
	curMode = mode;
}

void SHIMglEnd()
{
}

void SHIMglMatrixMode(GLenum mode)
{
	curMatStack = mode;
}

void SHIMglPushMatrix()
{
	MatStackPush(GetCurMatStack());
}

void SHIMglPopMatrix()
{
	MatStackPop(GetCurMatStack());
}

void SHIMglRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	vec3 v = { x, y, z };
	glm_make_rad(&angle);
	glm_rotate(*GetCurMatStackMat(), angle, v);
}

void SHIMglTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	vec3 v = { x, y, z };
	glm_translate(*GetCurMatStackMat(), v);
}

void SHIMglScalef(GLfloat x, GLfloat y, GLfloat z)
{
	vec3 v = { x, y, z };
	glm_scale(*GetCurMatStackMat(), v);
}

void SHIMglNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
	if (curModel->data)
	{
		curNormal[0] = nx;
		curNormal[1] = ny;
		curNormal[2] = nz;
	}
}

void SHIMglTexCoord2f(GLfloat s, GLfloat t)
{
	if (curModel->data)
	{
		curTexture[0] = s;
		curTexture[1] = t;
	}
}

static void AddData(float data)
{
	curModel->data[curModel->dataLength] = data;
	curModel->dataLength++;
}

static void AddVertex(int offset)
{
	int vertexSize = 6;
	if (curModel->modelUsesTexture)
		vertexSize += 2;

	if (curModel->data)
	{
		float* curPtr = &curModel->data[curModel->dataLength];
		memcpy(curPtr, curPtr + (offset * vertexSize), vertexSize * sizeof(float));
	}

	curModel->dataLength += vertexSize;
}

void SHIMglVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	modeVerts++;
	switch (curMode)
	{
	case GL_QUADS:
		if (modeVerts == 4)
		{
			modeVerts = 0;
			// Repeat 1st and 3rd points from last triangle to make a quad
			AddVertex(-3);
			AddVertex(-2);
		}
		break;
	case GL_QUAD_STRIP:
		if (modeVerts == 4)
		{
			// Repeat 1st and 3rd points from last triangle to make a quad
			AddVertex(-1);
			AddVertex(-3);
		}
		else if (modeVerts > 4 && (modeVerts % 2) == 0)
		{
			// Repeat 1st and 3rd points from last triangle to make a quad with the last vertex
			AddVertex(-4);
			AddVertex(-3);
			// Repeat 2 other vertex points to make the quad with the current vertex
			AddVertex(-3);
			AddVertex(-5);
		}
		break;
	case GL_TRIANGLE_STRIP:
		if (modeVerts > 3)
		{
			// Repeat 2 previous points, note need odd/even case as winding is opposite for each strip
			if ((modeVerts % 2) == 1)
			{
				AddVertex(-3);
				AddVertex(-2);
			}
			else
			{
				AddVertex(-1);
				AddVertex(-3);
			}
		}
		break;
	case GL_TRIANGLE_FAN:
		if (modeVerts > 3)
		{
			// Repeat 1st point and last edge point to make a fan
			AddVertex(-3);
			AddVertex(-2);
		}
		break;
	}

	if (curModel->data)
	{
		vec3 mv;

		if (curModel->modelUsesTexture)
		{
			vec3 tv = { curTexture[0], curTexture[1], 1 };
			glm_mat4_mulv3(txMatStack.stack[txMatStack.level], tv, 1, mv);
			AddData(mv[0]);
			AddData(mv[1]);
		}
		vec3 nv = { curNormal[0], curNormal[1], curNormal[2] };
		//TODO: May need to multiple by inverse transpose matrix when scaling is being used...
		glm_mat4_mulv3(*GetCurMatStackMat(), nv, 0, mv);
		glm_vec3_normalize(mv);	// Needed?
		AddData(mv[0]);
		AddData(mv[1]);
		AddData(mv[2]);

		vec3 v = { x, y, z };
		glm_mat4_mulv3(*GetCurMatStackMat(), v, 1, mv);
		AddData(mv[0]);
		AddData(mv[1]);
		AddData(mv[2]);
	}
	else
	{
		curModel->dataLength += (curModel->modelUsesTexture) ? 8 : 6;
	}
}

void SHIMglVertex2f(GLfloat x, GLfloat y)
{
	SHIMglVertex3f(x, y, 0);
}
