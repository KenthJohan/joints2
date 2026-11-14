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
	GLuint asyncProgramId;
	GLuint eligibilityBuffer;
	uint32_t *eligibility;
	size_t eligibilityCapacity;
	GLint  projectionUniform;
	GLint  atlasUniform;
	GLint  pickProjectionUniform;
	GLint  pickAtlasUniform;
	GLint  pickOrderUniform;
	GLint  asyncProjectionUniform;
	GLint  asyncAtlasUniform;
	GLint  asyncMouseUniform;
	GLint  asyncOrderUniform;
	int    initialized;
} egg_render_t;

enum { EGG_PICK_SLOTS = 3 };
typedef struct {
	GLuint buffer;
	GLsync fence;
	uint64_t *entities;
	size_t count, capacity;
	uint64_t frame;
	bool displayed;
	bool invalidated;
} egg_pick_slot_t;

struct egg_pick_async_t {
	egg_pick_slot_t slots[EGG_PICK_SLOTS];
	int active;
	int x, y;
	uint32_t order;
};

typedef struct {
	GLint buffer;
	GLint64 start, size;
} egg_storage_binding_t;

static egg_storage_binding_t sStorageBinding(GLuint index)
{
	egg_storage_binding_t binding;
	glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, index, &binding.buffer);
	glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_START, index, &binding.start);
	glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE, index, &binding.size);
	return binding;
}

static void sRestoreStorageBinding(GLuint index, egg_storage_binding_t binding)
{
	if (!binding.buffer || binding.size <= 0) {
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, (GLuint)binding.buffer);
	} else {
		GLint64 buffer_size = 0;
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)binding.buffer);
		glGetBufferParameteri64v(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &buffer_size);
		if (!binding.start && binding.size == buffer_size) {
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, (GLuint)binding.buffer);
		} else {
			glBindBufferRange(GL_SHADER_STORAGE_BUFFER, index, (GLuint)binding.buffer,
				(GLintptr)binding.start, (GLsizeiptr)binding.size);
		}
	}
}

