#include "draw.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EGG_PI 3.14159265358979323846f

typedef struct {
	egg_drawlist_t *list;
	float           instanceIndex;
	uint8_t         r;
	uint8_t         g;
	uint8_t         b;
	uint8_t         a;
} sBatch_t;

static void *sGrowBuffer(void *buffer, int32_t *capacity, int32_t count, size_t elementSize)
{
	if (count <= *capacity) {
		return buffer;
	}

	int32_t newCapacity = (*capacity == 0) ? 64 : *capacity;
	while (newCapacity < count) {
		newCapacity *= 2;
	}

	void *newBuffer = realloc(buffer, (size_t)newCapacity * elementSize);
	if (newBuffer == NULL) {
		return NULL;
	}

	*capacity = newCapacity;
	return newBuffer;
}

static unsigned char *sReadBinaryFile(const char *path, size_t *outSize)
{
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		return NULL;
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return NULL;
	}

	long size = ftell(file);
	if (size <= 0 || fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return NULL;
	}

	unsigned char *bytes = malloc((size_t)size);
	if (bytes == NULL) {
		fclose(file);
		return NULL;
	}

	if (fread(bytes, 1, (size_t)size, file) != (size_t)size) {
		fclose(file);
		free(bytes);
		return NULL;
	}

	fclose(file);
	*outSize = (size_t)size;
	return bytes;
}

static unsigned char *sLoadSystemFont(size_t *outSize)
{
	const char *candidates[] = {
	"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
	"/usr/share/fonts/TTF/DejaVuSans.ttf",
	"/usr/share/fonts/dejavu/DejaVuSans.ttf",
	"C:/Windows/Fonts/arial.ttf",
	"/System/Library/Fonts/Supplemental/Arial.ttf",
	};

	for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
		unsigned char *fontData = sReadBinaryFile(candidates[i], outSize);
		if (fontData != NULL) {
			return fontData;
		}
	}

	return NULL;
}

int egg_font_bake(egg_font_t *font, unsigned char *bitmap)
{
	size_t         fontSize = 0;
	unsigned char *fontData = sLoadSystemFont(&fontSize);
	if (fontData == NULL) {
		fprintf(stderr, "egg: failed to locate a default TrueType font\n");
		return 0;
	}

	memset(bitmap, 0, EGG_ATLAS_WIDTH * EGG_ATLAS_HEIGHT);
	int rowUsed = stbtt_BakeFontBitmap(fontData, 0, EGG_BAKE_FONT_SIZE, bitmap, EGG_ATLAS_WIDTH, EGG_ATLAS_HEIGHT,
	EGG_FIRST_CHAR, EGG_CHAR_COUNT, font->glyphs);
	free(fontData);
	if (rowUsed <= 0) {
		return 0;
	}

	font->lineHeight = EGG_BAKE_FONT_SIZE * 1.2f;
	return 1;
}

void egg_dl_init(egg_draw_t *d)
{
	memset(d, 0, sizeof(*d));
	d->pixelScale = 1.0f;
}

void egg_dl_destroy(egg_draw_t *d)
{
	for (int32_t i = 0; i < d->listCount; ++i) {
		free(d->lists[i].vertices.data);
		free(d->lists[i].transforms.data);
	}
	free(d->lists);
	memset(d, 0, sizeof(*d));
}

static egg_drawlist_t *sGetList(egg_draw_t *d, int32_t z)
{
	if (z < 0) {
		z = 0;
	}

	if (z >= d->listCount) {
		egg_drawlist_t *lists = sGrowBuffer(d->lists, &d->listCapacity, z + 1, sizeof(egg_drawlist_t));
		if (lists == NULL) {
			return NULL;
		}
		d->lists = lists;
		memset(d->lists + d->listCount, 0, (size_t)(z + 1 - d->listCount) * sizeof(egg_drawlist_t));
		d->listCount = z + 1;
	}

	return &d->lists[z];
}

