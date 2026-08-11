#include "egg.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../draw/src/stb_truetype.h"

#define EGG_BATCH_VERTEX_COUNT (6 * 1024)
#define EGG_FIRST_CHAR         32
#define EGG_ATLAS_WIDTH        512
#define EGG_ATLAS_HEIGHT       512
#define EGG_BAKE_FONT_SIZE     32.0f
#define EGG_TRANSFORM_CAPACITY 1024
#define EGG_CHAR_COUNT         96
#define EGG_PI                 3.14159265358979323846f

typedef struct {
	float x;
	float y;
	float c;
	float s;
} egg_instance_transform_t;

typedef struct {
	float   position[2];
	float   instanceIndex;
	float   uv[2];
	float   useTexture;
	uint8_t rgba[4];
} egg_vertex_t;

typedef struct {
	egg_vertex_t *data;
	int32_t       count;
	int32_t       capacity;
} egg_vertex_buffer_t;

typedef struct {
	egg_instance_transform_t *data;
	int32_t                   count;
	int32_t                   capacity;
} egg_transform_buffer_t;

typedef struct egg_t {
	egg_vertex_buffer_t    vertices;
	egg_transform_buffer_t transforms;
	stbtt_bakedchar        glyphs[EGG_CHAR_COUNT];
	GLuint                 vaoId;
	GLuint                 vboId;
	GLuint                 transformBufferId;
	GLuint                 transformTextureId;
	GLuint                 atlasTextureId;
	GLuint                 programId;
	GLint                  projectionUniform;
	GLint                  atlasUniform;
	GLint                  transformUniform;
	float                  lineHeight;
	float                  pixelScale;
	int                    initialized;
} egg_t;

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
"    p = vec2((c * p.x + s * p.y) + x, (-s * p.x + c * p.y) + y);\n"
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

static void *sGrowBuffer(void *buffer, int32_t *capacity, int32_t count, size_t elementSize)
{
	if (count <= *capacity) {
		return buffer;
	}

	int32_t newCapacity = (*capacity == 0) ? 64 : *capacity;
	while (newCapacity < count) {
		newCapacity *= 2;
	}

	void *newBuffer = realloc(buffer, (size_t)newCapacity * elementSize);
	if (newBuffer == NULL) {
		return NULL;
	}

	*capacity = newCapacity;
	return newBuffer;
}

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

	for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
		unsigned char *fontData = sReadBinaryFile(candidates[i], outSize);
		if (fontData != NULL) {
			return fontData;
		}
	}

	return NULL;
}

static void sAppendVertex(egg_t *egg, float x, float y, float instanceIndex, float u, float v, float useTexture,
uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	egg_vertex_t *vertices = sGrowBuffer(egg->vertices.data, &egg->vertices.capacity, egg->vertices.count + 1,
	sizeof(egg_vertex_t));
	if (vertices == NULL) {
		return;
	}

	egg->vertices.data                                    = (egg_vertex_t *)vertices;
	egg->vertices.data[egg->vertices.count].position[0]   = x;
	egg->vertices.data[egg->vertices.count].position[1]   = y;
	egg->vertices.data[egg->vertices.count].instanceIndex = instanceIndex;
	egg->vertices.data[egg->vertices.count].uv[0]         = u;
	egg->vertices.data[egg->vertices.count].uv[1]         = v;
	egg->vertices.data[egg->vertices.count].useTexture    = useTexture;
	egg->vertices.data[egg->vertices.count].rgba[0]       = r;
	egg->vertices.data[egg->vertices.count].rgba[1]       = g;
	egg->vertices.data[egg->vertices.count].rgba[2]       = b;
	egg->vertices.data[egg->vertices.count].rgba[3]       = a;
	egg->vertices.count += 1;
}

static void sAppendTransform(egg_t *egg, float x, float y, float c, float s)
{
	egg_instance_transform_t *transforms = sGrowBuffer(egg->transforms.data, &egg->transforms.capacity,
	egg->transforms.count + 1, sizeof(egg_instance_transform_t));
	if (transforms == NULL) {
		return;
	}

	egg->transforms.data                          = transforms;
	egg->transforms.data[egg->transforms.count].x = x;
	egg->transforms.data[egg->transforms.count].y = y;
	egg->transforms.data[egg->transforms.count].c = c;
	egg->transforms.data[egg->transforms.count].s = s;
	egg->transforms.count += 1;
}