struct egg_pick_target_t {
	GLuint framebuffer;
	GLuint texture;
	GLuint depth;
	int width;
	int height;
	int valid;
	int region_x, region_y, region_w, region_h;
	int center_x, center_y;
	uint32_t next_order;
	bool region_mode;
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

static const char *kEggAsyncVertexShaderSource =
"#version 430\n"
"uniform mat4 projectionMatrix;\n"
"layout(location = 0) in vec2 v_position;\n"
"layout(location = 1) in vec2 v_uv;\n"
"layout(location = 2) in vec4 v_color;\n"
"out vec2 f_uv;\n"
"flat out vec4 f_color;\n"
"void main(void) {\n"
"    f_uv = v_uv; f_color = v_color;\n"
"    gl_Position = projectionMatrix * vec4(v_position, 0.0, 1.0);\n"
"}\n";

/* The high word orders nonnegative window depths; the inverted low word makes
 * the latest primitive win depth ties under atomicMin. */
static const char *kEggAsyncFragmentShaderSource =
"#version 430\n"
"#extension GL_ARB_gpu_shader_int64 : require\n"
"#extension GL_NV_shader_atomic_int64 : require\n"
"in vec2 f_uv;\n"
"flat in vec4 f_color;\n"
"uniform sampler2D atlasTexture;\n"
"uniform ivec2 mousePixel;\n"
"uniform uint orderBase;\n"
"layout(std430, binding = 0) buffer PickResult { uint64_t best; };\n"
"layout(std430, binding = 1) readonly buffer PickEligible { uint eligible[]; };\n"
"out vec4 FragColor;\n"
"void main(void) {\n"
"    float alpha = f_color.a * texture(atlasTexture, f_uv).r;\n"
"    FragColor = vec4(f_color.rgb, alpha);\n"
"    uint primitive = uint(gl_PrimitiveID);\n"
"    if (ivec2(gl_FragCoord.xy) == mousePixel && alpha > 0.0 && eligible[primitive] != 0u) {\n"
"        uint order = orderBase + primitive;\n"
"        uint64_t value = (uint64_t(floatBitsToUint(gl_FragCoord.z)) << 32) | uint64_t(~order);\n"
"        atomicMin(best, value);\n"
"    }\n"
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
"uniform uint orderBase;\n"
"layout(location = 0) out uvec2 pickValue;\n"
"void main(void) {\n"
"    if (f_id == 0u || f_color.a * texture(atlasTexture, f_uv).r <= 0.0) discard;\n"
"    pickValue = uvec2(f_id, orderBase + uint(gl_PrimitiveID));\n"
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
	egg->pickOrderUniform = glGetUniformLocation(egg->pickProgramId, "orderBase");

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
	if (egg->asyncProgramId != 0) glDeleteProgram(egg->asyncProgramId);
	if (egg->eligibilityBuffer != 0) glDeleteBuffers(1, &egg->eligibilityBuffer);
	free(egg->eligibility);

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

egg_pick_async_t *egg_pick_async_create(void)
{
	GLint major = 0, minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	if (major < 4 || (major == 4 && minor < 3)) {
		fprintf(stderr, "egg: asynchronous picking requires OpenGL 4.3\n");
		return NULL;
	}
	if (!SDL_GL_ExtensionSupported("GL_ARB_gpu_shader_int64") ||
		!SDL_GL_ExtensionSupported("GL_NV_shader_atomic_int64")) {
		fprintf(stderr, "egg: asynchronous picking requires GL_ARB_gpu_shader_int64 and GL_NV_shader_atomic_int64\n");
		return NULL;
	}
	GLint fragment_blocks = 0, bindings = 0;
	glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &fragment_blocks);
	glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &bindings);
	if (fragment_blocks < 2 || bindings < 2) {
		fprintf(stderr, "egg: asynchronous picking requires two fragment shader storage buffers\n");
		return NULL;
	}
	egg_pick_async_t *pick = calloc(1, sizeof(*pick));
	if (!pick) {
		fprintf(stderr, "egg: failed to allocate asynchronous picker\n");
		return NULL;
	}
	pick->active = -1;
	for (int i = 0; i < EGG_PICK_SLOTS; ++i) {
		glGenBuffers(1, &pick->slots[i].buffer);
		if (!pick->slots[i].buffer) {
			fprintf(stderr, "egg: failed to allocate picking buffer\n");
			egg_pick_async_destroy(pick);
			return NULL;
		}
	}
	return pick;
}

void egg_pick_async_destroy(egg_pick_async_t *pick)
{
	if (!pick) return;
	for (int i = 0; i < EGG_PICK_SLOTS; ++i) {
		if (pick->slots[i].fence) glDeleteSync(pick->slots[i].fence);
		if (pick->slots[i].buffer) glDeleteBuffers(1, &pick->slots[i].buffer);
		free(pick->slots[i].entities);
	}
	free(pick);
}

int egg_pick_async_begin(egg_pick_async_t *pick, int x, int y, uint64_t frame)
{
	if (!pick || pick->active >= 0 || x < 0 || y < 0) {
		fprintf(stderr, "egg: invalid asynchronous pick begin\n");
		return -1;
	}
	for (int i = 0; i < EGG_PICK_SLOTS; ++i) {
		egg_pick_slot_t *slot = &pick->slots[i];
		if (slot->fence) continue;
		if (!slot->buffer) {
			fprintf(stderr, "egg: picking buffer is unavailable\n");
			return -1;
		}
		GLint old;
		glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &old);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, slot->buffer);
		uint64_t empty = UINT64_MAX;
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(empty), &empty, GL_STREAM_READ);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)old);
		if (glGetError() != GL_NO_ERROR) {
			fprintf(stderr, "egg: failed to initialize picking buffer\n");
			return -1;
		}
		slot->count = 0;
		slot->frame = frame;
		slot->displayed = false;
		slot->invalidated = false;
		pick->active = i;
		pick->x = x;
		pick->y = y;
		pick->order = 0;
		return 1;
	}
	return 0;
}