static void sAppendVertex(egg_drawlist_t *l, float x, float y, float instanceIndex, float u, float v, float useTexture,
uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	egg_vertex_t *vertices = sGrowBuffer(l->vertices.data, &l->vertices.capacity, l->vertices.count + 1,
	sizeof(egg_vertex_t));
	if (vertices == NULL) {
		return;
	}

	l->vertices.data   = vertices;
	egg_vertex_t *dst  = &l->vertices.data[l->vertices.count];
	dst->position[0]   = x;
	dst->position[1]   = y;
	dst->instanceIndex = instanceIndex;
	dst->uv[0]         = u;
	dst->uv[1]         = v;
	dst->useTexture    = useTexture;
	dst->rgba[0]       = r;
	dst->rgba[1]       = g;
	dst->rgba[2]       = b;
	dst->rgba[3]       = a;
	l->vertices.count += 1;
}

static void sAppendTransform(egg_drawlist_t *l, float x, float y, float c, float s)
{
	egg_instance_transform_t *transforms = sGrowBuffer(l->transforms.data, &l->transforms.capacity,
	l->transforms.count + 1, sizeof(egg_instance_transform_t));
	if (transforms == NULL) {
		return;
	}

	l->transforms.data = transforms;
	egg_instance_transform_t *dst = &l->transforms.data[l->transforms.count];
	dst->x = x;
	dst->y = y;
	dst->c = c;
	dst->s = s;
	l->transforms.count += 1;
}

static void sAddQuad(egg_drawlist_t *l, float x0, float y0, float x1, float y1, float instanceIndex, float u0, float v0,
float u1, float v1, float useTexture, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	sAppendVertex(l, x0, y0, instanceIndex, u0, v0, useTexture, r, g, b, a);
	sAppendVertex(l, x1, y0, instanceIndex, u1, v0, useTexture, r, g, b, a);
	sAppendVertex(l, x1, y1, instanceIndex, u1, v1, useTexture, r, g, b, a);
	sAppendVertex(l, x0, y0, instanceIndex, u0, v0, useTexture, r, g, b, a);
	sAppendVertex(l, x1, y1, instanceIndex, u1, v1, useTexture, r, g, b, a);
	sAppendVertex(l, x0, y1, instanceIndex, u0, v1, useTexture, r, g, b, a);
}

static void sAddTriangle(egg_drawlist_t *l, float x0, float y0, float x1, float y1, float x2, float y2, float instanceIndex,
float useTexture, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	sAppendVertex(l, x0, y0, instanceIndex, 0.0f, 0.0f, useTexture, r, g, b, a);
	sAppendVertex(l, x1, y1, instanceIndex, 0.0f, 0.0f, useTexture, r, g, b, a);
	sAppendVertex(l, x2, y2, instanceIndex, 0.0f, 0.0f, useTexture, r, g, b, a);
}

static void sColorBytes(egg_color_t color, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	*r = (uint8_t)((color >> 16) & 0xFF);
	*g = (uint8_t)((color >> 8) & 0xFF);
	*b = (uint8_t)(color & 0xFF);
	*a = (uint8_t)((color >> 24) & 0xFF);
	if (*a == 0) {
		*a = 255;
	}
}

// Fetches the z list, pushes an instance transform and decodes the color.
static int sBeginBatch(sBatch_t *batch, egg_draw_t *d, int32_t z, float x, float y, float c, float s, egg_color_t color)
{
	batch->list = sGetList(d, z);
	if (batch->list == NULL) {
		return 0;
	}

	sAppendTransform(batch->list, x, y, c, s);
	batch->instanceIndex = (float)(batch->list->transforms.count - 1);
	sColorBytes(color, &batch->r, &batch->g, &batch->b, &batch->a);
	return 1;
}

static void sAddLine(const sBatch_t *batch, float pixelScale, float x1, float y1, float x2, float y2, float thickness)
{
	if (thickness <= 0.0f) {
		return;
	}

	float dx     = x2 - x1;
	float dy     = y2 - y1;
	float length = sqrtf(dx * dx + dy * dy);
	if (length <= 0.0f) {
		return;
	}

	float halfThickness = thickness * pixelScale * 0.5f;
	float nx            = -dy / length * halfThickness;
	float ny            = dx / length * halfThickness;

	float p1x = x1 + nx;
	float p1y = y1 + ny;
	float p2x = x1 - nx;
	float p2y = y1 - ny;
	float p3x = x2 + nx;
	float p3y = y2 + ny;
	float p4x = x2 - nx;
	float p4y = y2 - ny;

	egg_drawlist_t *l = batch->list;
	sAddTriangle(l, p1x, p1y, p2x, p2y, p3x, p3y, batch->instanceIndex, 0.0f, batch->r, batch->g, batch->b, batch->a);
	sAddTriangle(l, p2x, p2y, p4x, p4y, p3x, p3y, batch->instanceIndex, 0.0f, batch->r, batch->g, batch->b, batch->a);
}