static void sAddQuad(egg_t *egg, float x0, float y0, float x1, float y1, float instanceIndex, float u0, float v0,
float u1, float v1, float useTexture, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	sAppendVertex(egg, x0, y0, instanceIndex, u0, v0, useTexture, r, g, b, a);
	sAppendVertex(egg, x1, y0, instanceIndex, u1, v0, useTexture, r, g, b, a);
	sAppendVertex(egg, x1, y1, instanceIndex, u1, v1, useTexture, r, g, b, a);
	sAppendVertex(egg, x0, y0, instanceIndex, u0, v0, useTexture, r, g, b, a);
	sAppendVertex(egg, x1, y1, instanceIndex, u1, v1, useTexture, r, g, b, a);
	sAppendVertex(egg, x0, y1, instanceIndex, u0, v1, useTexture, r, g, b, a);
}

static void sAddTriangle(egg_t *egg, float x0, float y0, float x1, float y1, float x2, float y2, float instanceIndex,
						 float useTexture, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	sAppendVertex(egg, x0, y0, instanceIndex, 0.0f, 0.0f, useTexture, r, g, b, a);
	sAppendVertex(egg, x1, y1, instanceIndex, 0.0f, 0.0f, useTexture, r, g, b, a);
	sAppendVertex(egg, x2, y2, instanceIndex, 0.0f, 0.0f, useTexture, r, g, b, a);
}

static void sAddGlyphQuad(egg_t *egg, stbtt_aligned_quad q, float instanceIndex, uint8_t r, uint8_t g, uint8_t b,
uint8_t a)
{
	sAddQuad(egg, q.x0, q.y0, q.x1, q.y1, instanceIndex, q.s0, q.t0, q.s1, q.t1, 1.0f, r, g, b, a);
}

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

egg_t *egg_init(void)
{
	egg_t *egg = (egg_t *)calloc(1, sizeof(egg_t));
	if (egg == NULL) {
		return NULL;
	}

	egg->lineHeight = EGG_BAKE_FONT_SIZE * 1.2f;
	egg->pixelScale = 1.0f;
	egg->programId  = sCreateProgram(kEggVertexShaderSource, kEggFragmentShaderSource);
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
	glBufferData(GL_ARRAY_BUFFER, EGG_BATCH_VERTEX_COUNT * sizeof(egg_vertex_t), NULL, GL_DYNAMIC_DRAW);

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

	size_t         fontSize = 0;
	unsigned char *fontData = sLoadSystemFont(&fontSize);
	if (fontData == NULL) {
		fprintf(stderr, "egg: failed to locate a default TrueType font\n");
		egg_destroy(egg);
		return NULL;
	}

	unsigned char *bitmap = (unsigned char *)malloc(EGG_ATLAS_WIDTH * EGG_ATLAS_HEIGHT);
	if (bitmap == NULL) {
		free(fontData);
		egg_destroy(egg);
		return NULL;
	}

	memset(bitmap, 0, EGG_ATLAS_WIDTH * EGG_ATLAS_HEIGHT);
	int rowUsed = stbtt_BakeFontBitmap(fontData, 0, EGG_BAKE_FONT_SIZE, bitmap, EGG_ATLAS_WIDTH, EGG_ATLAS_HEIGHT,
	EGG_FIRST_CHAR, EGG_CHAR_COUNT, egg->glyphs);
	free(fontData);
	if (rowUsed <= 0) {
		free(bitmap);
		egg_destroy(egg);
		return NULL;
	}

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
	free(bitmap);

	egg->initialized = 1;
	return egg;
}

void egg_destroy(egg_t *egg)
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

	free(egg->vertices.data);
	free(egg->transforms.data);
	free(egg);
}

void egg_set_pixel_scale(egg_t *egg, float pixelScale)
{
	if (egg == NULL) {
		return;
	}

	egg->pixelScale = pixelScale > 0.0f ? pixelScale : 1.0f;
}

static uint8_t sColorByte(egg_color_t color, int shift)
{
	return (uint8_t)((color >> shift) & 0xFF);
}

