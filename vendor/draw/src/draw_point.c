// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "draw_point.h"

#include "draw_internal.h"

#define POINT_BATCH_SIZE 2048

PointRender *CreatePointDrawData(const DrawCreateInfo *createInfo)
{
	PointRender *render      = malloc(sizeof(PointRender));
	*render                  = (PointRender){0};
	ecs_vec_init_t(NULL, &render->points, PointData, POINT_BATCH_SIZE);
	render->programId        = CreateProgramFromStrings(createInfo->shaders[DRAW_SHADER_POINT_VERTEX],
	createInfo->shaders[DRAW_SHADER_POINT_FRAGMENT]);
	render->projectionUniform = glGetUniformLocation(render->programId, "projectionMatrix");
	int vertexAttribute      = 0;
	int sizeAttribute        = 1;
	int colorAttribute       = 2;

	glGenVertexArrays(1, &render->vaoId);
	glGenBuffers(1, &render->vboId);

	glBindVertexArray(render->vaoId);
	glEnableVertexAttribArray(vertexAttribute);
	glEnableVertexAttribArray(sizeAttribute);
	glEnableVertexAttribArray(colorAttribute);

	glBindBuffer(GL_ARRAY_BUFFER, render->vboId);
	glBufferData(GL_ARRAY_BUFFER, POINT_BATCH_SIZE * sizeof(PointData), NULL, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(vertexAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(PointData), (void *)offsetof(PointData, position));
	glVertexAttribPointer(sizeAttribute, 1, GL_FLOAT, GL_FALSE, sizeof(PointData), (void *)offsetof(PointData, size));
	glVertexAttribPointer(colorAttribute, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PointData), (void *)offsetof(PointData, rgba));

	CheckOpenGL();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return render;
}

void DestroyPointDrawData(PointRender *render)
{
	if (render == NULL) {
		return;
	}

	if (render->vaoId) {
		glDeleteVertexArrays(1, &render->vaoId);
		glDeleteBuffers(1, &render->vboId);
	}

	if (render->programId) {
		glDeleteProgram(render->programId);
	}

	ecs_vec_fini_t(NULL, &render->points, PointData);
	free(render);
}

void AddPoint(PointRender *render, b2Vec2 v, float size, b2HexColor c)
{
	RGBA8 rgba = MakeRGBA8(c, 1.0f);
	ecs_vec_append_t(NULL, &render->points, PointData)[0] = (PointData){v, size, rgba};
}

void FlushPoints(PointRender *render, const float *projectionMatrix)
{
	PointData *points = ecs_vec_first_t(&render->points, PointData);
	int32_t count = ecs_vec_count(&render->points);
	if (count == 0) {
		return;
	}

	glUseProgram(render->programId);
	glUniformMatrix4fv(render->projectionUniform, 1, GL_FALSE, projectionMatrix);
	glBindVertexArray(render->vaoId);

	glBindBuffer(GL_ARRAY_BUFFER, render->vboId);
	glEnable(GL_PROGRAM_POINT_SIZE);

	int base = 0;
	while (count > 0) {
		int batchCount = b2MinInt(count, POINT_BATCH_SIZE);
		glBufferSubData(GL_ARRAY_BUFFER, 0, batchCount * sizeof(PointData), points + base);
		glDrawArrays(GL_POINTS, 0, batchCount);

		CheckOpenGL();

		count -= POINT_BATCH_SIZE;
		base += POINT_BATCH_SIZE;
	}

	glDisable(GL_PROGRAM_POINT_SIZE);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);

	ecs_vec_clear(&render->points);
}