void egg_dl_text(egg_draw_t *d, const egg_font_t *font, int32_t z, float x, float y, float rotationCos, float rotationSin, float fontSize, egg_color_t color, const char *string)
{
	if (string == NULL) {
		return;
	}

	float scale = fontSize / EGG_BAKE_FONT_SIZE;
	if (scale <= 0.0f) {
		return;
	}

	sBatch_t batch;
	if (!sBeginBatch(&batch, d, z, x, y, rotationCos, rotationSin, color)) {
		return;
	}

	float cursorX = 0.0f;
	float cursorY = 0.0f;
	float startX  = 0.0f;

	for (const char *p = string; *p != '\0'; ++p) {
		int codepoint = (unsigned char)*p;
		if (codepoint == '\n') {
			cursorX = startX;
			cursorY -= font->lineHeight * scale;
			continue;
		}

		if (codepoint == '\t') {
			cursorX += 4.0f * font->lineHeight * 0.5f * scale;
			continue;
		}

		if (codepoint < EGG_FIRST_CHAR || codepoint >= EGG_FIRST_CHAR + EGG_CHAR_COUNT) {
			codepoint = '?';
		}

		stbtt_aligned_quad q;
		stbtt_GetBakedQuad(font->glyphs, EGG_ATLAS_WIDTH, EGG_ATLAS_HEIGHT, codepoint - EGG_FIRST_CHAR, &cursorX,
		&cursorY, &q, 1);

		float dx0 = q.x0 - startX;
		float dy0 = q.y0 - cursorY;
		float dx1 = q.x1 - startX;
		float dy1 = q.y1 - cursorY;
		q.x0      = startX + scale * dx0;
		q.y0      = cursorY - scale * dy0;
		q.x1      = startX + scale * dx1;
		q.y1      = cursorY - scale * dy1;

		sAddQuad(batch.list, q.x0, q.y0, q.x1, q.y1, batch.instanceIndex, q.s0, q.t0, q.s1, q.t1, 1.0f, batch.r,
		batch.g, batch.b, batch.a);
	}
}

void egg_dl_line(egg_draw_t *d, int32_t z, float x1, float y1, float x2, float y2, float thickness, egg_color_t color)
{
	sBatch_t batch;
	if (!sBeginBatch(&batch, d, z, 0.0f, 0.0f, 1.0f, 0.0f, color)) {
		return;
	}

	sAddLine(&batch, d->pixelScale, x1, y1, x2, y2, thickness);
}

void egg_dl_point(egg_draw_t *d, int32_t z, float x, float y, float size, egg_color_t color)
{
	if (size <= 0.0f) {
		return;
	}

	sBatch_t batch;
	if (!sBeginBatch(&batch, d, z, x, y, 1.0f, 0.0f, color)) {
		return;
	}

	float scaledSize = size * d->pixelScale;
	sAddQuad(batch.list, -scaledSize, -scaledSize, scaledSize, scaledSize, batch.instanceIndex, 0.0f, 0.0f, 1.0f, 1.0f,
	0.0f, batch.r, batch.g, batch.b, batch.a);
}

void egg_dl_circle(egg_draw_t *d, int32_t z, float x, float y, float radius, egg_color_t color)
{
	if (radius <= 0.0f) {
		return;
	}

	sBatch_t batch;
	if (!sBeginBatch(&batch, d, z, x, y, 1.0f, 0.0f, color)) {
		return;
	}

	int segments = 32;
	for (int i = 0; i < segments; ++i) {
		float angle0 = (float)i * (2.0f * EGG_PI / (float)segments);
		float angle1 = (float)(i + 1) * (2.0f * EGG_PI / (float)segments);

		float x0 = cosf(angle0) * radius;
		float y0 = sinf(angle0) * radius;
		float x1 = cosf(angle1) * radius;
		float y1 = sinf(angle1) * radius;

		sAddTriangle(batch.list, 0.0f, 0.0f, x0, y0, x1, y1, batch.instanceIndex, 0.0f, batch.r, batch.g, batch.b,
		batch.a);
	}
}