static void sColorBytes(egg_color_t color, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	*r = sColorByte(color, 16);
	*g = sColorByte(color, 8);
	*b = sColorByte(color, 0);
	*a = sColorByte(color, 24);
	if (*a == 0) {
		*a = 255;
	}
}

void egg_draw_text(egg_t *egg, float x, float y, float rotationCos, float rotationSin, float fontSize,
egg_color_t color, const char *string)
{
	if (egg == NULL || egg->initialized == 0 || string == NULL) {
		return;
	}

	float scale = fontSize / EGG_BAKE_FONT_SIZE;
	if (scale <= 0.0f) {
		return;
	}

	float   cursorX = 0.0f;
	float   cursorY = 0.0f;
	float   startX  = 0.0f;
	uint8_t r       = 0;
	uint8_t g       = 0;
	uint8_t b       = 0;
	uint8_t a       = 0;
	sColorBytes(color, &r, &g, &b, &a);

	sAppendTransform(egg, x, y, rotationCos, rotationSin);
	float instanceIndex = (float)(egg->transforms.count - 1);

	for (const char *p = string; *p != '\0'; ++p) {
		int codepoint = (unsigned char)*p;
		if (codepoint == '\n') {
			cursorX = startX;
			cursorY -= egg->lineHeight * scale;
			continue;
		}

		if (codepoint == '\t') {
			cursorX += 4.0f * egg->lineHeight * 0.5f * scale;
			continue;
		}

		if (codepoint < EGG_FIRST_CHAR || codepoint >= EGG_FIRST_CHAR + EGG_CHAR_COUNT) {
			codepoint = '?';
		}

		stbtt_aligned_quad q;
		stbtt_GetBakedQuad(egg->glyphs, EGG_ATLAS_WIDTH, EGG_ATLAS_HEIGHT, codepoint - EGG_FIRST_CHAR, &cursorX,
		&cursorY, &q, 1);

		float dx0 = q.x0 - startX;
		float dy0 = q.y0 - cursorY;
		float dx1 = q.x1 - startX;
		float dy1 = q.y1 - cursorY;
		q.x0      = startX + scale * dx0;
		q.y0      = cursorY - scale * dy0;
		q.x1      = startX + scale * dx1;
		q.y1      = cursorY - scale * dy1;

		sAddGlyphQuad(egg, q, instanceIndex, r, g, b, a);
	}
}

static void sAddLine(egg_t *egg, float x1, float y1, float x2, float y2, float thickness, float instanceIndex,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	if (egg == NULL || thickness <= 0.0f) {
		return;
	}

	float dx = x2 - x1;
	float dy = y2 - y1;
	float length = sqrtf(dx * dx + dy * dy);
	if (length <= 0.0f) {
		return;
	}

	float scaledThickness = thickness * egg->pixelScale;
	float halfThickness = scaledThickness * 0.5f;
	float nx = -dy / length * halfThickness;
	float ny = dx / length * halfThickness;

	float p1x = x1 + nx;
	float p1y = y1 + ny;
	float p2x = x1 - nx;
	float p2y = y1 - ny;
	float p3x = x2 + nx;
	float p3y = y2 + ny;
	float p4x = x2 - nx;
	float p4y = y2 - ny;

	sAddTriangle(egg, p1x, p1y, p2x, p2y, p3x, p3y, instanceIndex, 0.0f, r, g, b, a);
	sAddTriangle(egg, p2x, p2y, p4x, p4y, p3x, p3y, instanceIndex, 0.0f, r, g, b, a);
}

void egg_draw_line(egg_t *egg, float x1, float y1, float x2, float y2, float thickness, egg_color_t color)
{
	if (egg == NULL || egg->initialized == 0) {
		return;
	}

	sAppendTransform(egg, 0.0f, 0.0f, 1.0f, 0.0f);
	float instanceIndex = (float)(egg->transforms.count - 1);
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0;
	sColorBytes(color, &r, &g, &b, &a);

	sAddLine(egg, x1, y1, x2, y2, thickness, instanceIndex, r, g, b, a);
}

