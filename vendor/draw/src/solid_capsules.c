// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "solid_capsules.h"

#include "internal.h"

#define CAPSULE_BATCH_SIZE 2048

solid_capsules_t *solid_capsules_init(const draw_create_info_t *createInfo)
{
	solid_capsules_t *render = malloc(sizeof(solid_capsules_t));
	*render                  = (solid_capsules_t){0};
	ecs_vec_init_t(NULL, &render->capsules, solid_capsules_data_t, CAPSULE_BATCH_SIZE);
	render->programId         = CreateProgramFromStrings(createInfo->shaders[DRAW_SHADER_SOLID_CAPSULE_VERTEX],
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
	draw_vec2_t vertices[] = {{-a, -a}, {a, -a}, {-a, a}, {a, -a}, {a, a}, {-a, a}};
	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(vertexAttribute, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[1]);
	glBufferData(GL_ARRAY_BUFFER, CAPSULE_BATCH_SIZE * sizeof(solid_capsules_data_t), NULL, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(transformInstance, 4, GL_FLOAT, GL_FALSE, sizeof(solid_capsules_data_t), (void *)offsetof(solid_capsules_data_t, transform));
	glVertexAttribPointer(radiusInstance, 1, GL_FLOAT, GL_FALSE, sizeof(solid_capsules_data_t), (void *)offsetof(solid_capsules_data_t, radius));
	glVertexAttribPointer(lengthInstance, 1, GL_FLOAT, GL_FALSE, sizeof(solid_capsules_data_t), (void *)offsetof(solid_capsules_data_t, length));
	glVertexAttribPointer(colorInstance, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(solid_capsules_data_t), (void *)offsetof(solid_capsules_data_t, rgba));

	glVertexAttribDivisor(transformInstance, 1);
	glVertexAttribDivisor(radiusInstance, 1);
	glVertexAttribDivisor(lengthInstance, 1);
	glVertexAttribDivisor(colorInstance, 1);

	CheckOpenGL();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return render;
}

void solid_capsules_destroy(solid_capsules_t *render)
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

	ecs_vec_fini_t(NULL, &render->capsules, solid_capsules_data_t);
	free(render);
}

void solid_capsules_add(solid_capsules_t *render, draw_vec2_t p1, draw_vec2_t p2, float radius, draw_color_t c)
{
	draw_vec2_t d      = draw_vec2_sub(p2, p1);
	float       length = draw_vec2_length(d);
	if (length < 0.001f) {
		printf("WARNING: sample app: capsule too short!\n");
		return;
	}

	draw_vec2_t      axis = {d.x / length, d.y / length};
	draw_transform_t transform;
	transform.p   = draw_vec2_lerp(p1, p2, 0.5f);
	transform.q.c = axis.x;
	transform.q.s = axis.y;

	RGBA8 rgba                                                          = MakeRGBA8(c, 1.0f);
	ecs_vec_append_t(NULL, &render->capsules, solid_capsules_data_t)[0] = (solid_capsules_data_t){transform, radius, length, rgba};
}

void solid_capsules_flush(solid_capsules_t *render, float pixelScale, const float *projectionMatrix)
{
	solid_capsules_data_t *capsules = ecs_vec_first_t(&render->capsules, solid_capsules_data_t);
	int32_t                count    = ecs_vec_count(&render->capsules);
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
		int batchCount = draw_min_int(count, CAPSULE_BATCH_SIZE);
		glBufferSubData(GL_ARRAY_BUFFER, 0, batchCount * sizeof(solid_capsules_data_t), capsules + base);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, batchCount);

		CheckOpenGL();

		count -= CAPSULE_BATCH_SIZE;
		base += CAPSULE_BATCH_SIZE;
	}

	glDisable(GL_BLEND);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);

	ecs_vec_clear(&render->capsules);
}