void egg_dl_circle_outline(egg_draw_t *d, int32_t z, float x, float y, float radius, float thickness, egg_color_t color)
{
	if (radius <= 0.0f || thickness <= 0.0f) {
		return;
	}

	sBatch_t batch;
	if (!sBeginBatch(&batch, d, z, 0.0f, 0.0f, 1.0f, 0.0f, color)) {
		return;
	}

	int   segments        = 32;
	float scaledThickness = thickness * d->pixelScale;
	float innerRadius     = radius - scaledThickness * 0.5f;
	float outerRadius     = radius + scaledThickness * 0.5f;
	if (innerRadius < 0.0f) {
		innerRadius = 0.0f;
	}

	for (int i = 0; i < segments; ++i) {
		float angle0 = (float)i * (2.0f * EGG_PI / (float)segments);
		float angle1 = (float)(i + 1) * (2.0f * EGG_PI / (float)segments);

		float x0Outer = x + cosf(angle0) * outerRadius;
		float y0Outer = y + sinf(angle0) * outerRadius;
		float x1Outer = x + cosf(angle1) * outerRadius;
		float y1Outer = y + sinf(angle1) * outerRadius;
		float x0Inner = x + cosf(angle0) * innerRadius;
		float y0Inner = y + sinf(angle0) * innerRadius;
		float x1Inner = x + cosf(angle1) * innerRadius;
		float y1Inner = y + sinf(angle1) * innerRadius;

		sAddTriangle(batch.list, x0Outer, y0Outer, x1Outer, y1Outer, x1Inner, y1Inner, batch.instanceIndex, 0.0f,
		batch.r, batch.g, batch.b, batch.a);
		sAddTriangle(batch.list, x0Outer, y0Outer, x1Inner, y1Inner, x0Inner, y0Inner, batch.instanceIndex, 0.0f,
		batch.r, batch.g, batch.b, batch.a);
	}
}

// Adds one half-ring cap centred on (cx, cy) sweeping from startAngle to startAngle + PI.
static void sAddCap(const sBatch_t *batch, float cx, float cy, float startAngle, float radius, float innerRadius, int segments)
{
	for (int i = 0; i < segments; ++i) {
		float angle0 = startAngle + (float)i / (float)segments * EGG_PI;
		float angle1 = startAngle + (float)(i + 1) / (float)segments * EGG_PI;

		float ox0 = cx + cosf(angle0) * radius;
		float oy0 = cy + sinf(angle0) * radius;
		float ox1 = cx + cosf(angle1) * radius;
		float oy1 = cy + sinf(angle1) * radius;
		float ix0 = cx + cosf(angle0) * innerRadius;
		float iy0 = cy + sinf(angle0) * innerRadius;
		float ix1 = cx + cosf(angle1) * innerRadius;
		float iy1 = cy + sinf(angle1) * innerRadius;

		sAddTriangle(batch->list, ox0, oy0, ox1, oy1, ix1, iy1, batch->instanceIndex, 0.0f, batch->r, batch->g, batch->b,
		batch->a);
		sAddTriangle(batch->list, ox0, oy0, ix1, iy1, ix0, iy0, batch->instanceIndex, 0.0f, batch->r, batch->g, batch->b,
		batch->a);
	}
}

void egg_dl_capsule_outline(egg_draw_t *d, int32_t z, float x1, float y1, float x2, float y2, float radius, float thickness,
egg_color_t color)
{
	if (radius <= 0.0f || thickness <= 0.0f) {
		return;
	}

	sBatch_t batch;
	if (!sBeginBatch(&batch, d, z, 0.0f, 0.0f, 1.0f, 0.0f, color)) {
		return;
	}

	float dx     = x2 - x1;
	float dy     = y2 - y1;
	float length = sqrtf(dx * dx + dy * dy);
	if (length <= 0.0f) {
		return;
	}

	float nx         = dx / length;
	float ny         = dy / length;
	float halfLength = length * 0.5f;
	float centerX    = (x1 + x2) * 0.5f;
	float centerY    = (y1 + y2) * 0.5f;

	sAddLine(&batch, d->pixelScale, x1, y1, x2, y2, thickness);

	int   segments    = 24;
	float innerRadius = radius - thickness * d->pixelScale * 0.5f;
	sAddCap(&batch, centerX + nx * halfLength, centerY + ny * halfLength, EGG_PI, radius, innerRadius, segments);
	sAddCap(&batch, centerX - nx * halfLength, centerY - ny * halfLength, 0.0f, radius, innerRadius, segments);
}