static void sAddCircleFilled(egg_t *egg, float x, float y, float radius, float instanceIndex, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	if (egg == NULL || radius <= 0.0f) {
		return;
	}

	int segments = 32;
	for (int i = 0; i < segments; ++i) {
		float angle0 = (float)i * (2.0f * EGG_PI / (float)segments);
		float angle1 = (float)(i + 1) * (2.0f * EGG_PI / (float)segments);

		float x0 = x + cosf(angle0) * radius;
		float y0 = y + sinf(angle0) * radius;
		float x1 = x + cosf(angle1) * radius;
		float y1 = y + sinf(angle1) * radius;

		sAddTriangle(egg, x, y, x0, y0, x1, y1, instanceIndex, 0.0f, r, g, b, a);
	}
}

void egg_draw_point(egg_t *egg, float x, float y, float size, egg_color_t color)
{
	if (egg == NULL || egg->initialized == 0 || size <= 0.0f) {
		return;
	}

	sAppendTransform(egg, x, y, 1.0f, 0.0f);
	float instanceIndex = (float)(egg->transforms.count - 1);
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0;
	sColorBytes(color, &r, &g, &b, &a);

	float scaledSize = size * egg->pixelScale;
	sAddQuad(egg, -scaledSize, -scaledSize, scaledSize, scaledSize, instanceIndex, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
		r, g, b, a);
}

void egg_draw_circle(egg_t *egg, float x, float y, float radius, egg_color_t color)
{
	if (egg == NULL || egg->initialized == 0 || radius <= 0.0f) {
		return;
	}

	sAppendTransform(egg, x, y, 1.0f, 0.0f);
	float instanceIndex = (float)(egg->transforms.count - 1);
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0;
	sColorBytes(color, &r, &g, &b, &a);

	sAddCircleFilled(egg, 0.0f, 0.0f, radius, instanceIndex, r, g, b, a);
}

void egg_draw_circle_outline(egg_t *egg, float x, float y, float radius, float thickness, egg_color_t color)
{
	if (egg == NULL || egg->initialized == 0 || radius <= 0.0f || thickness <= 0.0f) {
		return;
	}

	sAppendTransform(egg, 0.0f, 0.0f, 1.0f, 0.0f);
	float instanceIndex = (float)(egg->transforms.count - 1);
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0;
	sColorBytes(color, &r, &g, &b, &a);

	int segments = 32;
	float scaledThickness = thickness * egg->pixelScale;
	float innerRadius = radius - scaledThickness * 0.5f;
	float outerRadius = radius + scaledThickness * 0.5f;
	if (innerRadius < 0.0f) {
		innerRadius = 0.0f;
	}

	for (int i = 0; i < segments; ++i) {
		float angle0 = (float)i * (2.0f * EGG_PI / (float)segments);
		float angle1 = (float)(i + 1) * (2.0f * EGG_PI / (float)segments);

		float x0Outer = x + cosf(angle0) * outerRadius;
		float y0Outer = y + sinf(angle0) * outerRadius;
		float x1Outer = x + cosf(angle1) * outerRadius;
		float y1Outer = y + sinf(angle1) * outerRadius;
		float x0Inner = x + cosf(angle0) * innerRadius;
		float y0Inner = y + sinf(angle0) * innerRadius;
		float x1Inner = x + cosf(angle1) * innerRadius;
		float y1Inner = y + sinf(angle1) * innerRadius;

		sAddTriangle(egg, x0Outer, y0Outer, x1Outer, y1Outer, x1Inner, y1Inner, instanceIndex, 0.0f,
			r, g, b, a);
		sAddTriangle(egg, x0Outer, y0Outer, x1Inner, y1Inner, x0Inner, y0Inner, instanceIndex, 0.0f,
			r, g, b, a);
	}
}

