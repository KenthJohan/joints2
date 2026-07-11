// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "draw_circle.h"

#include "draw_internal.h"

#define CIRCLE_BATCH_SIZE 2048

typedef struct
{
	b2Vec2 position;
	float  radius;
	RGBA8  rgba;
} CircleData;

ARRAY_DECLARE(CircleData);
ARRAY_INLINE(CircleData);
ARRAY_SOURCE(CircleData);

struct CircleRender
{
	CircleDataArray circles;
	GLuint          vaoId;
	GLuint          vboIds[2];
	GLuint          programId;
	GLint           projectionUniform;
	GLint           pixelScaleUniform;
};

CircleRender *CreateCircles(const DrawCreateInfo *createInfo)
{
	CircleRender *render     = malloc(sizeof(CircleRender));
	*render                  = (CircleRender){0};
	render->circles          = CircleDataArray_Create(CIRCLE_BATCH_SIZE);
	render->programId        = CreateProgramFromStrings(createInfo->shaders[DRAW_SHADER_CIRCLE_VERTEX],
	createInfo->shaders[DRAW_SHADER_CIRCLE_FRAGMENT]);
	render->projectionUniform = glGetUniformLocation(render->programId, "projectionMatrix");
	render->pixelScaleUniform = glGetUniformLocation(render->programId, "pixelScale");
	int vertexAttribute      = 0;
	int positionInstance     = 1;
	int radiusInstance       = 2;
	int colorInstance        = 3;

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

	CircleDataArray_Destroy(&render->circles);
	free(render);
}

void AddCircle(CircleRender *render, b2Vec2 center, float radius, b2HexColor color)
{
	RGBA8 rgba = MakeRGBA8(color, 1.0f);
	CircleDataArray_Push(&render->circles, (CircleData){center, radius, rgba});
}

void FlushCircles(CircleRender *render, Camera *camera, const float *projectionMatrix)
{
	int count = render->circles.count;
	if (count == 0) {
		return;
	}

	glUseProgram(render->programId);
	glUniformMatrix4fv(render->projectionUniform, 1, GL_FALSE, projectionMatrix);
	glUniform1f(render->pixelScaleUniform, camera->height / camera->zoom);

	glBindVertexArray(render->vaoId);
	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[1]);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	int base = 0;
	while (count > 0) {
		int batchCount = b2MinInt(count, CIRCLE_BATCH_SIZE);
		glBufferSubData(GL_ARRAY_BUFFER, 0, batchCount * sizeof(CircleData), render->circles.data + base);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, batchCount);

		CheckOpenGL();

		count -= CIRCLE_BATCH_SIZE;
		base += CIRCLE_BATCH_SIZE;
	}

	glDisable(GL_BLEND);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);

	render->circles.count = 0;
}