void egg_dl_transform(egg_draw_t *d, int32_t z, float x, float y, float rotationCos, float rotationSin, float scale, egg_color_t color)
{
	sBatch_t batch;
	if (!sBeginBatch(&batch, d, z, x, y, rotationCos, rotationSin, color)) {
		return;
	}

	sAddLine(&batch, d->pixelScale, 0.0f, 0.0f, scale, 0.0f, 0.05f);
	sAddLine(&batch, d->pixelScale, 0.0f, 0.0f, 0.0f, scale, 0.05f);
}

void egg_dl_rectangle(egg_draw_t *d, int32_t z, float x, float y, float rotationCos, float rotationSin, float width, float height, egg_color_t color)
{
	if (width <= 0.0f || height <= 0.0f) {
		return;
	}

	sBatch_t batch;
	if (!sBeginBatch(&batch, d, z, x, y, rotationCos, rotationSin, color)) {
		return;
	}

	float halfWidth  = width * 0.5f;
	float halfHeight = height * 0.5f;
	sAddQuad(batch.list, -halfWidth, -halfHeight, halfWidth, halfHeight, batch.instanceIndex, 0.0f, 0.0f, 1.0f, 1.0f,
	0.0f, batch.r, batch.g, batch.b, batch.a);
}

void egg_dl_rectangle_outline(egg_draw_t *d, int32_t z, float x, float y, float rotationCos, float rotationSin, float width, float height, float thickness, egg_color_t color)
{
	if (width <= 0.0f || height <= 0.0f || thickness <= 0.0f) {
		return;
	}

	sBatch_t batch;
	if (!sBeginBatch(&batch, d, z, x, y, rotationCos, rotationSin, color)) {
		return;
	}

	float halfWidth  = width * 0.5f;
	float halfHeight = height * 0.5f;
	sAddLine(&batch, d->pixelScale, -halfWidth, -halfHeight, halfWidth, -halfHeight, thickness);
	sAddLine(&batch, d->pixelScale, halfWidth, -halfHeight, halfWidth, halfHeight, thickness);
	sAddLine(&batch, d->pixelScale, halfWidth, halfHeight, -halfWidth, halfHeight, thickness);
	sAddLine(&batch, d->pixelScale, -halfWidth, halfHeight, -halfWidth, -halfHeight, thickness);
}

void egg_dl_bounds(egg_draw_t *d, int32_t z, float minX, float minY, float maxX, float maxY, egg_color_t color)
{
	sBatch_t batch;
	if (!sBeginBatch(&batch, d, z, 0.0f, 0.0f, 1.0f, 0.0f, color)) {
		return;
	}

	sAddLine(&batch, d->pixelScale, minX, minY, maxX, minY, 0.05f);
	sAddLine(&batch, d->pixelScale, maxX, minY, maxX, maxY, 0.05f);
	sAddLine(&batch, d->pixelScale, maxX, maxY, minX, maxY, 0.05f);
	sAddLine(&batch, d->pixelScale, minX, maxY, minX, minY, 0.05f);
}

void egg_dl_polygon(egg_draw_t *d, int32_t z, const egg_vec2_t *vertices, int vertex_count, float tx, float ty,
float rot_c, float rot_s, egg_color_t color)
{
	if (vertices == NULL || vertex_count < 3) {
		return;
	}

	sBatch_t batch;
	if (!sBeginBatch(&batch, d, z, tx, ty, rot_c, rot_s, color)) {
		return;
	}

	for (int i = 1; i + 1 < vertex_count; ++i) {
		sAddTriangle(batch.list, vertices[0].x, vertices[0].y, vertices[i].x, vertices[i].y, vertices[i + 1].x,
		vertices[i + 1].y, batch.instanceIndex, 0.0f, batch.r, batch.g, batch.b, batch.a);
	}
}
