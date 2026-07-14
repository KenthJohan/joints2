// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "draw_line.h"

#include "draw_internal.h"

#define LINE_BATCH_SIZE (2 * 2048)

LineRender *CreateLineRender(const DrawCreateInfo *createInfo)
{
	LineRender *render        = malloc(sizeof(LineRender));
	*render                   = (LineRender){0};
	ecs_vec_init_t(NULL, &render->points, LineData, LINE_BATCH_SIZE);
	render->programId         = CreateProgramFromStrings(createInfo->shaders[DRAW_SHADER_LINE_VERTEX],
	createInfo->shaders[DRAW_SHADER_LINE_FRAGMENT]);
	render->projectionUniform = glGetUniformLocation(render->programId, "projectionMatrix");
	int vertexAttribute       = 0;
	int colorAttribute        = 1;

	glGenVertexArrays(1, &render->vaoId);
	glGenBuffers(1, &render->vboId);

	glBindVertexArray(render->vaoId);
	glEnableVertexAttribArray(vertexAttribute);
	glEnableVertexAttribArray(colorAttribute);

	glBindBuffer(GL_ARRAY_BUFFER, render->vboId);
	glBufferData(GL_ARRAY_BUFFER, LINE_BATCH_SIZE * sizeof(LineData), NULL, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(vertexAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(LineData), (void *)offsetof(LineData, position));
	glVertexAttribPointer(colorAttribute, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(LineData), (void *)offsetof(LineData, rgba));

	CheckOpenGL();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return render;
}

void DestroyLineRender(LineRender *render)
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

	ecs_vec_fini_t(NULL, &render->points, LineData);
	free(render);
}

void AddLine(LineRender *render, b2Vec2 p1, b2Vec2 p2, b2HexColor c)
{
	RGBA8 rgba = MakeRGBA8(c, 1.0f);
	ecs_vec_append_t(NULL, &render->points, LineData)[0] = (LineData){p1, rgba};
	ecs_vec_append_t(NULL, &render->points, LineData)[0] = (LineData){p2, rgba};
}

void FlushLines(LineRender *render, const float *projectionMatrix)
{
	LineData *points = ecs_vec_first_t(&render->points, LineData);
	int32_t count = ecs_vec_count(&render->points);
	if (count == 0) {
		return;
	}

	assert(count % 2 == 0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(render->programId);
	glUniformMatrix4fv(render->projectionUniform, 1, GL_FALSE, projectionMatrix);
	glBindVertexArray(render->vaoId);
	glBindBuffer(GL_ARRAY_BUFFER, render->vboId);

	int base = 0;
	while (count > 0) {
		int batchCount = b2MinInt(count, LINE_BATCH_SIZE);
		glBufferSubData(GL_ARRAY_BUFFER, 0, batchCount * sizeof(LineData), points + base);
		glDrawArrays(GL_LINES, 0, batchCount);

		CheckOpenGL();

		count -= LINE_BATCH_SIZE;
		base += LINE_BATCH_SIZE;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);

	ecs_vec_clear(&render->points);
}
