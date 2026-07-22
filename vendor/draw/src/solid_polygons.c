// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "solid_polygons.h"

#include "internal.h"

#define POLYGON_BATCH_SIZE 2048

solid_polygons_t *solid_polygons_init(const draw_create_info_t *createInfo)
{
	solid_polygons_t *render = malloc(sizeof(solid_polygons_t));
	*render                  = (solid_polygons_t){0};
	ecs_vec_init_t(NULL, &render->polygons, solid_polygons_data_t, 10 * POLYGON_BATCH_SIZE);
	render->programId         = CreateProgramFromStrings(createInfo->shaders[DRAW_SHADER_SOLID_POLYGON_VERTEX],
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
	draw_vec2_t vertices[] = {{-a, -a}, {a, -a}, {-a, a}, {a, -a}, {a, a}, {-a, a}};
	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(vertexAttribute, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

	glBindBuffer(GL_ARRAY_BUFFER, render->vboIds[1]);
	glBufferData(GL_ARRAY_BUFFER, POLYGON_BATCH_SIZE * sizeof(solid_polygons_data_t), NULL, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(instanceTransform, 4, GL_FLOAT, GL_FALSE, sizeof(solid_polygons_data_t), (void *)offsetof(solid_polygons_data_t, transform));
	glVertexAttribPointer(instancePoint12, 4, GL_FLOAT, GL_FALSE, sizeof(solid_polygons_data_t), (void *)offsetof(solid_polygons_data_t, p1));
	glVertexAttribPointer(instancePoint34, 4, GL_FLOAT, GL_FALSE, sizeof(solid_polygons_data_t), (void *)offsetof(solid_polygons_data_t, p3));
	glVertexAttribPointer(instancePoint56, 4, GL_FLOAT, GL_FALSE, sizeof(solid_polygons_data_t), (void *)offsetof(solid_polygons_data_t, p5));
	glVertexAttribPointer(instancePoint78, 4, GL_FLOAT, GL_FALSE, sizeof(solid_polygons_data_t), (void *)offsetof(solid_polygons_data_t, p7));
	glVertexAttribIPointer(instancePointCount, 1, GL_INT, sizeof(solid_polygons_data_t), (void *)offsetof(solid_polygons_data_t, count));
	glVertexAttribPointer(instanceRadius, 1, GL_FLOAT, GL_FALSE, sizeof(solid_polygons_data_t), (void *)offsetof(solid_polygons_data_t, radius));
	glVertexAttribPointer(instanceColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(solid_polygons_data_t), (void *)offsetof(solid_polygons_data_t, color));

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

void solid_polygons_destroy(solid_polygons_t *render)
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

	ecs_vec_fini_t(NULL, &render->polygons, solid_polygons_data_t);
	free(render);
}

void solid_polygons_add(solid_polygons_t *render, draw_transform_t transform, const draw_vec2_t *points, int count, float radius, draw_color_t color)
{
	solid_polygons_data_t data = {};
	data.transform             = transform;

	int     n  = count < 8 ? count : 8;
	draw_vec2_t *ps = &data.p1;
	for (int i = 0; i < n; ++i) {
		ps[i] = points[i];
	}

	data.count  = n;
	data.radius = radius;
	data.color  = MakeRGBA8(color, 1.0f);

	ecs_vec_append_t(NULL, &render->polygons, solid_polygons_data_t)[0] = data;
}

void solid_polygons_flush(solid_polygons_t *render, float pixelScale, const float *projectionMatrix)
{
	solid_polygons_data_t *polygons = ecs_vec_first_t(&render->polygons, solid_polygons_data_t);
	int32_t                count    = ecs_vec_count(&render->polygons);
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
		int batchCount = draw_min_int(count, POLYGON_BATCH_SIZE);
		glBufferSubData(GL_ARRAY_BUFFER, 0, batchCount * sizeof(solid_polygons_data_t), polygons + base);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, batchCount);
		CheckOpenGL();

		count -= POLYGON_BATCH_SIZE;
		base += POLYGON_BATCH_SIZE;
	}

	glDisable(GL_BLEND);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);

	ecs_vec_clear(&render->polygons);
}
