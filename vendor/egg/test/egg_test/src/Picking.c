#include <bake_test.h>
#include <EgDraws.h>
#include <egg.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *window;
static SDL_GLContext context;
static egg_render_t *renderer;
static egg_pick_target_t *target;
static ecs_vec_t layers;
static ecs_world_t *world;

static bool selected(uint32_t id, void *ctx)
{
	return id != *(uint32_t *)ctx;
}

void Picking_setup(void)
{
	world = ecs_init();
	test_assert(SDL_Init(SDL_INIT_VIDEO));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
	window = SDL_CreateWindow("picking test", 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
	test_assert(window != NULL);
	context = SDL_GL_CreateContext(window);
	test_assert(context != NULL);
	renderer = egg_render_init();
	test_assert(renderer != NULL);
	target = egg_pick_target_create();
	test_assert(target != NULL);
	ecs_vec_init_t(NULL, &layers, ecs_vec_t, 0);
}

void Picking_teardown(void)
{
	ecs_vec_t *layer = ecs_vec_first_t(&layers, ecs_vec_t);
	for (int32_t i = 0; i < ecs_vec_count(&layers); ++i) {
		ecs_vec_fini_t(NULL, &layer[i], EgDraws_Vertex);
	}
	ecs_vec_fini_t(NULL, &layers, ecs_vec_t);
	egg_pick_target_destroy(target);
	egg_render_destroy(renderer);
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
	ecs_fini(world);
}

static void add_quad(int layer_index, uint32_t id, uint8_t alpha)
{
	while (ecs_vec_count(&layers) <= layer_index) {
		ecs_vec_t *layer = ecs_vec_append_t(NULL, &layers, ecs_vec_t);
		ecs_vec_init_t(NULL, layer, EgDraws_Vertex, 0);
	}
	ecs_vec_t *layer = ecs_vec_get_t(&layers, ecs_vec_t, layer_index);
	EgDraws_Vertex *v = ecs_vec_grow_t(NULL, layer, EgDraws_Vertex, 6);
	float p[6][2] = {{-0.8f,-0.8f}, {0.8f,-0.8f}, {0.8f,0.8f},
		{-0.8f,-0.8f}, {0.8f,0.8f}, {-0.8f,0.8f}};
	for (int i = 0; i < 6; ++i) {
		v[i] = (EgDraws_Vertex){
			.position = {p[i][0], p[i][1]},
			.uv = {0.5f / EG_DRAWS_ATLAS_WIDTH, 0.5f / EG_DRAWS_ATLAS_HEIGHT},
			.rgba = {255,255,255,alpha}, .pick_id = id
		};
	}
}

static uint64_t resolve_async(uint32_t id, void *ctx)
{
	return id == *(uint32_t *)ctx ? 0 : id;
}

static uint64_t resolve_benchmark(uint32_t id, void *ctx)
{
	(void)ctx;
	return id;
}

void Picking_test_benchmark(void)
{
	if (!getenv("EGG_PICK_BENCHMARK")) return;
	egg_pick_async_t *pick = egg_pick_async_create();
	test_assert(pick != NULL);
	SDL_GL_SetSwapInterval(0);
	for (int i = 0; i < 2000; ++i) add_quad(0, (uint32_t)(i + 1), 255);
	ecs_vec_t *layer = ecs_vec_get_t(&layers, ecs_vec_t, 0);
	EgDraws_Vertex *vertices = ecs_vec_first_t(layer, EgDraws_Vertex);
	for (int32_t i = 0; i < ecs_vec_count(layer); ++i) {
		vertices[i].position[0] *= 0.1f;
		vertices[i].position[1] *= 0.1f;
	}
	const float matrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
	glViewport(0, 0, 64, 64);
	const int frames = 100;
	for (int mode = 0; mode < 3; ++mode) {
		int issued = 0;
		glFinish();
		uint64_t start = SDL_GetPerformanceCounter();
		for (int i = 0; i < frames; ++i) {
			glClear(GL_COLOR_BUFFER_BIT);
			if (mode == 0) {
				egg_draw_layers(renderer, &layers, matrix);
			} else if (mode == 1) {
				uint32_t id, skipped = 0;
				egg_draw_layers(renderer, &layers, matrix);
				test_assert(egg_pick_target_begin_region(target, 64, 64, 32, 32));
				test_assert(egg_pick_draw_layers(target, renderer, &layers, matrix, selected, &skipped));
				test_assert(egg_pick_target_read_region(target, &id));
				++issued;
			} else {
				uint64_t id = 0, frame = 0;
				test_assert(egg_pick_async_poll(pick, &id, &frame) >= 0);
				int started = egg_pick_async_begin(pick, 32, 31, (uint64_t)i);
				test_assert(started >= 0);
				if (!started) egg_draw_layers(renderer, &layers, matrix);
				else {
					test_assert(egg_pick_async_draw(pick, renderer, &layers, matrix, resolve_benchmark, NULL));
					test_assert(egg_pick_async_end(pick));
					egg_pick_async_complete(pick, true);
					++issued;
				}
			}
		}
		glFinish();
		double ms = 1000.0 * (SDL_GetPerformanceCounter() - start) / SDL_GetPerformanceFrequency() / frames;
		printf("picking benchmark %s: %.3f ms/frame; %d/%d picks issued\n",
			mode == 0 ? "visible only" : mode == 1 ? "old synchronous" : "new asynchronous",
			ms, issued, frames);
	}
	egg_pick_async_destroy(pick);
}

static uint64_t async_result(egg_pick_async_t *pick, uint64_t expected_frame)
{
	uint64_t id = 0, frame = 0;
	int ready = 0;
	for (int i = 0; i < 1000 && !ready; ++i) {
		ready = egg_pick_async_poll(pick, &id, &frame);
		if (!ready) SDL_Delay(1);
	}
	test_int(ready, 1);
	test_assert(frame == expected_frame);
	return id;
}

void Picking_test_async(void)
{
	if (!SDL_GL_ExtensionSupported("GL_ARB_gpu_shader_int64") ||
		!SDL_GL_ExtensionSupported("GL_NV_shader_atomic_int64")) {
		test_assert(egg_pick_async_create() == NULL);
		return;
	}
	const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
	uint32_t skipped = 77;
	add_quad(0, 42, 255);
	add_quad(1, skipped, 255);
	add_quad(2, 55, 0);
	add_quad(3, 44, 255);
	egg_pick_async_t *pick = egg_pick_async_create();
	test_assert(pick != NULL);
	glViewport(0, 0, 64, 64);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	egg_draw_layers(renderer, &layers, identity);
	unsigned char visible[4], combined[4];
	glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, visible);
	glClear(GL_COLOR_BUFFER_BIT);
	GLuint bindings[2];
	GLint alignment = 0;
	glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &alignment);
	test_assert(alignment > 0);
	glGenBuffers(2, bindings);
	for (int i = 0; i < 2; ++i) {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, bindings[i]);
		glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)alignment + 64, NULL, GL_DYNAMIC_DRAW);
		glBindBufferRange(GL_SHADER_STORAGE_BUFFER, (GLuint)i, bindings[i], alignment, 64);
	}
	test_int(egg_pick_async_begin(pick, 32, 32, 7), 1);
	test_assert(egg_pick_async_draw(pick, renderer, &layers, identity, resolve_async, &skipped));
	for (int i = 0; i < 2; ++i) {
		GLint buffer = 0;
		GLint64 start = 0, size = 0;
		glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, (GLuint)i, &buffer);
		glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_START, (GLuint)i, &start);
		glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE, (GLuint)i, &size);
		test_assert((GLuint)buffer == bindings[i] && start == alignment && size == 64);
	}
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glDeleteBuffers(2, bindings);
	glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, combined);
	test_assert(memcmp(visible, combined, sizeof(visible)) == 0);
	test_assert(egg_pick_async_end(pick));
	egg_pick_async_complete(pick, true);
	test_assert(async_result(pick, 7) == 44);

	ecs_vec_t near_layers;
	ecs_vec_init_t(NULL, &near_layers, ecs_vec_t, 0);
	ecs_vec_t *near_layer = ecs_vec_append_t(NULL, &near_layers, ecs_vec_t);
	ecs_vec_init_t(NULL, near_layer, EgDraws_Vertex, 0);
	EgDraws_Vertex *near_vertices = ecs_vec_grow_t(NULL, near_layer, EgDraws_Vertex, 6);
	memcpy(near_vertices, ecs_vec_first(ecs_vec_get_t(&layers, ecs_vec_t, 0)), 6 * sizeof(*near_vertices));
	for (int i = 0; i < 6; ++i) near_vertices[i].pick_id = 88;
	float near_projection[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-0.5f,1};
	test_int(egg_pick_async_begin(pick, 32, 32, 8), 1);
	test_assert(egg_pick_async_draw(pick, renderer, &near_layers, near_projection, resolve_async, &skipped));
	test_assert(egg_pick_async_draw(pick, renderer, &layers, identity, resolve_async, &skipped));
	test_assert(egg_pick_async_end(pick));
	egg_pick_async_complete(pick, true);
	test_assert(async_result(pick, 8) == 88);
	test_int(egg_pick_async_begin(pick, 32, 32, 10), 1);
	test_assert(egg_pick_async_draw(pick, renderer, &layers, identity, resolve_async, &skipped));
	test_assert(egg_pick_async_draw(pick, renderer, &near_layers, near_projection, resolve_async, &skipped));
	test_assert(egg_pick_async_end(pick));
	egg_pick_async_complete(pick, true);
	test_assert(async_result(pick, 10) == 88);
	for (int i = 0; i < 6; ++i) near_vertices[i].pick_id = skipped;
	test_int(egg_pick_async_begin(pick, 32, 32, 11), 1);
	test_assert(egg_pick_async_draw(pick, renderer, &layers, identity, resolve_async, &skipped));
	test_assert(egg_pick_async_draw(pick, renderer, &near_layers, near_projection, resolve_async, &skipped));
	test_assert(egg_pick_async_end(pick));
	egg_pick_async_complete(pick, true);
	test_assert(async_result(pick, 11) == 44);
	test_int(egg_pick_async_begin(pick, 0, 0, 9), 1);
	test_assert(egg_pick_async_draw(pick, renderer, &layers, identity, resolve_async, &skipped));
	test_assert(egg_pick_async_end(pick));
	egg_pick_async_complete(pick, true);
	test_assert(async_result(pick, 9) == 0);

	for (int i = 0; i < 3; ++i) {
		test_int(egg_pick_async_begin(pick, 32, 32, (uint64_t)(20 - i)), 1);
		test_assert(egg_pick_async_end(pick));
		egg_pick_async_complete(pick, true);
	}
	test_int(egg_pick_async_begin(pick, 32, 32, 21), 0);
	glFinish();
	test_assert(async_result(pick, 20) == 0);
	test_int(egg_pick_async_begin(pick, 32, 32, 22), 1);
	egg_pick_async_invalidate(pick);
	test_assert(egg_pick_async_end(pick));
	egg_pick_async_complete(pick, true);
	glFinish();
	uint64_t ignored_id = 0, ignored_frame = 0;
	test_int(egg_pick_async_poll(pick, &ignored_id, &ignored_frame), 0);
	test_int(egg_pick_async_begin(pick, 32, 32, 23), 1);
	test_assert(egg_pick_async_end(pick));
	egg_pick_async_complete(pick, false);
	glFinish();
	test_int(egg_pick_async_poll(pick, &ignored_id, &ignored_frame), 0);
	ecs_vec_fini_t(NULL, near_layer, EgDraws_Vertex);
	ecs_vec_fini_t(NULL, &near_layers, ecs_vec_t);
	egg_pick_async_destroy(pick);
}

