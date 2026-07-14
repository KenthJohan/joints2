// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT


#include "text.h"

#include "internal.h"

#define TEXT_BATCH_VERTEX_COUNT (6 * 1024)
#define TEXT_FIRST_CHAR         32
#define TEXT_ATLAS_WIDTH        512
#define TEXT_ATLAS_HEIGHT       512
#define TEXT_BAKE_FONT_SIZE     32.0f


static unsigned char *sReadBinaryFile(const char *path, size_t *outSize)
{
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		return NULL;
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return NULL;
	}

	long size = ftell(file);
	if (size <= 0 || fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return NULL;
	}

	unsigned char *bytes = malloc((size_t)size);
	if (bytes == NULL) {
		fclose(file);
		return NULL;
	}

	if (fread(bytes, 1, (size_t)size, file) != (size_t)size) {
		fclose(file);
		free(bytes);
		return NULL;
	}

	fclose(file);
	*outSize = (size_t)size;
	return bytes;
}

static unsigned char *sLoadSystemFont(size_t *outSize)
{
	const char *candidates[] = {
	"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
	"/usr/share/fonts/TTF/DejaVuSans.ttf",
	"/usr/share/fonts/dejavu/DejaVuSans.ttf",
	"C:/Windows/Fonts/arial.ttf",
	"/System/Library/Fonts/Supplemental/Arial.ttf",
	};

	for (int i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); ++i) {
		unsigned char *fontData = sReadBinaryFile(candidates[i], outSize);
		if (fontData != NULL) {
			return fontData;
		}
	}

	return NULL;
}

text_t *text_init(const draw_create_info_t *createInfo)
{
	text_t *render       = malloc(sizeof(text_t));
	*render                  = (text_t){0};
	ecs_vec_init_t(NULL, &render->vertices, text_vertex_t, TEXT_BATCH_VERTEX_COUNT);
	render->programId        = CreateProgramFromStrings(createInfo->shaders[DRAW_SHADER_TEXT_VERTEX],
	createInfo->shaders[DRAW_SHADER_TEXT_FRAGMENT]);
	render->projectionUniform = glGetUniformLocation(render->programId, "projectionMatrix");
	render->textureUniform    = glGetUniformLocation(render->programId, "glyphAtlas");
	render->lineHeight        = TEXT_BAKE_FONT_SIZE * 1.2f;

	if (render->programId == 0) {
		printf("WARNING: draw: failed to create text shader program\n");
		return render;
	}

	glGenVertexArrays(1, &render->vaoId);
	glGenBuffers(1, &render->vboId);

	glBindVertexArray(render->vaoId);
	glBindBuffer(GL_ARRAY_BUFFER, render->vboId);
	glBufferData(GL_ARRAY_BUFFER, TEXT_BATCH_VERTEX_COUNT * sizeof(text_vertex_t), NULL, GL_DYNAMIC_DRAW);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(text_vertex_t), (void *)offsetof(text_vertex_t, position));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(text_vertex_t), (void *)offsetof(text_vertex_t, uv));
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(text_vertex_t), (void *)offsetof(text_vertex_t, rgba));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	size_t         fontSize = 0;
	unsigned char *fontData = sLoadSystemFont(&fontSize);
	if (fontData == NULL) {
		printf("WARNING: draw: failed to locate a default TrueType font\n");
		return render;
	}

	unsigned char *bitmap = malloc(TEXT_ATLAS_WIDTH * TEXT_ATLAS_HEIGHT);
	if (bitmap == NULL) {
		free(fontData);
		return render;
	}

	int rowUsed = stbtt_BakeFontBitmap(fontData, 0, TEXT_BAKE_FONT_SIZE, bitmap, TEXT_ATLAS_WIDTH, TEXT_ATLAS_HEIGHT,
	TEXT_FIRST_CHAR, TEXT_CHAR_COUNT, render->glyphs);
	free(fontData);

	if (rowUsed <= 0) {
		printf("WARNING: draw: failed to bake text font atlas\n");
		free(bitmap);
		return render;
	}

	glGenTextures(1, &render->textureId);
	glBindTexture(GL_TEXTURE_2D, render->textureId);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, TEXT_ATLAS_WIDTH, TEXT_ATLAS_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	free(bitmap);
	render->initialized = 1;
	return render;
}

