#include "b2DebugDraw_init.h"
#include "AppDraw.h"
#include <assert.h>
#include <EgShapedraw.h>
#include <stddef.h>

static EgShapedrawList *sGetContext(void *context)
{
	return (EgShapedrawList *)(context);
}

static m3f32 sMakeTransform(float x, float y, float rotationCos, float rotationSin)
{
	return (m3f32){
		.c0 = {rotationCos, rotationSin, 0.0f},
		.c1 = {-rotationSin, rotationCos, 0.0f},
		.c2 = {x, y, 1.0f},
	};
}

static m3f32 sIdentityTransform(void)
{
	return (m3f32)M3_IDENTITY;
}

void DrawPolygonFcn(b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, b2HexColor color, void *context)
{
	EgShapedrawList *egg = sGetContext(context);
	assert(egg != NULL);
	m3f32 matrix = sMakeTransform((float)transform.p.x, (float)transform.p.y, transform.q.c, transform.q.s);
	EgShapedrawList_AddPolygon(egg, APP_DRAW_Z_DEBUG, &matrix, (const EgShapedrawVec2 *)vertices, vertexCount, color);
}

void DrawSolidPolygonFcn(b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, float radius, b2HexColor color, void *context)
{
	(void)radius;
	assert(offsetof(b2Vec2, x) == offsetof(EgShapedrawVec2, x));
	assert(offsetof(b2Vec2, y) == offsetof(EgShapedrawVec2, y));
	assert(sizeof(b2Vec2) == sizeof(EgShapedrawVec2));

	EgShapedrawList *egg = sGetContext(context);
	assert(egg != NULL);
	m3f32 matrix = sMakeTransform((float)transform.p.x, (float)transform.p.y, transform.q.c, transform.q.s);
	EgShapedrawList_AddPolygon(egg, APP_DRAW_Z_DEBUG, &matrix, (const EgShapedrawVec2 *)vertices, vertexCount, color);
}

void DrawCircleFcn(b2Pos center, float radius, b2HexColor color, void *context)
{
	// Circle outline thickness uses pixel-size units.
	EgShapedrawList *egg = sGetContext(context);
	assert(egg != NULL);
	m3f32 matrix = sMakeTransform((float)center.x, (float)center.y, 1.0f, 0.0f);
	EgShapedrawList_AddCircleOutline(egg, APP_DRAW_Z_DEBUG, &matrix, radius, 1.0f, color);
}

void DrawSolidCircleFcn(b2WorldTransform transform, b2Vec2 center, float radius, b2HexColor color, void *context)
{
	EgShapedrawList *egg = sGetContext(context);
	assert(egg != NULL);
	float x = (float)transform.p.x + transform.q.c * center.x - transform.q.s * center.y;
	float y = (float)transform.p.y + transform.q.s * center.x + transform.q.c * center.y;
	m3f32 matrix = sMakeTransform(x, y, transform.q.c, transform.q.s);
	EgShapedrawList_AddCircle(egg, APP_DRAW_Z_DEBUG, &matrix, radius, color);
}

void DrawSolidCapsuleFcn(b2Pos p1, b2Pos p2, float radius, b2HexColor color, void *context)
{
	// Capsule outline thickness uses pixel-size units.
	EgShapedrawList *egg = sGetContext(context);
	assert(egg != NULL);
	m3f32 matrix = sMakeTransform((float)p1.x, (float)p1.y, 1.0f, 0.0f);
	EgShapedrawList_AddCapsuleOutline(egg, APP_DRAW_Z_DEBUG, &matrix, 0.0f, 0.0f, (float)(p2.x - p1.x), (float)(p2.y - p1.y), radius, 1.0f, color);
}

void DrawLineFcn(b2Pos p1, b2Pos p2, b2HexColor color, void *context)
{
	// Line thickness uses pixel-size units.
	EgShapedrawList *egg = sGetContext(context);
	assert(egg != NULL);
	m3f32 matrix = sMakeTransform((float)p1.x, (float)p1.y, 1.0f, 0.0f);
	EgShapedrawList_AddLine(egg, APP_DRAW_Z_DEBUG, &matrix, 0.0f, 0.0f, (float)(p2.x - p1.x), (float)(p2.y - p1.y), 1.0f, color);
}

void DrawTransformFcn(b2WorldTransform transform, void *context)
{
	EgShapedrawList *egg = sGetContext(context);
	assert(egg != NULL);
	m3f32 matrix = sMakeTransform((float)transform.p.x, (float)transform.p.y, transform.q.c, transform.q.s);
	EgShapedrawList_AddTransform(egg, APP_DRAW_Z_DEBUG, &matrix, 1.0f, 0xFFFF0000u);
}

void DrawPointFcn(b2Pos p, float size, b2HexColor color, void *context)
{
	// Point size uses pixel-size units.
	EgShapedrawList *egg = sGetContext(context);
	assert(egg != NULL);
	m3f32 matrix = sMakeTransform((float)p.x, (float)p.y, 1.0f, 0.0f);
	EgShapedrawList_AddPoint(egg, APP_DRAW_Z_DEBUG, &matrix, size, color);
}

void DrawStringFcn(b2Pos p, const char *s, b2HexColor color, void *context)
{
	EgShapedrawList *egg = sGetContext(context);
	if (egg != NULL) {
		m3f32 transform = {
			.c0 = {1.0f, 0.0f, 0.0f},
			.c1 = {0.0f, 1.0f, 0.0f},
			.c2 = {(float)p.x, (float)p.y, 1.0f},
		};
		EgShapedrawList_AddText(egg, APP_DRAW_Z_TEXT, &transform, 0.5f, color, s);
	}
}

void DrawBoundsFcn(b2AABB aabb, b2HexColor color, void *context)
{
	EgShapedrawList *egg = sGetContext(context);
	assert(egg != NULL);
	m3f32 matrix = sIdentityTransform();
	EgShapedrawList_AddBounds(egg, APP_DRAW_Z_DEBUG, &matrix, aabb.lowerBound.x, aabb.lowerBound.y, aabb.upperBound.x, aabb.upperBound.y, color);
}

void b2DebugDraw_init(b2DebugDraw *d, EgShapedrawList *egg)
{
	// Box2D debug draw callbacks receive world-space coordinates and sizes, so the egg
	// renderer should interpret the incoming values in world units and convert them to
	// pixel-space thickness/point radius using the camera-derived pixel scale.
	d->DrawPolygonFcn           = DrawPolygonFcn;
	d->DrawSolidPolygonFcn      = DrawSolidPolygonFcn;
	d->DrawCircleFcn            = DrawCircleFcn;
	d->DrawSolidCircleFcn       = DrawSolidCircleFcn;
	d->DrawSolidCapsuleFcn      = DrawSolidCapsuleFcn;
	d->DrawLineFcn              = DrawLineFcn;
	d->DrawTransformFcn         = DrawTransformFcn;
	d->DrawPointFcn             = DrawPointFcn;
	d->DrawStringFcn            = DrawStringFcn;
	d->DrawBoundsFcn            = DrawBoundsFcn;
	d->context                  = egg;
	d->drawMass                 = true;
	d->drawContacts             = true;
	d->drawContactForces        = true;
	d->drawingBounds.lowerBound = (b2Vec2){-FLT_MAX, -FLT_MAX};
	d->drawingBounds.upperBound = (b2Vec2){FLT_MAX, FLT_MAX};
	d->forceScale               = 1.0f;
	d->jointScale               = 1.0f;
	d->drawShapes               = true;
	// s_context.debugDraw.drawContactFeatures = true;
}