void Picking_test_integer_target(void)
{
	const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
	uint32_t skipped = 77;
	uint32_t result = 0;
	add_quad(0, UINT32_C(0xf1234567), 255);
	add_quad(1, skipped, 255);
	add_quad(2, 55, 0);
	glViewport(2, 3, 10, 11);
	glEnable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	test_assert(egg_pick_target_framebuffer(target) == 0);
	test_assert(egg_pick_target_begin(target, 64, 64));
	test_assert(egg_pick_target_framebuffer(target) != 0);
	GLint previous_read;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, egg_pick_target_framebuffer(target));
	float depth = 0.0f;
	glReadPixels(32, 32, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)previous_read);
	test_assert(depth == 1.0f);
	test_assert(egg_pick_draw_layers(target, renderer, &layers, identity, selected, &skipped));
	test_assert(glIsEnabled(GL_BLEND));
	test_assert(glIsEnabled(GL_DEPTH_TEST));
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	test_int(viewport[0], 2);
	test_int(viewport[1], 3);
	test_int(viewport[2], 10);
	test_int(viewport[3], 11);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 3);
	test_assert(egg_pick_target_read(target, 32, 32, &result));
	test_assert(result == UINT32_C(0xf1234567));
	GLint skip_pixels;
	glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
	test_int(skip_pixels, 3);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	test_assert(egg_pick_target_read(target, 0, 0, &result));
	test_assert(result == 0);
	test_assert(!egg_pick_target_read(target, -1, 0, &result));
	add_quad(3, 44, 1);
	test_assert(egg_pick_draw_layers(target, renderer, &layers, identity, selected, &skipped));
	test_assert(egg_pick_target_read(target, 32, 32, &result));
	test_assert(result == 44);

	const unsigned char *atlas = EgDraws_Font_GetBitmap();
	int transparent = 1;
	while (transparent < EG_DRAWS_ATLAS_WIDTH * EG_DRAWS_ATLAS_HEIGHT && atlas[transparent] != 0) {
		++transparent;
	}
	test_assert(transparent < EG_DRAWS_ATLAS_WIDTH * EG_DRAWS_ATLAS_HEIGHT);
	add_quad(4, 88, 255);
	ecs_vec_t *layer = ecs_vec_get_t(&layers, ecs_vec_t, 4);
	EgDraws_Vertex *v = ecs_vec_first_t(layer, EgDraws_Vertex);
	for (int i = 0; i < 6; ++i) {
		v[i].uv[0] = ((transparent % EG_DRAWS_ATLAS_WIDTH) + 0.5f) / EG_DRAWS_ATLAS_WIDTH;
		v[i].uv[1] = ((transparent / EG_DRAWS_ATLAS_WIDTH) + 0.5f) / EG_DRAWS_ATLAS_HEIGHT;
	}
	test_assert(egg_pick_draw_layers(target, renderer, &layers, identity, selected, &skipped));
	test_assert(egg_pick_target_read(target, 32, 32, &result));
	test_assert(result == 44);

	egg_render_t *second = egg_render_init();
	test_assert(second != NULL);
	ecs_vec_t second_layers;
	ecs_vec_init_t(NULL, &second_layers, ecs_vec_t, 0);
	ecs_vec_t *second_layer = ecs_vec_append_t(NULL, &second_layers, ecs_vec_t);
	ecs_vec_init_t(NULL, second_layer, EgDraws_Vertex, 0);
	EgDraws_Vertex *second_vertices = ecs_vec_grow_t(NULL, second_layer, EgDraws_Vertex, 6);
	ecs_vec_t *first_layer = ecs_vec_get_t(&layers, ecs_vec_t, 0);
	memcpy(second_vertices, ecs_vec_first(first_layer), 6 * sizeof(*second_vertices));
	for (int i = 0; i < 6; ++i) {
		second_vertices[i].pick_id = 49;
		second_vertices[i].position[1] = second_vertices[i].position[1] > 0 ? 0.8f : 0.2f;
	}
	test_assert(egg_pick_draw_layers(target, second, &second_layers, identity, selected, &skipped));
	test_assert(egg_pick_target_read(target, 32, 10, &result));
	test_assert(result == 49);
	test_assert(egg_pick_target_read(target, 32, 54, &result));
	test_assert(result == 44);
	ecs_vec_fini_t(NULL, second_layer, EgDraws_Vertex);
	ecs_vec_fini_t(NULL, &second_layers, ecs_vec_t);
	egg_render_destroy(second);

	test_assert(egg_pick_target_begin(target, 32, 32));
	test_assert(egg_pick_target_read(target, 16, 16, &result));
	test_assert(result == 0);
}

