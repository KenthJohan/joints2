// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "draw_solid_circle.h"

#include "draw_internal.h"

#define SOLID_CIRCLE_BATCH_SIZE 2048

typedef struct
{
	b2Transform transform;
	float       radius;
	RGBA8       rgba;
} SolidCircle;

ARRAY_DECLARE(SolidCircle);
ARRAY_INLINE(SolidCircle);
ARRAY_SOURCE(SolidCircle);

struct SolidCircles
{
	SolidCircleArray circles;
	GLuint           vaoId;
	GLuint           vboIds[2];
	GLuint           programId;
	GLint            projectionUniform;
	GLint            pixelScaleUniform;
};

SolidCircles *CreateSolidCircles(const DrawCreateInfo *createInfo)
{
	SolidCircles *render     = malloc(sizeof(SolidCircles));
	*render                  = (SolidCircles){0};
	render->circles          = SolidCircleArray_Create(SOLID_CIRCLE_BATCH_SIZE);
	render->programId        = CreateProgramFromStrings(createInfo->shaders[DRAW_SHADER_SOLID_CIRCLE_VERTEX],
	createInfo->shaders[DRAW_SHADER_SOLID_CIRCLE_FRAGMENT]);
	render->projectionUniform = glGetUniformLocation(render->programId, "projectionMatrix");
	render->pixelScaleUniform = glGetUniformLocation(render->programId, "pixelScale");

	glGenVertexArrays(1, &render->vaoId);
	glGenBuffers(2, render->vboIds);

	glBindVertexArray(render->vaoId);

	int vertexAttribute   = 0;
	int transformInstance = 1;
	int radiusInstance    = 2;
	int colorInstance     = 3;
	glEnableVertexAttribArray(vertexAttribute);
	glEnableVertexAttribArray(transformInstance);
	glEnableVertexAttribArray(radiusInstance);
	glEnableVertexAttribArray(colorInstance);

	float  a          = 1.1f;
	b2Vec2 vertices[] = {{-a, -a}, {a, -a}, {-a, a}, {a, -a}, {a, a}, {-a, a}};
	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(vertexAttribute, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[1]);
	glBufferData(GL_ARRAY_BUFFER, SOLID_CIRCLE_BATCH_SIZE * sizeof(SolidCircle), NULL, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(transformInstance, 4, GL_FLOAT, GL_FALSE, sizeof(SolidCircle), (void *)offsetof(SolidCircle, transform));
	glVertexAttribPointer(radiusInstance, 1, GL_FLOAT, GL_FALSE, sizeof(SolidCircle), (void *)offsetof(SolidCircle, radius));
	glVertexAttribPointer(colorInstance, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SolidCircle), (void *)offsetof(SolidCircle, rgba));

	glVertexAttribDivisor(transformInstance, 1);
	glVertexAttribDivisor(radiusInstance, 1);
	glVertexAttribDivisor(colorInstance, 1);

	CheckOpenGL();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return render;
}

void DestroySolidCircles(SolidCircles *render)
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

	SolidCircleArray_Destroy(&render->circles);
	free(render);
}

void AddSolidCircle(SolidCircles *render, b2Transform transform, float radius, b2HexColor color)
{
	RGBA8 rgba = MakeRGBA8(color, 1.0f);
	SolidCircleArray_Push(&render->circles, (SolidCircle){transform, radius, rgba});
}

void FlushSolidCircles(SolidCircles *render, Camera *camera, const float *projectionMatrix)
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
		int batchCount = b2MinInt(count, SOLID_CIRCLE_BATCH_SIZE);
		glBufferSubData(GL_ARRAY_BUFFER, 0, batchCount * sizeof(SolidCircle), render->circles.data + base);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, batchCount);

		CheckOpenGL();

		count -= SOLID_CIRCLE_BATCH_SIZE;
		base += SOLID_CIRCLE_BATCH_SIZE;
	}

	glDisable(GL_BLEND);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);

	render->circles.count = 0;
}
