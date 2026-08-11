#include "b2DebugDraw_init.h"
#include <assert.h>
#include <draw.h>
#include <stddef.h>

static AppDrawContext *sGetContext(void *context)
{
	return (AppDrawContext *)(context);
}

void DrawPolygonFcn(b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, b2HexColor color, void *context)
{
	AppDrawContext *draw = sGetContext(context);
	draw_polygon(draw->draw, (float)transform.p.x, (float)transform.p.y, transform.q.c, transform.q.s, &vertices[0].x, vertexCount, color);
}

void DrawSolidPolygonFcn(b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, float radius, b2HexColor color, void *context)
{
	(void)radius;
	assert(offsetof(b2Vec2, x) == offsetof(egg_vec2_t, x));
	assert(offsetof(b2Vec2, y) == offsetof(egg_vec2_t, y));
	assert(sizeof(b2Vec2) == sizeof(egg_vec2_t));

	AppDrawContext *draw = sGetContext(context);
	if (draw->egg != NULL) {
		egg_draw_polygon(draw->egg, (const egg_vec2_t *)vertices, vertexCount, (float)transform.p.x, (float)transform.p.y,
			transform.q.c, transform.q.s, color);
	}
}

void DrawCircleFcn(b2Pos center, float radius, b2HexColor color, void *context)
{
	AppDrawContext *draw = sGetContext(context);
	draw_circle(draw->draw, (float)center.x, (float)center.y, radius, color);
}

void DrawSolidCircleFcn(b2WorldTransform transform, b2Vec2 center, float radius, b2HexColor color, void *context)
{
	AppDrawContext *draw = sGetContext(context);
	draw_solid_circle(draw->draw, (float)transform.p.x, (float)transform.p.y, transform.q.c, transform.q.s, center.x, center.y, radius, color);
}

void DrawSolidCapsuleFcn(b2Pos p1, b2Pos p2, float radius, b2HexColor color, void *context)
{
	AppDrawContext *draw = sGetContext(context);
	draw_capsule(draw->draw, (float)p1.x, (float)p1.y, (float)p2.x, (float)p2.y, radius, color);
}

void DrawLineFcn(b2Pos p1, b2Pos p2, b2HexColor color, void *context)
{
	AppDrawContext *draw = sGetContext(context);
	draw_line(draw->draw, (float)p1.x, (float)p1.y, (float)p2.x, (float)p2.y, color);
}

void DrawTransformFcn(b2WorldTransform transform, void *context)
{
	AppDrawContext *draw = sGetContext(context);
	draw_transform(draw->draw, (float)transform.p.x, (float)transform.p.y, transform.q.c, transform.q.s, 1.0f);
}

void DrawPointFcn(b2Pos p, float size, b2HexColor color, void *context)
{
	AppDrawContext *draw = sGetContext(context);
	draw_point(draw->draw, (float)p.x, (float)p.y, size, color);
}

void DrawStringFcn(b2Pos p, const char *s, b2HexColor color, void *context)
{
	AppDrawContext *draw = sGetContext(context);
	egg_draw_text(draw->egg, (float)p.x, (float)p.y, 1.0f, 0.0f, 0.5f, color, s);
}

void DrawBoundsFcn(b2AABB aabb, b2HexColor color, void *context)
{
	AppDrawContext *draw = sGetContext(context);
	draw_bounds(draw->draw, aabb.lowerBound.x, aabb.lowerBound.y, aabb.upperBound.x, aabb.upperBound.y, color);
}

void b2DebugDraw_init(b2DebugDraw *d, AppDrawContext *draw)
{
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
	d->context                  = draw;
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