bool egg_pick_async_draw(egg_pick_async_t *pick, egg_render_t *egg,
	const ecs_vec_t *layers, const float *projectionMatrix,
	egg_pick_resolve_t resolve, void *ctx)
{
	if (!pick || pick->active < 0 || !egg || !egg->initialized || !layers || !projectionMatrix || !resolve) {
		fprintf(stderr, "egg: invalid asynchronous pick draw\n");
		return false;
	}
	if (!egg->asyncProgramId) {
		egg->asyncProgramId = sCreateProgram(kEggAsyncVertexShaderSource, kEggAsyncFragmentShaderSource);
		if (!egg->asyncProgramId) {
			fprintf(stderr, "egg: failed to compile asynchronous picking shader\n");
			egg_draw_layers(egg, layers, projectionMatrix);
			return false;
		}
		egg->asyncProjectionUniform = glGetUniformLocation(egg->asyncProgramId, "projectionMatrix");
		egg->asyncAtlasUniform = glGetUniformLocation(egg->asyncProgramId, "atlasTexture");
		egg->asyncMouseUniform = glGetUniformLocation(egg->asyncProgramId, "mousePixel");
		egg->asyncOrderUniform = glGetUniformLocation(egg->asyncProgramId, "orderBase");
		glGenBuffers(1, &egg->eligibilityBuffer);
		if (!egg->eligibilityBuffer) {
			fprintf(stderr, "egg: failed to create picking eligibility buffer\n");
			glDeleteProgram(egg->asyncProgramId);
			egg->asyncProgramId = 0;
			egg_draw_layers(egg, layers, projectionMatrix);
			return false;
		}
	}
	egg_pick_slot_t *slot = &pick->slots[pick->active];
	const ecs_vec_t *layer = ecs_vec_first_t(layers, ecs_vec_t);
	GLint old_storage;
	egg_storage_binding_t old_base0 = sStorageBinding(0);
	egg_storage_binding_t old_base1 = sStorageBinding(1);
	GLint64 max_storage;
	glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &max_storage);
	glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &old_storage);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram(egg->asyncProgramId);
	glUniformMatrix4fv(egg->asyncProjectionUniform, 1, GL_FALSE, projectionMatrix);
	glUniform2i(egg->asyncMouseUniform, pick->x, pick->y);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, egg->atlasTextureId);
	glUniform1i(egg->asyncAtlasUniform, 0);
	glBindVertexArray(egg->vaoId);
	glBindBuffer(GL_ARRAY_BUFFER, egg->vboId);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, slot->buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, egg->eligibilityBuffer);
	bool success = true;
	int32_t next_layer = 0;
	for (int32_t i = 0; i < ecs_vec_count(layers); ++i) {
		next_layer = i;
		int32_t n = ecs_vec_count(&layer[i]);
		if (!n) {
			next_layer = i + 1;
			continue;
		}
		if (n % 3 || (uint32_t)(n / 3) > UINT32_MAX - pick->order) {
			fprintf(stderr, "egg: invalid or excessive picking primitives\n");
			success = false;
			break;
		}
		size_t count = (size_t)n / 3;
		if (count > SIZE_MAX - slot->count ||
			(size_t)n > (size_t)PTRDIFF_MAX / sizeof(egg_vertex_t) ||
			count > (size_t)PTRDIFF_MAX / sizeof(uint32_t)) {
			fprintf(stderr, "egg: picking metadata size overflow\n");
			success = false;
			break;
		}
		size_t needed = slot->count + count;
		if (needed > SIZE_MAX / sizeof(*slot->entities) ||
			max_storage <= 0 || count > (uint64_t)max_storage / sizeof(uint32_t)) {
			fprintf(stderr, "egg: picking metadata size overflow\n");
			success = false;
			break;
		}
		if (needed > slot->capacity) {
			size_t capacity = slot->capacity ? slot->capacity : 64;
			while (capacity < needed && capacity <= SIZE_MAX / 2) capacity *= 2;
			if (capacity < needed || capacity > SIZE_MAX / sizeof(*slot->entities)) capacity = needed;
			uint64_t *entities = realloc(slot->entities, capacity * sizeof(*entities));
			if (!entities) {
				fprintf(stderr, "egg: failed to allocate picking entity map\n");
				success = false;
				break;
			}
			slot->entities = entities;
			slot->capacity = capacity;
		}
		if (count > egg->eligibilityCapacity) {
			size_t capacity = egg->eligibilityCapacity ? egg->eligibilityCapacity : 64;
			while (capacity < count && capacity <= SIZE_MAX / 2) capacity *= 2;
			if (capacity < count || capacity > SIZE_MAX / sizeof(*egg->eligibility)) capacity = count;
			uint32_t *eligible = realloc(egg->eligibility, capacity * sizeof(*eligible));
			if (!eligible) {
				fprintf(stderr, "egg: failed to allocate picking eligibility map\n");
				success = false;
				break;
			}
			egg->eligibility = eligible;
			egg->eligibilityCapacity = capacity;
		}
		const egg_vertex_t *vertices = ecs_vec_first(&layer[i]);
		for (size_t j = 0; j < count; ++j) {
			uint32_t id = vertices[j * 3 + 2].pick_id;
			uint64_t entity = id ? resolve(id, ctx) : 0;
			slot->entities[slot->count + j] = entity;
			egg->eligibility[j] = entity != 0;
		}
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, egg->eligibilityBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, count * sizeof(*egg->eligibility), egg->eligibility, GL_STREAM_DRAW);
		glUniform1ui(egg->asyncOrderUniform, pick->order + 1);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)n * sizeof(*vertices)), vertices, GL_DYNAMIC_DRAW);
		glDrawArrays(GL_TRIANGLES, 0, n);
		next_layer = i + 1;
		slot->count = needed;
		pick->order += (uint32_t)count;
		if (glGetError() != GL_NO_ERROR) {
			fprintf(stderr, "egg: OpenGL error during asynchronous picking draw\n");
			success = false;
			break;
		}
	}
	if (!success) {
		glUseProgram(egg->programId);
		glUniformMatrix4fv(egg->projectionUniform, 1, GL_FALSE, projectionMatrix);
		glUniform1i(egg->atlasUniform, 0);
		for (int32_t i = next_layer; i < ecs_vec_count(layers); ++i) {
			int32_t n = ecs_vec_count(&layer[i]);
			if (!n) continue;
			glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)n * sizeof(egg_vertex_t)),
				ecs_vec_first(&layer[i]), GL_DYNAMIC_DRAW);
			glDrawArrays(GL_TRIANGLES, 0, n);
		}
	}
	sRestoreStorageBinding(0, old_base0);
	sRestoreStorageBinding(1, old_base1);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)old_storage);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	glDisable(GL_BLEND);
	return success;
}

