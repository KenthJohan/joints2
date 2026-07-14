#include "b2DebugDraw_init.h"
#include <draw.h>

void DrawPolygonFcn(b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, b2HexColor color, void *context)
{
	draw_t *draw = (draw_t *)(context);
	draw_polygon(draw, transform, vertices, vertexCount, color);
}

void DrawSolidPolygonFcn(b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, float radius, b2HexColor color, void *context)
{
	draw_t *draw = (draw_t *)(context);
	draw_solid_polygon(draw, transform, vertices, vertexCount, radius, color);
}

void DrawCircleFcn(b2Pos center, float radius, b2HexColor color, void *context)
{
	draw_t *draw = (draw_t *)(context);
	draw_circle(draw, center, radius, color);
}

void DrawSolidCircleFcn(b2WorldTransform transform, b2Vec2 center, float radius, b2HexColor color, void *context)
{
	draw_t *draw = (draw_t *)(context);
	draw_solid_circle(draw, transform, center, radius, color);
}

void DrawSolidCapsuleFcn(b2Pos p1, b2Pos p2, float radius, b2HexColor color, void *context)
{
	draw_t *draw = (draw_t *)(context);
	draw_capsule(draw, p1, p2, radius, color);
}

void DrawLineFcn(b2Pos p1, b2Pos p2, b2HexColor color, void *context)
{
	draw_t *draw = (draw_t *)(context);
	draw_line(draw, p1, p2, color);
}

void DrawTransformFcn(b2WorldTransform transform, void *context)
{
	draw_t *draw = (draw_t *)(context);
	draw_transform(draw, transform, 1.0f);
}

void DrawPointFcn(b2Pos p, float size, b2HexColor color, void *context)
{
	draw_t *draw = (draw_t *)(context);
	draw_point(draw, p, size, color);
}

void DrawStringFcn(b2Pos p, const char *s, b2HexColor color, void *context)
{
	draw_t *draw = (draw_t *)(context);
	draw_string(draw, p, color, "%s", s);
}

void DrawBoundsFcn(b2AABB aabb, b2HexColor color, void *context)
{
	draw_t *draw = (draw_t *)(context);
	draw_bounds(draw, aabb, color);
}

void b2DebugDraw_init(b2DebugDraw *d, draw_t *draw)
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
