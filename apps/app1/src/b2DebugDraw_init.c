#include "b2DebugDraw_init.h"
#include <assert.h>
#include <egg.h>
#include <stddef.h>

static egg_t *sGetContext(void *context)
{
	return (egg_t *)(context);
}

void DrawPolygonFcn(b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, b2HexColor color, void *context)
{
	egg_t *egg = sGetContext(context);
	assert(egg != NULL);
	egg_draw_polygon(egg, (const egg_vec2_t *)vertices, vertexCount, (float)transform.p.x, (float)transform.p.y, transform.q.c, transform.q.s, color);
}

void DrawSolidPolygonFcn(b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, float radius, b2HexColor color, void *context)
{
	(void)radius;
	assert(offsetof(b2Vec2, x) == offsetof(egg_vec2_t, x));
	assert(offsetof(b2Vec2, y) == offsetof(egg_vec2_t, y));
	assert(sizeof(b2Vec2) == sizeof(egg_vec2_t));

	egg_t *egg = sGetContext(context);
	assert(egg != NULL);
	egg_draw_polygon(egg, (const egg_vec2_t *)vertices, vertexCount, (float)transform.p.x, (float)transform.p.y, transform.q.c, transform.q.s, color);
}

void DrawCircleFcn(b2Pos center, float radius, b2HexColor color, void *context)
{
	// Circle outline thickness uses pixel-size units.
	egg_t *egg = sGetContext(context);
	assert(egg != NULL);
	egg_draw_circle_outline(egg, (float)center.x, (float)center.y, radius, 1.0f, color);
}

void DrawSolidCircleFcn(b2WorldTransform transform, b2Vec2 center, float radius, b2HexColor color, void *context)
{
	egg_t *egg = sGetContext(context);
	assert(egg != NULL);
	egg_draw_circle(egg, (float)transform.p.x + center.x, (float)transform.p.y + center.y, radius, color);
}

void DrawSolidCapsuleFcn(b2Pos p1, b2Pos p2, float radius, b2HexColor color, void *context)
{
	// Capsule outline thickness uses pixel-size units.
	egg_t *egg = sGetContext(context);
	assert(egg != NULL);
	egg_draw_capsule_outline(egg, (float)p1.x, (float)p1.y, (float)p2.x, (float)p2.y, radius, 1.0f, color);
}

void DrawLineFcn(b2Pos p1, b2Pos p2, b2HexColor color, void *context)
{
	// Line thickness uses pixel-size units.
	egg_t *egg = sGetContext(context);
	assert(egg != NULL);
	egg_draw_line(egg, (float)p1.x, (float)p1.y, (float)p2.x, (float)p2.y, 1.0f, color);
}

void DrawTransformFcn(b2WorldTransform transform, void *context)
{
	egg_t *egg = sGetContext(context);
	assert(egg != NULL);
	egg_draw_transform(egg, (float)transform.p.x, (float)transform.p.y, transform.q.c, transform.q.s, 1.0f, 0xFFFF0000u);
}

void DrawPointFcn(b2Pos p, float size, b2HexColor color, void *context)
{
	// Point size uses pixel-size units.
	egg_t *egg = sGetContext(context);
	assert(egg != NULL);
	egg_draw_point(egg, (float)p.x, (float)p.y, size, color);
}

void DrawStringFcn(b2Pos p, const char *s, b2HexColor color, void *context)
{
	egg_t *egg = sGetContext(context);
	if (egg != NULL) {
		egg_draw_text(egg, (float)p.x, (float)p.y, 1.0f, 0.0f, 0.5f, color, s);
	}
}

void DrawBoundsFcn(b2AABB aabb, b2HexColor color, void *context)
{
	egg_t *egg = sGetContext(context);
	assert(egg != NULL);
	egg_draw_bounds(egg, aabb.lowerBound.x, aabb.lowerBound.y, aabb.upperBound.x, aabb.upperBound.y, color);
}

void b2DebugDraw_init(b2DebugDraw *d, egg_t *egg)
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