bool egg_pick_async_end(egg_pick_async_t *pick)
{
	if (!pick || pick->active < 0) {
		fprintf(stderr, "egg: invalid asynchronous pick end\n");
		return false;
	}
	egg_pick_slot_t *slot = &pick->slots[pick->active];
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
	slot->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	if (!slot->fence) {
		fprintf(stderr, "egg: failed to create picking fence\n");
		glDeleteBuffers(1, &slot->buffer);
		slot->buffer = 0;
		glGenBuffers(1, &slot->buffer);
		pick->active = -1;
		return false;
	}
	return true;
}

void egg_pick_async_complete(egg_pick_async_t *pick, bool displayed)
{
	if (!pick || pick->active < 0) return;
	egg_pick_slot_t *slot = &pick->slots[pick->active];
	slot->displayed = displayed && !slot->invalidated;
	pick->active = -1;
}

void egg_pick_async_invalidate(egg_pick_async_t *pick)
{
	if (!pick) return;
	for (int i = 0; i < EGG_PICK_SLOTS; ++i) {
		pick->slots[i].displayed = false;
		pick->slots[i].invalidated = true;
	}
}

int egg_pick_async_poll(egg_pick_async_t *pick, uint64_t *entity, uint64_t *frame)
{
	if (!pick || !entity || !frame) {
		fprintf(stderr, "egg: invalid asynchronous pick poll\n");
		return -1;
	}
	int result = 0;
	for (int i = 0; i < EGG_PICK_SLOTS; ++i) {
		egg_pick_slot_t *slot = &pick->slots[i];
		if (!slot->fence || i == pick->active) continue;
		GLenum status = glClientWaitSync(slot->fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
		if (status == GL_TIMEOUT_EXPIRED) continue;
		if (status == GL_WAIT_FAILED) {
			fprintf(stderr, "egg: failed waiting for picking fence\n");
			glDeleteSync(slot->fence);
			slot->fence = NULL;
			glDeleteBuffers(1, &slot->buffer);
			slot->buffer = 0;
			glGenBuffers(1, &slot->buffer);
			slot->displayed = false;
			result = -1;
			continue;
		}
		uint64_t value = UINT64_MAX;
		if (slot->displayed) {
			GLint old;
			glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &old);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, slot->buffer);
			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(value), &value);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)old);
			if (glGetError() != GL_NO_ERROR) {
				fprintf(stderr, "egg: failed to read asynchronous picking result\n");
				result = -1;
			} else if (result != -1 && (!result || slot->frame > *frame)) {
				uint32_t order = ~(uint32_t)value;
				if (value != UINT64_MAX && (!order || order > slot->count)) {
					fprintf(stderr, "egg: invalid asynchronous picking order\n");
					result = -1;
				} else {
					*entity = value == UINT64_MAX ? 0 : slot->entities[order - 1];
					*frame = slot->frame;
					result = 1;
				}
			}
		}
		glDeleteSync(slot->fence);
		slot->fence = NULL;
		slot->displayed = false;
	}
	return result;
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

