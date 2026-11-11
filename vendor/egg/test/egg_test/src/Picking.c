#include <bake_test.h>
#include <EgDraws.h>
#include <egg.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
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
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3));
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
