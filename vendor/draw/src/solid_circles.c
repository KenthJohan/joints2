// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "solid_circles.h"

#include "internal.h"

#define SOLID_CIRCLE_BATCH_SIZE 2048

solid_circles_t *solid_circles_init(const draw_create_info_t *createInfo)
{
	solid_circles_t *render = malloc(sizeof(solid_circles_t));
	*render                 = (solid_circles_t){0};
	ecs_vec_init_t(NULL, &render->circles, solid_circles_data_t, SOLID_CIRCLE_BATCH_SIZE);
	render->programId         = CreateProgramFromStrings(createInfo->shaders[DRAW_SHADER_SOLID_CIRCLE_VERTEX],
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
	draw_vec2_t vertices[] = {{-a, -a}, {a, -a}, {-a, a}, {a, -a}, {a, a}, {-a, a}};
	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(vertexAttribute, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[1]);
	glBufferData(GL_ARRAY_BUFFER, SOLID_CIRCLE_BATCH_SIZE * sizeof(solid_circles_data_t), NULL, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(transformInstance, 4, GL_FLOAT, GL_FALSE, sizeof(solid_circles_data_t), (void *)offsetof(solid_circles_data_t, transform));
	glVertexAttribPointer(radiusInstance, 1, GL_FLOAT, GL_FALSE, sizeof(solid_circles_data_t), (void *)offsetof(solid_circles_data_t, radius));
	glVertexAttribPointer(colorInstance, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(solid_circles_data_t), (void *)offsetof(solid_circles_data_t, rgba));

	glVertexAttribDivisor(transformInstance, 1);
	glVertexAttribDivisor(radiusInstance, 1);
	glVertexAttribDivisor(colorInstance, 1);

	CheckOpenGL();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return render;
}

void solid_circles_destroy(solid_circles_t *render)
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

	ecs_vec_fini_t(NULL, &render->circles, solid_circles_data_t);
	free(render);
}

void solid_circles_add(solid_circles_t *render, draw_transform_t transform, float radius, draw_color_t color)
{
	RGBA8 rgba                                                        = MakeRGBA8(color, 1.0f);
	ecs_vec_append_t(NULL, &render->circles, solid_circles_data_t)[0] = (solid_circles_data_t){transform, radius, rgba};
}

void solid_circles_flush(solid_circles_t *render, float pixelScale, const float *projectionMatrix)
{
	solid_circles_data_t *circles = ecs_vec_first_t(&render->circles, solid_circles_data_t);
	int32_t               count   = ecs_vec_count(&render->circles);
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
		int batchCount = draw_min_int(count, SOLID_CIRCLE_BATCH_SIZE);
		glBufferSubData(GL_ARRAY_BUFFER, 0, batchCount * sizeof(solid_circles_data_t), circles + base);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, batchCount);

		CheckOpenGL();

		count -= SOLID_CIRCLE_BATCH_SIZE;
		base += SOLID_CIRCLE_BATCH_SIZE;
	}

	glDisable(GL_BLEND);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);

	ecs_vec_clear(&render->circles);
}