static bool sPickBegin(egg_pick_target_t *target, int width, int height,
	int x, int y, int region_width, int region_height)
{
	if (!target || width <= 0 || height <= 0 || x < 0 || y < 0 ||
		region_width <= 0 || region_height <= 0 || x > width - region_width ||
		y > height - region_height) {
		fprintf(stderr, "egg: invalid picking target dimensions\n");
		if (target) target->valid = 0;
		return false;
	}
	GLint old_draw, old_texture, old_unpack, old_renderbuffer;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
	glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &old_unpack);
	glGetIntegerv(GL_RENDERBUFFER_BINDING, &old_renderbuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target->framebuffer);
	GLint target_draw_buffer;
	glGetIntegerv(GL_DRAW_BUFFER0, &target_draw_buffer);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	if (width != target->width || height != target->height) {
		glBindTexture(GL_TEXTURE_2D, target->texture);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32UI, width, height, 0, GL_RG_INTEGER, GL_UNSIGNED_INT, NULL);
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
		GLint scissor_box[4];
		glGetIntegerv(GL_SCISSOR_BOX, scissor_box);
		glGetBooleani_v(GL_COLOR_WRITEMASK, 0, mask);
		glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
		glEnable(GL_SCISSOR_TEST);
		glScissor(x, y, region_width, region_height);
		glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);
		const GLuint zero[4] = {0, 0, 0, 0};
		const GLfloat depth = 1.0f;
		glClearBufferuiv(GL_COLOR, 0, zero);
		glClearBufferfv(GL_DEPTH, 0, &depth);
		glColorMaski(0, mask[0], mask[1], mask[2], mask[3]);
		glDepthMask(depth_mask);
		glScissor(scissor_box[0], scissor_box[1], scissor_box[2], scissor_box[3]);
		if (!scissor) glDisable(GL_SCISSOR_TEST);
		target->region_x = x;
		target->region_y = y;
		target->region_w = region_width;
		target->region_h = region_height;
		target->next_order = 1;
		target->region_mode = false;
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
	glDrawBuffer((GLenum)target_draw_buffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)old_draw);
	return target->valid != 0;
}

