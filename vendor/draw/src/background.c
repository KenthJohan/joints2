// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "background.h"

#include "internal.h"

background_t *background_init(const draw_create_info_t *createInfo)
{
	background_t *background = malloc(sizeof(background_t));
	*background              = (background_t){0};

	background->programId         = CreateProgramFromStrings(createInfo->shaders[DRAW_SHADER_BACKGROUND_VERTEX],
	        createInfo->shaders[DRAW_SHADER_BACKGROUND_FRAGMENT]);
	background->timeUniform       = glGetUniformLocation(background->programId, "time");
	background->resolutionUniform = glGetUniformLocation(background->programId, "resolution");
	background->baseColorUniform  = glGetUniformLocation(background->programId, "baseColor");
	int vertexAttribute           = 0;

	glGenVertexArrays(1, &background->vaoId);
	glGenBuffers(1, &background->vboId);

	glBindVertexArray(background->vaoId);
	glEnableVertexAttribArray(vertexAttribute);

	b2Vec2 vertices[] = {{-1.0f, 1.0f}, {-1.0f, -1.0f}, {1.0f, 1.0f}, {1.0f, -1.0f}};
	glBindBuffer(GL_ARRAY_BUFFER, background->vboId);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(vertexAttribute, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

	CheckOpenGL();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return background;
}

void background_destroy(background_t *background)
{
	if (background == NULL) {
		return;
	}

	if (background->vaoId) {
		glDeleteVertexArrays(1, &background->vaoId);
		glDeleteBuffers(1, &background->vboId);
		background->vaoId = 0;
		background->vboId = 0;
	}

	if (background->programId) {
		glDeleteProgram(background->programId);
		background->programId = 0;
	}

	free(background);
}

void background_render(background_t *background, float width, float height)
{
	glUseProgram(background->programId);

	float time = (float)SDL_GetTicks() * 0.001f;
	time       = fmodf(time, 100.0f);

	glUniform1f(background->timeUniform, time);
	glUniform2f(background->resolutionUniform, width, height);
	glUniform3f(background->baseColorUniform, 0.2f, 0.2f, 0.2f);

	glBindVertexArray(background->vaoId);

	glBindBuffer(GL_ARRAY_BUFFER, background->vboId);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);
}
