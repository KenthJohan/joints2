#include "egg.h"

#include <EgDraws.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

typedef EgDraws_Vertex egg_vertex_t;

typedef struct egg_render_t {
	GLuint vaoId;
	GLuint vboId;
	GLuint atlasTextureId;
	GLuint programId;
	GLuint pickProgramId;
	GLint  projectionUniform;
	GLint  atlasUniform;
	GLint  pickProjectionUniform;
	GLint  pickAtlasUniform;
	int    initialized;
} egg_render_t;

struct egg_pick_target_t {
	GLuint framebuffer;
	GLuint texture;
	GLuint depth;
	int width;
	int height;
	int valid;
};

static const char *kEggVertexShaderSource =
"#version 330\n"
"uniform mat4 projectionMatrix;\n"
"layout(location = 0) in vec2 v_position;\n"
"layout(location = 1) in vec2 v_uv;\n"
"layout(location = 2) in vec4 v_color;\n"
"out vec2 f_uv;\n"
"flat out vec4 f_color;\n"
"void main(void)\n"
"{\n"
"    f_uv = v_uv;\n"
"    f_color = v_color;\n"
"    gl_Position = projectionMatrix * vec4(v_position, 0.0, 1.0);\n"
"}\n";

static const char *kEggFragmentShaderSource =
"#version 330\n"
"in vec2 f_uv;\n"
"flat in vec4 f_color;\n"
"uniform sampler2D atlasTexture;\n"
"out vec4 FragColor;\n"
"void main(void)\n"
"{\n"
"    float atlasAlpha = texture(atlasTexture, f_uv).r;\n"
"    FragColor = vec4(f_color.rgb, f_color.a * atlasAlpha);\n"
"}\n";

static const char *kEggPickVertexShaderSource =
"#version 330\n"
"uniform mat4 projectionMatrix;\n"
"layout(location = 0) in vec2 v_position;\n"
"layout(location = 1) in vec2 v_uv;\n"
"layout(location = 2) in vec4 v_color;\n"
"layout(location = 3) in uint v_id;\n"
"out vec2 f_uv;\n"
"flat out vec4 f_color;\n"
"flat out uint f_id;\n"
"void main(void) {\n"
"    f_uv = v_uv; f_color = v_color; f_id = v_id;\n"
"    gl_Position = projectionMatrix * vec4(v_position, 0.0, 1.0);\n"
"}\n";

static const char *kEggPickFragmentShaderSource =
"#version 330\n"
"in vec2 f_uv;\n"
"flat in vec4 f_color;\n"
"flat in uint f_id;\n"
"uniform sampler2D atlasTexture;\n"
"layout(location = 0) out uint pickId;\n"
"void main(void) {\n"
"    if (f_id == 0u || f_color.a * texture(atlasTexture, f_uv).r <= 0.0) discard;\n"
"    pickId = f_id;\n"
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, EG_DRAWS_ATLAS_WIDTH, EG_DRAWS_ATLAS_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
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
	egg->pickProgramId = sCreateProgram(kEggPickVertexShaderSource, kEggPickFragmentShaderSource);
	if (egg->pickProgramId == 0) {
		fprintf(stderr, "egg: failed to create picking shader program\n");
		egg_render_destroy(egg);
		return NULL;
	}

	egg->projectionUniform = glGetUniformLocation(egg->programId, "projectionMatrix");
	egg->atlasUniform      = glGetUniformLocation(egg->programId, "atlasTexture");
	egg->pickProjectionUniform = glGetUniformLocation(egg->pickProgramId, "projectionMatrix");
	egg->pickAtlasUniform = glGetUniformLocation(egg->pickProgramId, "atlasTexture");

	glGenVertexArrays(1, &egg->vaoId);
	glGenBuffers(1, &egg->vboId);

	glBindVertexArray(egg->vaoId);
	glBindBuffer(GL_ARRAY_BUFFER, egg->vboId);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(egg_vertex_t), (void *)offsetof(egg_vertex_t, position));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(egg_vertex_t), (void *)offsetof(egg_vertex_t, uv));
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(egg_vertex_t), (void *)offsetof(egg_vertex_t, rgba));
	glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(egg_vertex_t), (void *)offsetof(egg_vertex_t, pick_id));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	const unsigned char *bitmap = EgDraws_Font_GetBitmap();
	if (bitmap == NULL) {
		egg_render_destroy(egg);
		return NULL;
	}

	sUploadAtlas(egg, bitmap);

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
	if (egg->atlasTextureId != 0) {
		glDeleteTextures(1, &egg->atlasTextureId);
	}
	if (egg->programId != 0) {
		glDeleteProgram(egg->programId);
	}
	if (egg->pickProgramId != 0) {
		glDeleteProgram(egg->pickProgramId);
	}

	free(egg);
}

