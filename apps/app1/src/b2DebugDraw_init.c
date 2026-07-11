#include "b2DebugDraw_init.h"
#include <draw.h>

void DrawPolygonFcn(b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, b2HexColor color, void *context)
{
	Draw *draw = (Draw *)(context);
	DrawPolygon(draw, transform, vertices, vertexCount, color);
}

void DrawSolidPolygonFcn(b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, float radius, b2HexColor color, void *context)
{
	Draw *draw = (Draw *)(context);
	DrawSolidPolygon(draw, transform, vertices, vertexCount, radius, color);
}

void DrawCircleFcn(b2Pos center, float radius, b2HexColor color, void *context)
{
	Draw *draw = (Draw *)(context);
	DrawCircle(draw, center, radius, color);
}

void DrawSolidCircleFcn(b2WorldTransform transform, b2Vec2 center, float radius, b2HexColor color, void *context)
{
	Draw *draw = (Draw *)(context);
	DrawSolidCircle(draw, transform, center, radius, color);
}

void DrawSolidCapsuleFcn(b2Pos p1, b2Pos p2, float radius, b2HexColor color, void *context)
{
	Draw *draw = (Draw *)(context);
	DrawCapsule(draw, p1, p2, radius, color);
}

void DrawLineFcn(b2Pos p1, b2Pos p2, b2HexColor color, void *context)
{
	Draw *draw = (Draw *)(context);
	DrawLine(draw, p1, p2, color);
}

void DrawTransformFcn(b2WorldTransform transform, void *context)
{
	Draw *draw = (Draw *)(context);
	DrawTransform(draw, transform, 1.0f);
}

void DrawPointFcn(b2Pos p, float size, b2HexColor color, void *context)
{
	Draw *draw = (Draw *)(context);
	DrawPoint(draw, p, size, color);
}

void DrawStringFcn(b2Pos p, const char *s, b2HexColor color, void *context)
{
	Draw *draw = (Draw *)(context);
	DrawString(draw, p, color, "%s", s);
}

void DrawBoundsFcn(b2AABB aabb, b2HexColor color, void *context)
{
	Draw *draw = (Draw *)(context);
	DrawBounds(draw, aabb, color);
}

void b2DebugDraw_init(b2DebugDraw *d, Draw *draw)
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