bool egg_pick_target_begin(egg_pick_target_t *target, int width, int height)
{
	return sPickBegin(target, width, height, 0, 0, width, height);
}

bool egg_pick_target_begin_region(egg_pick_target_t *target, int width, int height, int x, int y)
{
	if (!target || width <= 0 || height <= 0 || x < 0 || y < 0 || x >= width || y >= height) {
		fprintf(stderr, "egg: invalid picking region\n");
		if (target) target->valid = 0;
		return false;
	}
	int gl_y = height - 1 - y;
	int left = x > 0 ? x - 1 : 0;
	int bottom = gl_y > 0 ? gl_y - 1 : 0;
	int right = x < width - 1 ? x + 1 : x;
	int top = gl_y < height - 1 ? gl_y + 1 : gl_y;
	if (!sPickBegin(target, width, height, left, bottom, right - left + 1, top - bottom + 1)) return false;
	target->center_x = x;
	target->center_y = gl_y;
	target->region_mode = true;
	return true;
}

uint32_t egg_pick_target_framebuffer(const egg_pick_target_t *target)
{
	return target && target->valid ? target->framebuffer : 0;
}

bool egg_pick_draw_layers(egg_pick_target_t *target, egg_render_t *egg,
	const ecs_vec_t *layers, const float *projectionMatrix, egg_pick_filter_t filter, void *ctx)
{
	if (!target || !target->valid || !egg || !egg->initialized || !layers || !projectionMatrix) {
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
	GLboolean blend = glIsEnabledi(GL_BLEND, 0);
	GLboolean depth = glIsEnabled(GL_DEPTH_TEST);
	GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
	GLboolean color_mask[4];
	GLint scissor_box[4];
	glGetIntegerv(GL_SCISSOR_BOX, scissor_box);
	glGetBooleani_v(GL_COLOR_WRITEMASK, 0, color_mask);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target->framebuffer);
	GLint target_draw_buffer;
	glGetIntegerv(GL_DRAW_BUFFER0, &target_draw_buffer);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glViewport(0, 0, target->width, target->height);
	glEnable(GL_SCISSOR_TEST);
	glScissor(target->region_x, target->region_y, target->region_w, target->region_h);
	glDisablei(GL_BLEND, 0);
	glDisable(GL_DEPTH_TEST);
	glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
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
		uint32_t primitives = (uint32_t)n / 3;
		if (primitives > UINT32_MAX - target->next_order) {
			fprintf(stderr, "egg: picking draw order overflow\n");
			success = false;
			break;
		}
		size_t size = (size_t)n * sizeof(egg_vertex_t);
		const egg_vertex_t *data = ecs_vec_first(&l[i]);
		egg_vertex_t *vertices = NULL;
		if (filter) {
			vertices = malloc(size);
			if (!vertices) {
				fprintf(stderr, "egg: failed to allocate picking vertices\n");
				success = false;
				break;
			}
			memcpy(vertices, data, size);
			for (int32_t j = 0; j < n; ++j) {
				if (vertices[j].pick_id && !filter(vertices[j].pick_id, ctx)) {
					vertices[j].pick_id = 0;
				}
			}
			data = vertices;
		}
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)size, data, GL_DYNAMIC_DRAW);
		glUniform1ui(egg->pickOrderUniform, target->next_order);
		glDrawArrays(GL_TRIANGLES, 0, n);
		target->next_order += primitives;
		free(vertices);
		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			fprintf(stderr, "egg: OpenGL error drawing picking target: 0x%x\n", (unsigned)error);
			success = false;
			break;
		}
	}
	if (!success) target->valid = 0;
	glDrawBuffer((GLenum)target_draw_buffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)framebuffer);
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	glScissor(scissor_box[0], scissor_box[1], scissor_box[2], scissor_box[3]);
	if (!scissor) glDisable(GL_SCISSOR_TEST);
	glColorMaski(0, color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
	if (blend) glEnablei(GL_BLEND, 0);
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
	if (!target || !target->valid || !id || x < 0 || y < 0 || x >= target->width || y >= target->height ||
		x < target->region_x || x >= target->region_x + target->region_w ||
		target->height - 1 - y < target->region_y ||
		target->height - 1 - y >= target->region_y + target->region_h) {
		fprintf(stderr, "egg: invalid picking pixel read\n");
		return false;
	}
	GLint framebuffer, pack, alignment, row_length, skip_rows, skip_pixels, swap_bytes, read_buffer;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &framebuffer);
	glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
	glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
	glGetIntegerv(GL_PACK_ROW_LENGTH, &row_length);
	glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
	glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
	glGetIntegerv(GL_PACK_SWAP_BYTES, &swap_bytes);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, target->framebuffer);
	glGetIntegerv(GL_READ_BUFFER, &read_buffer);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
	glReadPixels(x, target->height - 1 - y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, id);
	glReadBuffer((GLenum)read_buffer);
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