void egg_draw_layers(egg_render_t *egg, const ecs_vec_t *layers, const float *projectionMatrix)
{
	if (egg == NULL || egg->initialized == 0 || layers == NULL) {
		return;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram(egg->programId);
	glUniformMatrix4fv(egg->projectionUniform, 1, GL_FALSE, projectionMatrix);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, egg->atlasTextureId);
	glUniform1i(egg->atlasUniform, 0);

	// Bind the VAO and VBO for this layer.
	glBindVertexArray(egg->vaoId);
	glBindBuffer(GL_ARRAY_BUFFER, egg->vboId);

	// Layers are drawn in index order so higher z lands on top.
	const ecs_vec_t *l = ecs_vec_first_t(layers, ecs_vec_t);

	// Iterate over each layer and draw its vertices.
	for (int32_t i = 0; i < ecs_vec_count(layers); ++i) {
		const ecs_vec_t *v = &l[i];            // Current layer
		const int32_t    n = ecs_vec_count(v); // Number of vertices in this layer

		// Skip empty layers.
		if (n == 0) {
			continue;
		}

		// Upload vertex data for this layer to the GPU.
		const GLsizeiptr size = (GLsizeiptr)(n * sizeof(egg_vertex_t));
		const void      *data = ecs_vec_first(v);
		glBufferData(GL_ARRAY_BUFFER, size, data, GL_DYNAMIC_DRAW);

		// Draw (nv) amount of vertices for this layer as triangles.
		glDrawArrays(GL_TRIANGLES, 0, n);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	glDisable(GL_BLEND);
}

egg_pick_target_t *egg_pick_target_create(void)
{
	egg_pick_target_t *target = calloc(1, sizeof(*target));
	if (target == NULL) {
		fprintf(stderr, "egg: failed to allocate picking target\n");
		return NULL;
	}
	glGenFramebuffers(1, &target->framebuffer);
	glGenTextures(1, &target->texture);
	glGenRenderbuffers(1, &target->depth);
	if (!target->framebuffer || !target->texture || !target->depth) {
		fprintf(stderr, "egg: failed to create picking framebuffer attachments\n");
		egg_pick_target_destroy(target);
		return NULL;
	}
	return target;
}

void egg_pick_target_destroy(egg_pick_target_t *target)
{
	if (target == NULL) {
		return;
	}
	if (target->framebuffer) glDeleteFramebuffers(1, &target->framebuffer);
	if (target->texture) glDeleteTextures(1, &target->texture);
	if (target->depth) glDeleteRenderbuffers(1, &target->depth);
	free(target);
}

bool egg_pick_target_begin(egg_pick_target_t *target, int width, int height)
{
	if (!target || width <= 0 || height <= 0) {
		if (target) target->valid = 0;
		return false;
	}
	GLint old_draw, old_texture, old_unpack, old_renderbuffer;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
	glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &old_unpack);
	glGetIntegerv(GL_RENDERBUFFER_BINDING, &old_renderbuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target->framebuffer);
	if (width != target->width || height != target->height) {
		glBindTexture(GL_TEXTURE_2D, target->texture);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target->texture, 0);
		glBindRenderbuffer(GL_RENDERBUFFER, target->depth);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
		glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target->depth);
		target->width = width;
		target->height = height;
	}
	GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "egg: picking framebuffer incomplete: 0x%x\n", (unsigned)status);
		target->valid = 0;
		target->width = target->height = 0;
	} else {
		GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
		GLboolean mask[4];
		GLboolean depth_mask;
		glGetBooleanv(GL_COLOR_WRITEMASK, mask);
		glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
		glDisable(GL_SCISSOR_TEST);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);
		const GLuint zero[4] = {0, 0, 0, 0};
		const GLfloat depth = 1.0f;
		glClearBufferuiv(GL_COLOR, 0, zero);
		glClearBufferfv(GL_DEPTH, 0, &depth);
		glColorMask(mask[0], mask[1], mask[2], mask[3]);
		glDepthMask(depth_mask);
		if (scissor) glEnable(GL_SCISSOR_TEST);
		target->valid = 1;
	}
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		fprintf(stderr, "egg: OpenGL error preparing picking target: 0x%x\n", (unsigned)error);
		target->valid = 0;
		target->width = target->height = 0;
	}
	glBindTexture(GL_TEXTURE_2D, (GLuint)old_texture);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)old_unpack);
	glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)old_renderbuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)old_draw);
	return target->valid != 0;
}

