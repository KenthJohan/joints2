// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "draw_solid_polygon.h"

#include "draw_internal.h"

#define POLYGON_BATCH_SIZE 2048

typedef struct
{
	b2Transform transform;
	b2Vec2      p1, p2, p3, p4, p5, p6, p7, p8;
	int         count;
	float       radius;
	RGBA8       color;
} Polygon;

ARRAY_DECLARE(Polygon);
ARRAY_INLINE(Polygon);
ARRAY_SOURCE(Polygon);

struct Polygons
{
	PolygonArray polygons;
	GLuint       vaoId;
	GLuint       vboIds[2];
	GLuint       programId;
	GLint        projectionUniform;
	GLint        pixelScaleUniform;
};

Polygons *CreatePolygons(const DrawCreateInfo *createInfo)
{
	Polygons *render         = malloc(sizeof(Polygons));
	*render                  = (Polygons){0};
	render->polygons         = PolygonArray_Create(10 * POLYGON_BATCH_SIZE);
	render->programId        = CreateProgramFromStrings(createInfo->shaders[DRAW_SHADER_SOLID_POLYGON_VERTEX],
	createInfo->shaders[DRAW_SHADER_SOLID_POLYGON_FRAGMENT]);
	render->projectionUniform = glGetUniformLocation(render->programId, "projectionMatrix");
	render->pixelScaleUniform = glGetUniformLocation(render->programId, "pixelScale");

	int vertexAttribute    = 0;
	int instanceTransform  = 1;
	int instancePoint12    = 2;
	int instancePoint34    = 3;
	int instancePoint56    = 4;
	int instancePoint78    = 5;
	int instancePointCount = 6;
	int instanceRadius     = 7;
	int instanceColor      = 8;

	glGenVertexArrays(1, &render->vaoId);
	glGenBuffers(2, render->vboIds);

	glBindVertexArray(render->vaoId);
	glEnableVertexAttribArray(vertexAttribute);
	glEnableVertexAttribArray(instanceTransform);
	glEnableVertexAttribArray(instancePoint12);
	glEnableVertexAttribArray(instancePoint34);
	glEnableVertexAttribArray(instancePoint56);
	glEnableVertexAttribArray(instancePoint78);
	glEnableVertexAttribArray(instancePointCount);
	glEnableVertexAttribArray(instanceRadius);
	glEnableVertexAttribArray(instanceColor);

	float  a          = 1.1f;
	b2Vec2 vertices[] = {{-a, -a}, {a, -a}, {-a, a}, {a, -a}, {a, a}, {-a, a}};
	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(vertexAttribute, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[1]);
	glBufferData(GL_ARRAY_BUFFER, POLYGON_BATCH_SIZE * sizeof(Polygon), NULL, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(instanceTransform, 4, GL_FLOAT, GL_FALSE, sizeof(Polygon), (void *)offsetof(Polygon, transform));
	glVertexAttribPointer(instancePoint12, 4, GL_FLOAT, GL_FALSE, sizeof(Polygon), (void *)offsetof(Polygon, p1));
	glVertexAttribPointer(instancePoint34, 4, GL_FLOAT, GL_FALSE, sizeof(Polygon), (void *)offsetof(Polygon, p3));
	glVertexAttribPointer(instancePoint56, 4, GL_FLOAT, GL_FALSE, sizeof(Polygon), (void *)offsetof(Polygon, p5));
	glVertexAttribPointer(instancePoint78, 4, GL_FLOAT, GL_FALSE, sizeof(Polygon), (void *)offsetof(Polygon, p7));
	glVertexAttribIPointer(instancePointCount, 1, GL_INT, sizeof(Polygon), (void *)offsetof(Polygon, count));
	glVertexAttribPointer(instanceRadius, 1, GL_FLOAT, GL_FALSE, sizeof(Polygon), (void *)offsetof(Polygon, radius));
	glVertexAttribPointer(instanceColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Polygon), (void *)offsetof(Polygon, color));

	glVertexAttribDivisor(instanceTransform, 1);
	glVertexAttribDivisor(instancePoint12, 1);
	glVertexAttribDivisor(instancePoint34, 1);
	glVertexAttribDivisor(instancePoint56, 1);
	glVertexAttribDivisor(instancePoint78, 1);
	glVertexAttribDivisor(instancePointCount, 1);
	glVertexAttribDivisor(instanceRadius, 1);
	glVertexAttribDivisor(instanceColor, 1);

	CheckOpenGL();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return render;
}

void DestroyPolygons(Polygons *render)
{
	if (render == NULL) {
		return;
	}

	if (render->vaoId) {
		glDeleteVertexArrays(1, &render->vaoId);
		glDeleteBuffers(2, render->vboIds);
	}

	if (render->programId) {
		glDeleteProgram(render->programId);
	}

	PolygonArray_Destroy(&render->polygons);
	free(render);
}

void AddPolygon(Polygons *render, b2Transform transform, const b2Vec2 *points, int count, float radius, b2HexColor color)
{
	Polygon data   = {};
	data.transform = transform;

	int     n  = count < 8 ? count : 8;
	b2Vec2 *ps = &data.p1;
	for (int i = 0; i < n; ++i) {
		ps[i] = points[i];
	}

	data.count  = n;
	data.radius = radius;
	data.color  = MakeRGBA8(color, 1.0f);

	PolygonArray_Push(&render->polygons, data);
}

void FlushPolygons(Polygons *render, Camera *camera)
{
	int count = render->polygons.count;
	if (count == 0) {
		return;
	}

	glUseProgram(render->programId);

	float proj[16] = {0.0f};
	BuildProjectionMatrix(camera, proj, 0.2f);

	glUniformMatrix4fv(render->projectionUniform, 1, GL_FALSE, proj);
	glUniform1f(render->pixelScaleUniform, camera->height / camera->zoom);

	glBindVertexArray(render->vaoId);
	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[1]);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	int base = 0;
	while (count > 0) {
		int batchCount = b2MinInt(count, POLYGON_BATCH_SIZE);
		glBufferSubData(GL_ARRAY_BUFFER, 0, batchCount * sizeof(Polygon), render->polygons.data + base);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, batchCount);
		CheckOpenGL();

		count -= POLYGON_BATCH_SIZE;
		base += POLYGON_BATCH_SIZE;
	}

	glDisable(GL_BLEND);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);

	render->polygons.count = 0;
}
