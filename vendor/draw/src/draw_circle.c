// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "draw_circle.h"

#include "draw_internal.h"

#define CIRCLE_BATCH_SIZE 2048


CircleRender *CreateCircles(const DrawCreateInfo *createInfo)
{
	CircleRender *render      = malloc(sizeof(CircleRender));
	*render                   = (CircleRender){0};
	ecs_vec_init_t(NULL, &render->circles, CircleData, CIRCLE_BATCH_SIZE);
	render->programId         = CreateProgramFromStrings(createInfo->shaders[DRAW_SHADER_CIRCLE_VERTEX],
	createInfo->shaders[DRAW_SHADER_CIRCLE_FRAGMENT]);
	render->projectionUniform = glGetUniformLocation(render->programId, "projectionMatrix");
	render->pixelScaleUniform = glGetUniformLocation(render->programId, "pixelScale");
	int vertexAttribute       = 0;
	int positionInstance      = 1;
	int radiusInstance        = 2;
	int colorInstance         = 3;

	glGenVertexArrays(1, &render->vaoId);
	glGenBuffers(2, render->vboIds);

	glBindVertexArray(render->vaoId);
	glEnableVertexAttribArray(vertexAttribute);
	glEnableVertexAttribArray(positionInstance);
	glEnableVertexAttribArray(radiusInstance);
	glEnableVertexAttribArray(colorInstance);

	float  a          = 1.1f;
	b2Vec2 vertices[] = {{-a, -a}, {a, -a}, {-a, a}, {a, -a}, {a, a}, {-a, a}};
	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(vertexAttribute, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[1]);
	glBufferData(GL_ARRAY_BUFFER, CIRCLE_BATCH_SIZE * sizeof(CircleData), NULL, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(positionInstance, 2, GL_FLOAT, GL_FALSE, sizeof(CircleData), (void *)offsetof(CircleData, position));
	glVertexAttribPointer(radiusInstance, 1, GL_FLOAT, GL_FALSE, sizeof(CircleData), (void *)offsetof(CircleData, radius));
	glVertexAttribPointer(colorInstance, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(CircleData), (void *)offsetof(CircleData, rgba));

	glVertexAttribDivisor(positionInstance, 1);
	glVertexAttribDivisor(radiusInstance, 1);
	glVertexAttribDivisor(colorInstance, 1);

	CheckOpenGL();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return render;
}

void DestroyCircles(CircleRender *render)
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

	ecs_vec_fini_t(NULL, &render->circles, CircleData);
	free(render);
}

void AddCircle(CircleRender *render, b2Vec2 center, float radius, b2HexColor color)
{
	RGBA8 rgba = MakeRGBA8(color, 1.0f);
	ecs_vec_append_t(NULL, &render->circles, CircleData)[0] = (CircleData){center, radius, rgba};
}

void FlushCircles(CircleRender *render, float pixelScale, const float *projectionMatrix)
{
	CircleData *circles = ecs_vec_first_t(&render->circles, CircleData);
	int32_t count = ecs_vec_count(&render->circles);
	if (count == 0) {
		return;
	}

	glUseProgram(render->programId);
	glUniformMatrix4fv(render->projectionUniform, 1, GL_FALSE, projectionMatrix);
	glUniform1f(render->pixelScaleUniform, pixelScale);

	glBindVertexArray(render->vaoId);
	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[1]);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	int base = 0;
	while (count > 0) {
		int batchCount = b2MinInt(count, CIRCLE_BATCH_SIZE);
		glBufferSubData(GL_ARRAY_BUFFER, 0, batchCount * sizeof(CircleData), circles + base);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, batchCount);

		CheckOpenGL();

		count -= CIRCLE_BATCH_SIZE;
		base += CIRCLE_BATCH_SIZE;
	}

	glDisable(GL_BLEND);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);

	ecs_vec_clear(&render->circles);
}
