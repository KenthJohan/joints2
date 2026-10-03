#include "egg.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "draw.h"

#define EGG_TRANSFORM_CAPACITY 1024

typedef struct egg_render_t {
	GLuint     vaoId;
	GLuint     vboId;
	GLuint     transformBufferId;
	GLuint     transformTextureId;
	GLuint     atlasTextureId;
	GLuint     programId;
	GLint      projectionUniform;
	GLint      atlasUniform;
	GLint      transformUniform;
	int        initialized;
} egg_render_t;

static const char *kEggVertexShaderSource =
"#version 330\n"
"uniform mat4 projectionMatrix;\n"
"uniform samplerBuffer transformBuffer;\n"
"layout(location = 0) in vec2 v_position;\n"
"layout(location = 1) in float v_instanceIndex;\n"
"layout(location = 2) in vec2 v_uv;\n"
"layout(location = 3) in float v_useTexture;\n"
"layout(location = 4) in vec4 v_color;\n"
"out vec2 f_uv;\n"
"out vec4 f_color;\n"
"out float f_useTexture;\n"
"void main(void)\n"
"{\n"
"    f_uv = v_uv;\n"
"    f_color = v_color;\n"
"    f_useTexture = v_useTexture;\n"
"    vec4 instanceTransform = texelFetch(transformBuffer, int(v_instanceIndex + 0.5));\n"
"    float x = instanceTransform.x;\n"
"    float y = instanceTransform.y;\n"
"    float c = instanceTransform.z;\n"
"    float s = instanceTransform.w;\n"
"    vec2 p = vec2(v_position.x, v_position.y);\n"
"    p = vec2((c * p.x - s * p.y) + x, (s * p.x + c * p.y) + y);\n"
"    gl_Position = projectionMatrix * vec4(p, 0.0f, 1.0f);\n"
"}\n";

static const char *kEggFragmentShaderSource =
"#version 330\n"
"in vec2 f_uv;\n"
"in vec4 f_color;\n"
"in float f_useTexture;\n"
"uniform sampler2D atlasTexture;\n"
"out vec4 FragColor;\n"
"void main(void)\n"
"{\n"
"    if (f_useTexture > 0.5) {\n"
"        vec4 atlasSample = texture(atlasTexture, f_uv);\n"
"        FragColor = vec4(f_color.rgb, f_color.a * atlasSample.r);\n"
"    } else {\n"
"        FragColor = f_color;\n"
"    }\n"
"}\n";

static GLuint sCompileShader(GLenum type, const char *source)
{
	GLuint shader = glCreateShader(type);
	if (shader == 0) {
		return 0;
	}

	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == 0) {
		GLchar  log[2048];
		GLsizei length = 0;
		glGetShaderInfoLog(shader, sizeof(log), &length, log);
		fprintf(stderr, "egg: shader compile failed: %s\n", log);
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

static GLuint sCreateProgram(const char *vertexSource, const char *fragmentSource)
{
	GLuint vertexShader   = sCompileShader(GL_VERTEX_SHADER, vertexSource);
	GLuint fragmentShader = sCompileShader(GL_FRAGMENT_SHADER, fragmentSource);
	if (vertexShader == 0 || fragmentShader == 0) {
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		return 0;
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);

	GLint linked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (linked == 0) {
		GLchar  log[2048];
		GLsizei length = 0;
		glGetProgramInfoLog(program, sizeof(log), &length, log);
		fprintf(stderr, "egg: program link failed: %s\n", log);
		glDeleteProgram(program);
		program = 0;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	return program;
}

static void sUploadAtlas(egg_render_t *egg, const unsigned char *bitmap)
{
	glGenTextures(1, &egg->atlasTextureId);
	glBindTexture(GL_TEXTURE_2D, egg->atlasTextureId);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, EGG_ATLAS_WIDTH, EGG_ATLAS_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	unsigned char whitePixel[4] = {255, 255, 255, 255};
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
	glBindTexture(GL_TEXTURE_2D, 0);
}

egg_render_t *egg_render_init(void)
{
	egg_render_t *egg = (egg_render_t *)calloc(1, sizeof(egg_render_t));
	if (egg == NULL) {
		return NULL;
	}

	egg->programId = sCreateProgram(kEggVertexShaderSource, kEggFragmentShaderSource);
	if (egg->programId == 0) {
		fprintf(stderr, "egg: failed to create shader program\n");
		free(egg);
		return NULL;
	}

	egg->projectionUniform = glGetUniformLocation(egg->programId, "projectionMatrix");
	egg->atlasUniform      = glGetUniformLocation(egg->programId, "atlasTexture");
	egg->transformUniform  = glGetUniformLocation(egg->programId, "transformBuffer");

	glGenVertexArrays(1, &egg->vaoId);
	glGenBuffers(1, &egg->vboId);
	glGenBuffers(1, &egg->transformBufferId);
	glGenTextures(1, &egg->transformTextureId);

	glBindVertexArray(egg->vaoId);
	glBindBuffer(GL_ARRAY_BUFFER, egg->vboId);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(egg_vertex_t), (void *)offsetof(egg_vertex_t, position));
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(egg_vertex_t), (void *)offsetof(egg_vertex_t, instanceIndex));
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(egg_vertex_t), (void *)offsetof(egg_vertex_t, uv));
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(egg_vertex_t), (void *)offsetof(egg_vertex_t, useTexture));
	glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(egg_vertex_t), (void *)offsetof(egg_vertex_t, rgba));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glBindBuffer(GL_TEXTURE_BUFFER, egg->transformBufferId);
	glBufferData(GL_TEXTURE_BUFFER, EGG_TRANSFORM_CAPACITY * sizeof(egg_instance_transform_t), NULL, GL_DYNAMIC_DRAW);
	glBindTexture(GL_TEXTURE_BUFFER, egg->transformTextureId);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, egg->transformBufferId);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);

	egg_font_t     font;
	unsigned char *bitmap = (unsigned char *)malloc(EGG_ATLAS_WIDTH * EGG_ATLAS_HEIGHT);
	if (bitmap == NULL || !egg_font_bake(&font, bitmap)) {
		free(bitmap);
		egg_render_destroy(egg);
		return NULL;
	}

	sUploadAtlas(egg, bitmap);
	free(bitmap);

	egg->initialized = 1;
	return egg;
}