bool egg_pick_target_read_region(egg_pick_target_t *target, uint32_t *id)
{
	if (!target || !target->valid || !target->region_mode || !id) {
		fprintf(stderr, "egg: invalid picking region read\n");
		return false;
	}
	GLint framebuffer, pack, alignment, row_length, skip_rows, skip_pixels, swap_bytes, read_buffer;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &framebuffer);
	glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
	glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
	glGetIntegerv(GL_PACK_ROW_LENGTH, &row_length);
	glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
	glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
	glGetIntegerv(GL_PACK_SWAP_BYTES, &swap_bytes);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, target->framebuffer);
	glGetIntegerv(GL_READ_BUFFER, &read_buffer);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
	uint32_t pixels[3 * 3 * 2] = {0};
	glReadPixels(target->region_x, target->region_y, target->region_w, target->region_h,
		GL_RG_INTEGER, GL_UNSIGNED_INT, pixels);
	glReadBuffer((GLenum)read_buffer);
	glPixelStorei(GL_PACK_ALIGNMENT, alignment);
	glPixelStorei(GL_PACK_ROW_LENGTH, row_length);
	glPixelStorei(GL_PACK_SKIP_ROWS, skip_rows);
	glPixelStorei(GL_PACK_SKIP_PIXELS, skip_pixels);
	glPixelStorei(GL_PACK_SWAP_BYTES, swap_bytes);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)pack);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)framebuffer);
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		fprintf(stderr, "egg: OpenGL error reading picking region: 0x%x\n", (unsigned)error);
		target->valid = 0;
		return false;
	}
	*id = 0;
	int best_distance = 3;
	uint32_t best_order = 0;
	for (int y = 0; y < target->region_h; ++y) {
		for (int x = 0; x < target->region_w; ++x) {
			int index = (y * target->region_w + x) * 2;
			int dx = target->region_x + x - target->center_x;
			int dy = target->region_y + y - target->center_y;
			int distance = dx * dx + dy * dy;
			if (pixels[index] && (distance < best_distance ||
				(distance == best_distance && pixels[index + 1] > best_order))) {
				*id = pixels[index];
				best_distance = distance;
				best_order = pixels[index + 1];
			}
		}
	}
	return true;
}