static void add_pixel(int layer_index, uint32_t id, int x, int y)
{
	add_quad(layer_index, id, 255);
	ecs_vec_t *layer = ecs_vec_get_t(&layers, ecs_vec_t, layer_index);
	EgDraws_Vertex *v = ecs_vec_first_t(layer, EgDraws_Vertex) + ecs_vec_count(layer) - 6;
	float left = (float)x / 32.0f - 1.0f;
	float right = (float)(x + 1) / 32.0f - 1.0f;
	float bottom = (float)y / 32.0f - 1.0f;
	float top = (float)(y + 1) / 32.0f - 1.0f;
	for (int i = 0; i < 6; ++i) {
		v[i].position[0] = v[i].position[0] < 0 ? left : right;
		v[i].position[1] = v[i].position[1] < 0 ? bottom : top;
	}
}

void Picking_test_region(void)
{
	const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
	uint32_t skipped = 77, result = 0;
	// Input (32,31) corresponds to OpenGL pixel (32,32).
	add_pixel(0, 11, 31, 32);
	add_pixel(1, 22, 33, 32);
	add_pixel(2, 33, 33, 33);
	glViewport(2, 3, 10, 11);
	glEnable(GL_SCISSOR_TEST);
	glScissor(4, 5, 6, 7);
	glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
	glColorMaski(1, GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
	glEnablei(GL_BLEND, 1);
	test_assert(egg_pick_target_begin_region(target, 64, 64, 32, 31));
	GLint old_draw;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, egg_pick_target_framebuffer(target));
	glDrawBuffer(GL_NONE);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)old_draw);
	GLint old_read;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, egg_pick_target_framebuffer(target));
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)old_read);
	test_assert(egg_pick_draw_layers(target, renderer, &layers, identity, selected, &skipped));
	test_assert(egg_pick_target_read_region(target, &result));
	test_int(result, 22); // Equidistant: later layer wins; diagonals are farther away.
	glBindFramebuffer(GL_READ_FRAMEBUFFER, egg_pick_target_framebuffer(target));
	GLint read_buffer;
	glGetIntegerv(GL_READ_BUFFER, &read_buffer);
	test_int(read_buffer, GL_NONE);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)old_read);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, egg_pick_target_framebuffer(target));
	GLint draw_buffer;
	glGetIntegerv(GL_DRAW_BUFFER0, &draw_buffer);
	test_int(draw_buffer, GL_NONE);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)old_draw);
	egg_render_t *second = egg_render_init();
	test_assert(second != NULL);
	ecs_vec_t second_layers = {0};
	ecs_vec_t *second_layer = ecs_vec_append_t(NULL, &second_layers, ecs_vec_t);
	ecs_vec_init_t(NULL, second_layer, EgDraws_Vertex, 0);
	EgDraws_Vertex *second_vertices = ecs_vec_grow_t(NULL, second_layer, EgDraws_Vertex, 6);
	ecs_vec_t *first_layer = ecs_vec_get_t(&layers, ecs_vec_t, 0);
	memcpy(second_vertices, ecs_vec_first(first_layer), 6 * sizeof(*second_vertices));
	for (int i = 0; i < 6; ++i) second_vertices[i].pick_id = 66;
	test_assert(egg_pick_draw_layers(target, second, &second_layers, identity, selected, &skipped));
	test_assert(egg_pick_target_read_region(target, &result));
	test_int(result, 66); // Later draw context wins an equal-distance tie.
	ecs_vec_fini_t(NULL, second_layer, EgDraws_Vertex);
	ecs_vec_fini_t(NULL, &second_layers, ecs_vec_t);
	egg_render_destroy(second);
	GLint box[4], viewport[4];
	GLboolean mask[4];
	glGetIntegerv(GL_SCISSOR_BOX, box);
	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetBooleanv(GL_COLOR_WRITEMASK, mask);
	test_assert(glIsEnabled(GL_SCISSOR_TEST));
	test_int(box[0], 4); test_int(box[1], 5); test_int(box[2], 6); test_int(box[3], 7);
	test_int(viewport[0], 2); test_int(viewport[1], 3); test_int(viewport[2], 10); test_int(viewport[3], 11);
	test_assert(!mask[0] && mask[1] && !mask[2] && mask[3]);
	glGetBooleani_v(GL_COLOR_WRITEMASK, 1, mask);
	test_assert(mask[0] && !mask[1] && mask[2] && !mask[3]);
	test_assert(glIsEnabledi(GL_BLEND, 1));

	add_pixel(0, 44, 32, 32);
	test_assert(egg_pick_target_begin_region(target, 64, 64, 32, 31));
	test_assert(egg_pick_draw_layers(target, renderer, &layers, identity, NULL, NULL));
	test_assert(egg_pick_target_read_region(target, &result));
	test_int(result, 44); // Center wins over later adjacent primitives.

	// An unrelated region must be cleared instead of returning stale pixels.
	test_assert(egg_pick_target_begin_region(target, 64, 64, 0, 0));
	test_assert(egg_pick_draw_layers(target, renderer, &layers, identity, selected, &skipped));
	test_assert(egg_pick_target_read_region(target, &result));
	test_int(result, 0);
	test_assert(!egg_pick_target_read(target, 32, 31, &result));
	add_pixel(3, 55, 0, 63);
	test_assert(egg_pick_target_begin_region(target, 64, 64, 0, 0));
	test_assert(egg_pick_draw_layers(target, renderer, &layers, identity, selected, &skipped));
	test_assert(egg_pick_target_read_region(target, &result));
	test_int(result, 55);
	add_pixel(3, 56, 63, 63);
	add_pixel(3, 57, 0, 0);
	add_pixel(3, 58, 63, 0);
	const int corners[3][3] = {{63,0,56}, {0,63,57}, {63,63,58}};
	for (int i = 0; i < 3; ++i) {
		test_assert(egg_pick_target_begin_region(target, 64, 64, corners[i][0], corners[i][1]));
		test_assert(egg_pick_draw_layers(target, renderer, &layers, identity, selected, &skipped));
		test_assert(egg_pick_target_read_region(target, &result));
		test_int(result, corners[i][2]);
	}
	glDisable(GL_SCISSOR_TEST);
	test_assert(egg_pick_target_begin_region(target, 64, 64, 63, 63));
	test_assert(egg_pick_draw_layers(target, renderer, &layers, identity, selected, &skipped));
	test_assert(!glIsEnabled(GL_SCISSOR_TEST));
	test_assert(!egg_pick_target_begin_region(target, 64, 64, 64, 0));
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisablei(GL_BLEND, 1);
	glDisable(GL_SCISSOR_TEST);
}