void egg_draw_capsule_outline(egg_t *egg, float x1, float y1, float x2, float y2, float radius, float thickness,
	egg_color_t color)
{
	if (egg == NULL || egg->initialized == 0 || radius <= 0.0f || thickness <= 0.0f) {
		return;
	}

	sAppendTransform(egg, 0.0f, 0.0f, 1.0f, 0.0f);
	float instanceIndex = (float)(egg->transforms.count - 1);
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0;
	sColorBytes(color, &r, &g, &b, &a);

	float dx = x2 - x1;
	float dy = y2 - y1;
	float length = sqrtf(dx * dx + dy * dy);
	if (length <= 0.0f) {
		return;
	}

	float nx = dx / length;
	float ny = dy / length;

	float halfLength = length * 0.5f;
	float centerX = (x1 + x2) * 0.5f;
	float centerY = (y1 + y2) * 0.5f;

	sAddLine(egg, x1, y1, x2, y2, thickness, instanceIndex, r, g, b, a);

	int segments = 24;
	for (int i = 0; i < segments; ++i) {
		float t0 = (float)i / (float)segments;
		float t1 = (float)(i + 1) / (float)segments;
		float angle0 = EGG_PI + t0 * EGG_PI;
		float angle1 = EGG_PI + t1 * EGG_PI;

		float ox0 = centerX + nx * halfLength + cosf(angle0) * radius;
		float oy0 = centerY + ny * halfLength + sinf(angle0) * radius;
		float ox1 = centerX + nx * halfLength + cosf(angle1) * radius;
		float oy1 = centerY + ny * halfLength + sinf(angle1) * radius;
		float scaledThickness = thickness * egg->pixelScale;
		float ix0 = centerX + nx * halfLength + cosf(angle0) * (radius - scaledThickness * 0.5f);
		float iy0 = centerY + ny * halfLength + sinf(angle0) * (radius - scaledThickness * 0.5f);
		float ix1 = centerX + nx * halfLength + cosf(angle1) * (radius - scaledThickness * 0.5f);
		float iy1 = centerY + ny * halfLength + sinf(angle1) * (radius - scaledThickness * 0.5f);

		sAddTriangle(egg, ox0, oy0, ox1, oy1, ix1, iy1, instanceIndex, 0.0f, r, g, b, a);
		sAddTriangle(egg, ox0, oy0, ix1, iy1, ix0, iy0, instanceIndex, 0.0f, r, g, b, a);
	}

	for (int i = 0; i < segments; ++i) {
		float t0 = (float)i / (float)segments;
		float t1 = (float)(i + 1) / (float)segments;
		float angle0 = 0.0f + t0 * EGG_PI;
		float angle1 = 0.0f + t1 * EGG_PI;

		float ox0 = centerX - nx * halfLength + cosf(angle0) * radius;
		float oy0 = centerY - ny * halfLength + sinf(angle0) * radius;
		float ox1 = centerX - nx * halfLength + cosf(angle1) * radius;
		float oy1 = centerY - ny * halfLength + sinf(angle1) * radius;
		float scaledThickness = thickness * egg->pixelScale;
		float ix0 = centerX - nx * halfLength + cosf(angle0) * (radius - scaledThickness * 0.5f);
		float iy0 = centerY - ny * halfLength + sinf(angle0) * (radius - scaledThickness * 0.5f);
		float ix1 = centerX - nx * halfLength + cosf(angle1) * (radius - scaledThickness * 0.5f);
		float iy1 = centerY - ny * halfLength + sinf(angle1) * (radius - scaledThickness * 0.5f);

		sAddTriangle(egg, ox0, oy0, ox1, oy1, ix1, iy1, instanceIndex, 0.0f, r, g, b, a);
		sAddTriangle(egg, ox0, oy0, ix1, iy1, ix0, iy0, instanceIndex, 0.0f, r, g, b, a);
	}
}

void egg_draw_transform(egg_t *egg, float x, float y, float rotationCos, float rotationSin, float scale, egg_color_t color)
{
	if (egg == NULL || egg->initialized == 0) {
		return;
	}

	sAppendTransform(egg, x, y, rotationCos, rotationSin);
	float instanceIndex = (float)(egg->transforms.count - 1);
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0;
	sColorBytes(color, &r, &g, &b, &a);

	float scaledThickness = 0.05f * egg->pixelScale;
	sAddLine(egg, 0.0f, 0.0f, scale, 0.0f, scaledThickness, instanceIndex, r, g, b, a);
	sAddLine(egg, 0.0f, 0.0f, 0.0f, scale, scaledThickness, instanceIndex, r, g, b, a);
}