void text_destroy(text_t *render)
{
	if (render == NULL) {
		return;
	}

	if (render->vaoId) {
		glDeleteVertexArrays(1, &render->vaoId);
		glDeleteBuffers(1, &render->vboId);
	}

	if (render->textureId) {
		glDeleteTextures(1, &render->textureId);
	}

	if (render->programId) {
		glDeleteProgram(render->programId);
	}

	ecs_vec_fini_t(NULL, &render->vertices, text_vertex_t);
	free(render);
}

static void AddGlyphQuad(text_t *render, stbtt_aligned_quad q, RGBA8 rgba)
{
	ecs_vec_append_t(NULL, &render->vertices, text_vertex_t)[0] = (text_vertex_t){{q.x0, q.y0}, {q.s0, q.t0}, rgba};
	ecs_vec_append_t(NULL, &render->vertices, text_vertex_t)[0] = (text_vertex_t){{q.x1, q.y0}, {q.s1, q.t0}, rgba};
	ecs_vec_append_t(NULL, &render->vertices, text_vertex_t)[0] = (text_vertex_t){{q.x1, q.y1}, {q.s1, q.t1}, rgba};
	ecs_vec_append_t(NULL, &render->vertices, text_vertex_t)[0] = (text_vertex_t){{q.x0, q.y0}, {q.s0, q.t0}, rgba};
	ecs_vec_append_t(NULL, &render->vertices, text_vertex_t)[0] = (text_vertex_t){{q.x1, q.y1}, {q.s1, q.t1}, rgba};
	ecs_vec_append_t(NULL, &render->vertices, text_vertex_t)[0] = (text_vertex_t){{q.x0, q.y1}, {q.s0, q.t1}, rgba};
}

void text_add(text_t *render, float x, float y, float fontSize, b2HexColor color, const char *string)
{
	if (render->initialized == 0 || string == NULL) {
		return;
	}

	float scale = fontSize / TEXT_BAKE_FONT_SIZE;
	if (scale <= 0.0f) {
		return;
	}

	float startX  = x;
	float cursorX = x;
	float cursorY = y;
	RGBA8 rgba    = MakeRGBA8(color, 1.0f);

	for (const char *p = string; *p != '\0'; ++p) {
		int codepoint = (unsigned char)*p;
		if (codepoint == '\n') {
			cursorX = startX;
			cursorY -= render->lineHeight * scale;
			continue;
		}

		if (codepoint == '\t') {
			cursorX += 4.0f * render->lineHeight * 0.5f * scale;
			continue;
		}

		if (codepoint < TEXT_FIRST_CHAR || codepoint >= TEXT_FIRST_CHAR + TEXT_CHAR_COUNT) {
			codepoint = '?';
		}

		stbtt_aligned_quad q;
		stbtt_GetBakedQuad(render->glyphs, TEXT_ATLAS_WIDTH, TEXT_ATLAS_HEIGHT, codepoint - TEXT_FIRST_CHAR, &cursorX,
		&cursorY, &q, 1);

		float dx0 = q.x0 - x;
		float dy0 = q.y0 - y;
		float dx1 = q.x1 - x;
		float dy1 = q.y1 - y;
		q.x0      = x + scale * dx0;
		q.y0      = y - scale * dy0;
		q.x1      = x + scale * dx1;
		q.y1      = y - scale * dy1;

		AddGlyphQuad(render, q, rgba);
	}
}

void text_flush(text_t *render, const float *projectionMatrix)
{
	text_vertex_t *vertices = ecs_vec_first_t(&render->vertices, text_vertex_t);
	int32_t count = ecs_vec_count(&render->vertices);
	if (count == 0 || render->initialized == 0) {
		return;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram(render->programId);
	glUniformMatrix4fv(render->projectionUniform, 1, GL_FALSE, projectionMatrix);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, render->textureId);
	glUniform1i(render->textureUniform, 0);

	glBindVertexArray(render->vaoId);
	glBindBuffer(GL_ARRAY_BUFFER, render->vboId);

	int base = 0;
	while (count > 0) {
		int batchCount = b2MinInt(count, TEXT_BATCH_VERTEX_COUNT);
		glBufferSubData(GL_ARRAY_BUFFER, 0, batchCount * sizeof(text_vertex_t), vertices + base);
		glDrawArrays(GL_TRIANGLES, 0, batchCount);

		count -= TEXT_BATCH_VERTEX_COUNT;
		base += TEXT_BATCH_VERTEX_COUNT;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	glDisable(GL_BLEND);

	ecs_vec_clear(&render->vertices);
}