uint32_t egg_pick_target_framebuffer(const egg_pick_target_t *target)
{
	return target && target->valid ? target->framebuffer : 0;
}

bool egg_pick_draw_layers(egg_pick_target_t *target, egg_render_t *egg,
	const ecs_vec_t *layers, const float *projectionMatrix, egg_pick_filter_t filter, void *ctx)
{
	if (!target || !target->valid || !egg || !egg->initialized || !layers || !projectionMatrix || !filter) {
		fprintf(stderr, "egg: invalid picking draw arguments\n");
		return false;
	}
	GLint framebuffer, viewport[4], program, vao, buffer, active, texture;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetIntegerv(GL_CURRENT_PROGRAM, &program);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
	GLboolean blend = glIsEnabled(GL_BLEND);
	GLboolean depth = glIsEnabled(GL_DEPTH_TEST);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target->framebuffer);
	glViewport(0, 0, target->width, target->height);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glUseProgram(egg->pickProgramId);
	glUniformMatrix4fv(egg->pickProjectionUniform, 1, GL_FALSE, projectionMatrix);
	glBindTexture(GL_TEXTURE_2D, egg->atlasTextureId);
	glUniform1i(egg->pickAtlasUniform, 0);
	glBindVertexArray(egg->vaoId);
	glBindBuffer(GL_ARRAY_BUFFER, egg->vboId);

	bool success = true;
	const ecs_vec_t *l = ecs_vec_first_t(layers, ecs_vec_t);
	for (int32_t i = 0; i < ecs_vec_count(layers); ++i) {
		int32_t n = ecs_vec_count(&l[i]);
		if (!n) continue;
		size_t size = (size_t)n * sizeof(egg_vertex_t);
		egg_vertex_t *vertices = malloc(size);
		if (!vertices) {
			fprintf(stderr, "egg: failed to allocate picking vertices\n");
			success = false;
			break;
		}
		memcpy(vertices, ecs_vec_first(&l[i]), size);
		for (int32_t j = 0; j < n; ++j) {
			if (vertices[j].pick_id && !filter(vertices[j].pick_id, ctx)) {
				vertices[j].pick_id = 0;
			}
		}
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)size, vertices, GL_DYNAMIC_DRAW);
		glDrawArrays(GL_TRIANGLES, 0, n);
		free(vertices);
		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			fprintf(stderr, "egg: OpenGL error drawing picking target: 0x%x\n", (unsigned)error);
			success = false;
			break;
		}
	}
	if (!success) target->valid = 0;
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)framebuffer);
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	if (blend) glEnable(GL_BLEND);
	if (depth) glEnable(GL_DEPTH_TEST);
	glBindVertexArray((GLuint)vao);
	glBindBuffer(GL_ARRAY_BUFFER, (GLuint)buffer);
	glBindTexture(GL_TEXTURE_2D, (GLuint)texture);
	glActiveTexture((GLenum)active);
	glUseProgram((GLuint)program);
	return success;
}

bool egg_pick_target_read(egg_pick_target_t *target, int x, int y, uint32_t *id)
{
	if (!target || !target->valid || !id || x < 0 || y < 0 || x >= target->width || y >= target->height) {
		return false;
	}
	GLint framebuffer, pack, alignment, row_length, skip_rows, skip_pixels, swap_bytes;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &framebuffer);
	glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
	glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
	glGetIntegerv(GL_PACK_ROW_LENGTH, &row_length);
	glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
	glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
	glGetIntegerv(GL_PACK_SWAP_BYTES, &swap_bytes);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, target->framebuffer);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
	glReadPixels(x, target->height - 1 - y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, id);
	glPixelStorei(GL_PACK_ALIGNMENT, alignment);
	glPixelStorei(GL_PACK_ROW_LENGTH, row_length);
	glPixelStorei(GL_PACK_SKIP_ROWS, skip_rows);
	glPixelStorei(GL_PACK_SKIP_PIXELS, skip_pixels);
	glPixelStorei(GL_PACK_SWAP_BYTES, swap_bytes);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)pack);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)framebuffer);
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		fprintf(stderr, "egg: OpenGL error reading picking target: 0x%x\n", (unsigned)error);
		target->valid = 0;
		return false;
	}
	return true;
}