void egg_draw_rectangle(egg_t *egg, float x, float y, float rotationCos, float rotationSin, float width, float height,
egg_color_t color)
{
	if (egg == NULL || egg->initialized == 0) {
		return;
	}

	if (width <= 0.0f || height <= 0.0f) {
		return;
	}

	sAppendTransform(egg, x, y, rotationCos, rotationSin);
	float   instanceIndex = (float)(egg->transforms.count - 1);
	float   halfWidth     = width * 0.5f;
	float   halfHeight    = height * 0.5f;
	uint8_t r             = 0;
	uint8_t g             = 0;
	uint8_t b             = 0;
	uint8_t a             = 0;
	sColorBytes(color, &r, &g, &b, &a);

	sAddQuad(egg, -halfWidth, -halfHeight, halfWidth, halfHeight, instanceIndex, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
	r, g, b, a);
}

void egg_draw_bounds(egg_t *egg, float minX, float minY, float maxX, float maxY, egg_color_t color)
{
	if (egg == NULL || egg->initialized == 0) {
		return;
	}

	sAppendTransform(egg, 0.0f, 0.0f, 1.0f, 0.0f);
	float instanceIndex = (float)(egg->transforms.count - 1);
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0;
	sColorBytes(color, &r, &g, &b, &a);

	float scaledThickness = 0.05f * egg->pixelScale;
	sAddLine(egg, minX, minY, maxX, minY, scaledThickness, instanceIndex, r, g, b, a);
	sAddLine(egg, maxX, minY, maxX, maxY, scaledThickness, instanceIndex, r, g, b, a);
	sAddLine(egg, maxX, maxY, minX, maxY, scaledThickness, instanceIndex, r, g, b, a);
	sAddLine(egg, minX, maxY, minX, minY, scaledThickness, instanceIndex, r, g, b, a);
}

void egg_draw_polygon(egg_t *egg, const egg_vec2_t *vertices, int vertex_count, float tx, float ty,
					 float rot_c, float rot_s, egg_color_t color)
{
	if (egg == NULL || egg->initialized == 0 || vertices == NULL || vertex_count < 3) {
		return;
	}

	sAppendTransform(egg, tx, ty, rot_c, rot_s);
	float   instanceIndex = (float)(egg->transforms.count - 1);
	uint8_t r             = sColorByte(color, 16);
	uint8_t g             = sColorByte(color, 8);
	uint8_t b             = sColorByte(color, 0);
	uint8_t a             = sColorByte(color, 24);
	if (a == 0) {
		a = 255;
	}

	for (int i = 1; i + 1 < vertex_count; ++i) {
		sAddTriangle(egg, vertices[0].x, vertices[0].y, vertices[i].x, vertices[i].y, vertices[i + 1].x, vertices[i + 1].y,
				instanceIndex, 0.0f, r, g, b, a);
	}
}

void egg_flush(egg_t *egg, const float *projectionMatrix)
{
	if (egg == NULL || egg->initialized == 0 || egg->vertices.count == 0) {
		egg->vertices.count   = 0;
		egg->transforms.count = 0;
		return;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram(egg->programId);
	glUniformMatrix4fv(egg->projectionUniform, 1, GL_FALSE, projectionMatrix);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, egg->atlasTextureId);
	glUniform1i(egg->atlasUniform, 0);

	glActiveTexture(GL_TEXTURE1);
	glBindBuffer(GL_TEXTURE_BUFFER, egg->transformBufferId);
	int32_t transformUploadCount = egg->transforms.count > 0 ? egg->transforms.count : 1;
	glBufferData(GL_TEXTURE_BUFFER, (GLsizeiptr)(transformUploadCount * sizeof(egg_instance_transform_t)),
	egg->transforms.count > 0 ? egg->transforms.data : NULL, GL_DYNAMIC_DRAW);
	glBindTexture(GL_TEXTURE_BUFFER, egg->transformTextureId);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, egg->transformBufferId);
	glUniform1i(egg->transformUniform, 1);

	glBindVertexArray(egg->vaoId);
	glBindBuffer(GL_ARRAY_BUFFER, egg->vboId);

	int32_t remaining = egg->vertices.count;
	int32_t base      = 0;
	while (remaining > 0) {
		int32_t batchCount = remaining < EGG_BATCH_VERTEX_COUNT ? remaining : EGG_BATCH_VERTEX_COUNT;
		glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(batchCount * sizeof(egg_vertex_t)), egg->vertices.data + base);
		glDrawArrays(GL_TRIANGLES, 0, batchCount);
		remaining -= batchCount;
		base += batchCount;
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

	egg->vertices.count   = 0;
	egg->transforms.count = 0;
}
