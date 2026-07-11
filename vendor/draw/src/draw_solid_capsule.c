// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "draw_solid_capsule.h"

#include "draw_internal.h"

#define CAPSULE_BATCH_SIZE 2048

typedef struct
{
	b2Transform transform;
	float       radius;
	float       length;
	RGBA8       rgba;
} Capsule;

ARRAY_DECLARE(Capsule);
ARRAY_INLINE(Capsule);
ARRAY_SOURCE(Capsule);

struct Capsules
{
	CapsuleArray capsules;
	GLuint       vaoId;
	GLuint       vboIds[2];
	GLuint       programId;
	GLint        projectionUniform;
	GLint        pixelScaleUniform;
};

Capsules *CreateCapsules(const DrawCreateInfo *createInfo)
{
	Capsules *render         = malloc(sizeof(Capsules));
	*render                  = (Capsules){0};
	render->capsules         = CapsuleArray_Create(CAPSULE_BATCH_SIZE);
	render->programId        = CreateProgramFromStrings(createInfo->shaders[DRAW_SHADER_SOLID_CAPSULE_VERTEX],
	createInfo->shaders[DRAW_SHADER_SOLID_CAPSULE_FRAGMENT]);
	render->projectionUniform = glGetUniformLocation(render->programId, "projectionMatrix");
	render->pixelScaleUniform = glGetUniformLocation(render->programId, "pixelScale");

	int vertexAttribute   = 0;
	int transformInstance = 1;
	int radiusInstance    = 2;
	int lengthInstance    = 3;
	int colorInstance     = 4;

	glGenVertexArrays(1, &render->vaoId);
	glGenBuffers(2, render->vboIds);

	glBindVertexArray(render->vaoId);
	glEnableVertexAttribArray(vertexAttribute);
	glEnableVertexAttribArray(transformInstance);
	glEnableVertexAttribArray(radiusInstance);
	glEnableVertexAttribArray(lengthInstance);
	glEnableVertexAttribArray(colorInstance);

	float  a          = 1.1f;
	b2Vec2 vertices[] = {{-a, -a}, {a, -a}, {-a, a}, {a, -a}, {a, a}, {-a, a}};
	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(vertexAttribute, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[1]);
	glBufferData(GL_ARRAY_BUFFER, CAPSULE_BATCH_SIZE * sizeof(Capsule), NULL, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(transformInstance, 4, GL_FLOAT, GL_FALSE, sizeof(Capsule), (void *)offsetof(Capsule, transform));
	glVertexAttribPointer(radiusInstance, 1, GL_FLOAT, GL_FALSE, sizeof(Capsule), (void *)offsetof(Capsule, radius));
	glVertexAttribPointer(lengthInstance, 1, GL_FLOAT, GL_FALSE, sizeof(Capsule), (void *)offsetof(Capsule, length));
	glVertexAttribPointer(colorInstance, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Capsule), (void *)offsetof(Capsule, rgba));

	glVertexAttribDivisor(transformInstance, 1);
	glVertexAttribDivisor(radiusInstance, 1);
	glVertexAttribDivisor(lengthInstance, 1);
	glVertexAttribDivisor(colorInstance, 1);

	CheckOpenGL();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return render;
}

void DestroyCapsules(Capsules *render)
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

	CapsuleArray_Destroy(&render->capsules);
	free(render);
}

void AddCapsule(Capsules *render, b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor c)
{
	b2Vec2 d      = b2Sub(p2, p1);
	float  length = b2Length(d);
	if (length < 0.001f) {
		printf("WARNING: sample app: capsule too short!\n");
		return;
	}

	b2Vec2      axis = {d.x / length, d.y / length};
	b2Transform transform;
	transform.p   = b2Lerp(p1, p2, 0.5f);
	transform.q.c = axis.x;
	transform.q.s = axis.y;

	RGBA8 rgba = MakeRGBA8(c, 1.0f);
	CapsuleArray_Push(&render->capsules, (Capsule){transform, radius, length, rgba});
}

void FlushCapsules(Capsules *render, Camera *camera)
{
	int count = render->capsules.count;
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
		int batchCount = b2MinInt(count, CAPSULE_BATCH_SIZE);
		glBufferSubData(GL_ARRAY_BUFFER, 0, batchCount * sizeof(Capsule), render->capsules.data + base);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, batchCount);

		CheckOpenGL();

		count -= CAPSULE_BATCH_SIZE;
		base += CAPSULE_BATCH_SIZE;
	}

	glDisable(GL_BLEND);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);

	render->capsules.count = 0;
}