void egg_render_destroy(egg_render_t *egg)
{
	if (egg == NULL) {
		return;
	}

	if (egg->vaoId != 0) {
		glDeleteVertexArrays(1, &egg->vaoId);
	}
	if (egg->vboId != 0) {
		glDeleteBuffers(1, &egg->vboId);
	}
	if (egg->transformBufferId != 0) {
		glDeleteBuffers(1, &egg->transformBufferId);
	}
	if (egg->transformTextureId != 0) {
		glDeleteTextures(1, &egg->transformTextureId);
	}
	if (egg->atlasTextureId != 0) {
		glDeleteTextures(1, &egg->atlasTextureId);
	}
	if (egg->programId != 0) {
		glDeleteProgram(egg->programId);
	}

	free(egg);
}

void egg_flush(egg_render_t *egg, egg_draw_t *draw, const float *projectionMatrix)
{
	if (egg == NULL || egg->initialized == 0 || draw == NULL) {
		return;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram(egg->programId);
	glUniformMatrix4fv(egg->projectionUniform, 1, GL_FALSE, projectionMatrix);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, egg->atlasTextureId);
	glUniform1i(egg->atlasUniform, 0);

	// Lists are drawn in index order so higher z lands on top.
	for (int32_t li = 0; li < draw->listCount; ++li) {
		egg_drawlist_t *l = &draw->lists[li];
		if (l->vertices.count == 0) {
			l->transforms.count = 0;
			continue;
		}

		glActiveTexture(GL_TEXTURE1);
		glBindBuffer(GL_TEXTURE_BUFFER, egg->transformBufferId);
		int32_t transformUploadCount = l->transforms.count > 0 ? l->transforms.count : 1;
		glBufferData(GL_TEXTURE_BUFFER, (GLsizeiptr)(transformUploadCount * sizeof(egg_instance_transform_t)),
		l->transforms.count > 0 ? l->transforms.data : NULL, GL_DYNAMIC_DRAW);
		glBindTexture(GL_TEXTURE_BUFFER, egg->transformTextureId);
		glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, egg->transformBufferId);
		glUniform1i(egg->transformUniform, 1);

		glBindVertexArray(egg->vaoId);
		glBindBuffer(GL_ARRAY_BUFFER, egg->vboId);

		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(l->vertices.count * sizeof(egg_vertex_t)), l->vertices.data, GL_DYNAMIC_DRAW);
		glDrawArrays(GL_TRIANGLES, 0, l->vertices.count);

		l->vertices.count   = 0;
		l->transforms.count = 0;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	glDisable(GL_BLEND);
}
