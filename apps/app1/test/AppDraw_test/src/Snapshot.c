#include <bake_test.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <EgWindowsSdl.h>
#include <EgWindowsSdlGl.h>
#include "../../../src/AppDraw.c"

static ecs_world_t *world;
static ecs_entity_t entity;
static EgDraws_List *list;
static const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
static int generic_frames, generic_begins, generic_ends, sdl_frames;
static int renderer_cleanups;
static SDL_GLContext expected_cleanup_context;
static bool cleanup_failure_after_destroy;

typedef struct {
	void (*destroy)(void *);
	void *resource;
} TrackedCleanup;

static void tracked_destroy(void *resource)
{
	TrackedCleanup *cleanup = resource;
	test_assert(SDL_GL_GetCurrentContext() == expected_cleanup_context);
	++renderer_cleanups;
	cleanup->destroy(cleanup->resource);
}

static bool tracked_cleanup(void *window, void *context, void (*destroy)(void *), void *resource)
{
	TrackedCleanup cleanup = {destroy, resource};
	return EgWindowsSdlGlCleanup(window, context, tracked_destroy, &cleanup);
}

static bool failing_cleanup(void *window, void *context, void (*destroy)(void *), void *resource)
{
	if (cleanup_failure_after_destroy) {
		test_assert(EgWindowsSdlGlCleanup(window, context, destroy, resource));
	}
	return false;
}

static void generic_frame_begin(ecs_world_t *w, ecs_entity_t window)
{
	const EgWindowsWindow *native = ecs_get(w, window, EgWindowsWindow);
	const EgWindowsOpenGLContext *gl = ecs_get(w, window, EgWindowsOpenGLContext);
	test_assert(native && gl);
	test_assert(SDL_GL_GetCurrentWindow() == native->object);
	test_assert(SDL_GL_GetCurrentContext() == gl->gl_context);
	test_int(generic_begins, generic_frames);
	++generic_begins;
}

static void generic_frame_end(ecs_world_t *w, ecs_entity_t window)
{
	const EgWindowsWindow *native = ecs_get(w, window, EgWindowsWindow);
	const EgWindowsOpenGLContext *gl = ecs_get(w, window, EgWindowsOpenGLContext);
	test_assert(native && gl);
	test_assert(SDL_GL_GetCurrentWindow() == native->object);
	test_assert(SDL_GL_GetCurrentContext() == gl->gl_context);
	test_int(generic_begins, generic_ends + 1);
	++generic_ends;
}

static void generic_frame_complete(ecs_world_t *w, ecs_entity_t window, bool displayed)
{
	(void)w;
	(void)window;
	if (displayed) {
		test_int(generic_begins, generic_frames + 1);
		test_int(generic_ends, generic_frames + 1);
		++generic_frames;
	}
}

static void sdl_frame_complete(ecs_world_t *w, ecs_entity_t window, bool displayed)
{
	(void)w;
	(void)window;
	if (displayed) ++sdl_frames;
}

void Snapshot_setup(void)
{
	world = ecs_init();
	ECS_IMPORT(world, EgWindowsSdl);
	ECS_IMPORT(world, EgWindowsSdlGl);
	ECS_IMPORT(world, AppDraw);
	entity = ecs_new(world);
	ecs_add_id(world, entity, EgUiScriptEventsTarget);
	list = EgDraws_List_Create();
	test_assert(list != NULL);
	ecs_vec_t *layer = EgDraws_List_GetLayer(list, 0);
	test_assert(layer != NULL);
	EgDraws_Vertex *v = ecs_vec_grow_t(NULL, layer, EgDraws_Vertex, 3);
	test_assert(v != NULL);
	for (int i = 0; i < 3; ++i) {
		v[i] = (EgDraws_Vertex){.position = {(float)i, 0}, .rgba = {255,255,255,255}, .pick_id = (uint32_t)entity};
	}
}

void Snapshot_teardown(void)
{
	EgDraws_List_Destroy(list);
	ecs_fini(world);
}

static void complete_pick(ecs_entity_t window, egg_render_t *renderer)
{
	AppDrawPickFrame_begin(world, window);
	AppDrawPickState *state = ecs_get_mut(world, window, AppDrawPickState);
	AppDrawPickFrame_draw(world, state, renderer, list, identity);
	AppDrawPickFrame_end(world, window);
	AppDrawPickFrame_complete(world, window, true);
	state = ecs_get_mut(world, window, AppDrawPickState);
	state->frame--;
}

void Snapshot_test_publish_and_invalidate(void)
{
	ecs_entity_t window = ecs_new(world);
	ecs_add_id(world, window, AppDrawGpuPicking);
	ecs_set(world, window, AppDrawPickState, {.frame = -1});
	AppDrawPickState *state = ecs_get_mut(world, window, AppDrawPickState);
	state->cached = true;
	state->picked = entity;
	AppDrawPickFrame_complete(world, window, false);
	test_assert(!state->cached && !state->picked);
	AppDrawPickFrame_begin(world, window);
	test_assert(!state->drawing);
	AppDrawPickFrame_complete(world, window, true);
	test_assert(!state->cached);
}

void Snapshot_test_live_eligibility(void)
{
	test_assert(AppDraw_IsPickable((uint32_t)entity, world));
	ecs_remove_id(world, entity, EgUiScriptEventsTarget);
	test_assert(!AppDraw_IsPickable((uint32_t)entity, world));
	ecs_add_id(world, entity, EgUiScriptEventsTarget);
	test_assert(AppDraw_IsPickable((uint32_t)entity, world));
	ecs_entity_t parent = ecs_new(world);
	ecs_add_pair(world, entity, EcsChildOf, parent);
	ecs_add_id(world, parent, EcsDisabled);
	test_assert(!AppDraw_IsPickable((uint32_t)entity, world));
	ecs_remove_id(world, parent, EcsDisabled);
	test_assert(AppDraw_IsPickable((uint32_t)entity, world));
	ecs_delete(world, entity);
	test_assert(!AppDraw_IsPickable((uint32_t)entity, world));
}

void Snapshot_test_on_demand(void)
{
	test_assert(SDL_Init(SDL_INIT_VIDEO));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
	SDL_Window *window = SDL_CreateWindow("app picking test", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(window != NULL);
	SDL_GLContext context = SDL_GL_CreateContext(window);
	test_assert(context != NULL);
	egg_render_t *renderer = egg_render_init();
	test_assert(renderer != NULL);
	ecs_entity_t e_window = ecs_new(world);
	ecs_set(world, e_window, EgWindowsWindow, {.object = window});
	ecs_set(world, e_window, EgWindowsOpenGLContext, {.gl_context = context, .cleanup = EgWindowsSdlGlCleanup});
	ecs_add_id(world, e_window, AppDrawGpuPicking);
	ecs_set(world, e_window, AppDrawPickState,
		{.frame = -1});
	ecs_singleton_set(world, EgButtonsState, {{0}});
	ecs_vec_t *layer = ecs_vec_get_t(&list->layers, ecs_vec_t, 0);
	EgDraws_Vertex *v = ecs_vec_first_t(layer, EgDraws_Vertex);
	v[0].position[0] = -1; v[0].position[1] = -1;
	v[1].position[0] = 1; v[1].position[1] = -1;
	v[2].position[0] = 0; v[2].position[1] = 1;
	for (int i = 0; i < 3; ++i) {
		v[i].uv[0] = 0.5f / EG_DRAWS_ATLAS_WIDTH;
		v[i].uv[1] = 0.5f / EG_DRAWS_ATLAS_HEIGHT;
	}
	AppDrawPickState *state = ecs_get_mut(world, e_window, AppDrawPickState);
	SDL_WarpMouseInWindow(window, 32, 32);
	for (int i = 0; i < 50 && SDL_GetMouseFocus() != window; ++i) {
		SDL_PumpEvents();
		SDL_Delay(10);
	}
	test_assert(SDL_GetMouseFocus() == window);
	EgWindowsSdlUpdateState(world);
	complete_pick(e_window, renderer);
	state = ecs_get_mut(world, e_window, AppDrawPickState);
	ecs_entity_t result = 0;
	float x = 0, y = 0;
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	test_assert(state->cached);

	ecs_entity_t top = ecs_new(world);
	ecs_add_id(world, top, EgUiScriptEventsTarget);
	AppDrawPickFrame_begin(world, e_window);
	AppDrawPickFrame_draw(world, state, renderer, list, identity);
	for (int i = 0; i < 3; ++i) v[i].pick_id = (uint32_t)top;
	AppDrawPickFrame_draw(world, state, renderer, list, identity);
	AppDrawPickFrame_end(world, e_window);
	AppDrawPickFrame_complete(world, e_window, true);
	state->frame--;
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == top);
	for (int i = 0; i < 3; ++i) v[i].pick_id = (uint32_t)entity;
	complete_pick(e_window, renderer);

	EgWindowsOpenGLContext *gl = ecs_get_mut(world, e_window, EgWindowsOpenGLContext);
	gl->gl_context = NULL;
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == 0 && !state->cached);
	gl->gl_context = context;
	complete_pick(e_window, renderer);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity && state->cached);

	SDL_WarpMouseInWindow(window, 2, 32);
	SDL_PumpEvents();
	EgWindowsSdlUpdateState(world);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	complete_pick(e_window, renderer);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == 0);
	SDL_WarpMouseInWindow(window, 32, 32);
	SDL_PumpEvents();
	EgWindowsSdlUpdateState(world);
	complete_pick(e_window, renderer);
	ecs_singleton_set(world, EgButtonsState, {.mouse = {[1] = EG_BUTTONS_STATE_HELD}});
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	ecs_singleton_set(world, EgButtonsState, {{0}});
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	ecs_remove_id(world, entity, EgUiScriptEventsTarget);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == 0);
	ecs_add_id(world, entity, EgUiScriptEventsTarget);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	ecs_entity_t deleted = entity;
	ecs_delete(world, deleted);
	entity = ecs_new(world);
	test_assert((uint32_t)entity == (uint32_t)deleted && entity != deleted);
	ecs_add_id(world, entity, EgUiScriptEventsTarget);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == 0);
	for (int i = 0; i < 3; ++i) v[i].pick_id = (uint32_t)entity;
	complete_pick(e_window, renderer);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	for (int i = 0; i < 3; ++i) v[i].position[0] += 4;
	complete_pick(e_window, renderer);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == 0);
	test_assert(state->cached);
	test_assert(SDL_SetWindowSize(window, 96, 96));
	int width = 0, height = 0;
	for (int i = 0; i < 50 && (width != 96 || height != 96); ++i) {
		SDL_PumpEvents();
		test_assert(SDL_GetWindowSizeInPixels(window, &width, &height));
		SDL_Delay(10);
	}
	test_int(width, 96);
	test_int(height, 96);
	EgWindowsSdlUpdateState(world);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == 0);
	test_assert(!state->cached);
	state->cached = true;
	state->drawing = true;
	AppDrawPickFrame_complete(world, e_window, false);
	test_assert(!state->cached && !state->drawing);
	AppDrawPickFrame_complete(world, e_window, true);
	test_assert(!state->cached);

	SDL_Window *current_window = SDL_GL_GetCurrentWindow();
	SDL_GLContext current_context = SDL_GL_GetCurrentContext();
	ecs_remove(world, e_window, AppDrawPickState);
	test_assert(SDL_GL_GetCurrentWindow() == current_window &&
		SDL_GL_GetCurrentContext() == current_context);
	egg_render_destroy(renderer);
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void Snapshot_test_pick_target_lifecycle(void)
{
	test_assert(SDL_Init(SDL_INIT_VIDEO));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));


	SDL_Window *context_window = SDL_CreateWindow("pick target context removal", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(context_window != NULL);
	SDL_GLContext context = SDL_GL_CreateContext(context_window);
	test_assert(context != NULL);
	ecs_entity_t e_context = ecs_new(world);
	ecs_set(world, e_context, EgWindowsWindow, {.object = context_window});
	ecs_set(world, e_context, EgWindowsOpenGLContext, {.gl_context = context, .cleanup = EgWindowsSdlGlCleanup});
	ecs_set(world, e_context, AppDrawPickState,
		{.frame = -1});
	AppDrawPickState *state = ecs_get_mut(world, e_context, AppDrawPickState);
	state->target = egg_pick_target_create();
	test_assert(state->target != NULL);
	test_assert(egg_pick_target_begin_region(state->target, 64, 64, 32, 32));
	GLuint framebuffer = egg_pick_target_framebuffer(state->target);
	test_assert(framebuffer && glIsFramebuffer(framebuffer));
	test_assert(SDL_GL_MakeCurrent(NULL, NULL));
	ecs_remove(world, e_context, EgWindowsOpenGLContext);
	state = ecs_get_mut(world, e_context, AppDrawPickState);
	test_assert(state && state->target == NULL);
	SDL_Window *current_window = SDL_GL_GetCurrentWindow();
	SDL_GLContext current_context = SDL_GL_GetCurrentContext();
	test_assert(!current_window && !current_context);
	ecs_remove(world, e_context, AppDrawPickState);
	test_assert(SDL_GL_MakeCurrent(context_window, context));
	test_assert(!glIsFramebuffer(framebuffer));
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(context_window);

	SDL_Window *state_window = SDL_CreateWindow("pick target state removal", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(state_window != NULL);
	context = SDL_GL_CreateContext(state_window);
	test_assert(context != NULL);
	ecs_entity_t e_state = ecs_new(world);
	ecs_set(world, e_state, EgWindowsWindow, {.object = state_window});
	ecs_set(world, e_state, EgWindowsOpenGLContext, {.gl_context = context, .cleanup = EgWindowsSdlGlCleanup});
	ecs_set(world, e_state, AppDrawPickState,
		{.frame = -1});
	state = ecs_get_mut(world, e_state, AppDrawPickState);
	state->target = egg_pick_target_create();
	test_assert(state->target != NULL);
	test_assert(egg_pick_target_begin_region(state->target, 64, 64, 32, 32));
	framebuffer = egg_pick_target_framebuffer(state->target);
	test_assert(framebuffer && glIsFramebuffer(framebuffer));
	test_assert(SDL_GL_MakeCurrent(NULL, NULL));
	ecs_remove(world, e_state, AppDrawPickState);
	current_window = SDL_GL_GetCurrentWindow();
	current_context = SDL_GL_GetCurrentContext();
	test_assert(!current_window && !current_context);
	test_assert(SDL_GL_MakeCurrent(state_window, context));
	test_assert(!glIsFramebuffer(framebuffer));
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(state_window);

	SDL_Window *window = SDL_CreateWindow("pick target window removal", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(window != NULL);
	context = SDL_GL_CreateContext(window);
	test_assert(context != NULL);
	ecs_entity_t e_window = ecs_new(world);
	ecs_set(world, e_window, EgWindowsWindow, {.object = window});
	ecs_set(world, e_window, EgWindowsOpenGLContext, {.gl_context = context, .cleanup = EgWindowsSdlGlCleanup});
	ecs_set(world, e_window, AppDrawPickState,
		{.frame = -1});
	state = ecs_get_mut(world, e_window, AppDrawPickState);
	state->target = egg_pick_target_create();
	test_assert(state->target != NULL);
	test_assert(egg_pick_target_begin_region(state->target, 64, 64, 32, 32));
	framebuffer = egg_pick_target_framebuffer(state->target);
	test_assert(framebuffer && glIsFramebuffer(framebuffer));
	test_assert(SDL_GL_MakeCurrent(NULL, NULL));
	ecs_remove(world, e_window, EgWindowsWindow);
	state = ecs_get_mut(world, e_window, AppDrawPickState);
	test_assert(state && state->target == NULL);
	current_window = SDL_GL_GetCurrentWindow();
	current_context = SDL_GL_GetCurrentContext();
	test_assert(!current_window && !current_context);
	ecs_remove(world, e_window, EgWindowsOpenGLContext);
	ecs_remove(world, e_window, AppDrawPickState);
	test_assert(SDL_GL_MakeCurrent(window, context));
	test_assert(!glIsFramebuffer(framebuffer));
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(window);

	window = SDL_CreateWindow("pick target entity deletion", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(window != NULL);
	context = SDL_GL_CreateContext(window);
	test_assert(context != NULL);
	e_window = ecs_new(world);
	ecs_set(world, e_window, EgWindowsWindow, {.object = window});
	ecs_set(world, e_window, EgWindowsOpenGLContext, {.gl_context = context, .cleanup = EgWindowsSdlGlCleanup});
	ecs_set(world, e_window, AppDrawPickState,
		{.frame = -1});
	state = ecs_get_mut(world, e_window, AppDrawPickState);
	state->target = egg_pick_target_create();
	test_assert(state->target != NULL);
	test_assert(egg_pick_target_begin_region(state->target, 64, 64, 32, 32));
	framebuffer = egg_pick_target_framebuffer(state->target);
	test_assert(framebuffer && glIsFramebuffer(framebuffer));
	test_assert(SDL_GL_MakeCurrent(NULL, NULL));
	ecs_delete(world, e_window);
	current_window = SDL_GL_GetCurrentWindow();
	current_context = SDL_GL_GetCurrentContext();
	test_assert(!current_window && !current_context);
	test_assert(SDL_GL_MakeCurrent(window, context));
	test_assert(!glIsFramebuffer(framebuffer));
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(window);

	window = SDL_CreateWindow("pick target world shutdown", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(window != NULL);
	context = SDL_GL_CreateContext(window);
	test_assert(context != NULL);
	ecs_world_t *shutdown_world = ecs_init();
	ECS_IMPORT(shutdown_world, EgWindowsSdl);
	ECS_IMPORT(shutdown_world, EgWindowsSdlGl);
	ECS_IMPORT(shutdown_world, AppDraw);

	e_window = ecs_new(shutdown_world);
	ecs_set(shutdown_world, e_window, EgWindowsWindow, {.object = window});
	ecs_set(shutdown_world, e_window, EgWindowsOpenGLContext, {.gl_context = context, .cleanup = EgWindowsSdlGlCleanup});
	ecs_set(shutdown_world, e_window, AppDrawPickState,
		{.frame = -1});
	state = ecs_get_mut(shutdown_world, e_window, AppDrawPickState);
	state->target = egg_pick_target_create();
	test_assert(state->target != NULL);
	test_assert(egg_pick_target_begin_region(state->target, 64, 64, 32, 32));
	framebuffer = egg_pick_target_framebuffer(state->target);
	test_assert(framebuffer && glIsFramebuffer(framebuffer));
	test_assert(SDL_GL_MakeCurrent(NULL, NULL));
	ecs_fini(shutdown_world);
	test_assert(SDL_GL_GetCurrentContext() == NULL);
	test_assert(SDL_GL_MakeCurrent(window, context));
	test_assert(!glIsFramebuffer(framebuffer));
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void Snapshot_test_renderer_lifecycle(void)
{
	test_assert(SDL_Init(SDL_INIT_VIDEO));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
	SDL_Window *other = SDL_CreateWindow("other context", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(other != NULL);
	SDL_GLContext other_context = SDL_GL_CreateContext(other);
	test_assert(other_context != NULL);

	for (int mode = 0; mode < 6; ++mode) {
		SDL_Window *native = SDL_CreateWindow("renderer lifecycle", 64, 64, SDL_WINDOW_OPENGL);
		test_assert(native != NULL);
		SDL_GLContext context = SDL_GL_CreateContext(native);
		test_assert(context != NULL);
		ecs_world_t *w = ecs_init();
		ECS_IMPORT(w, EgWindowsSdl);
		ECS_IMPORT(w, EgWindowsSdlGl);
		ECS_IMPORT(w, AppDraw);
		ecs_entity_t window = ecs_new(w);
		ecs_set(w, window, EgWindowsWindow, {.object = native});
		ecs_set(w, window, EgWindowsOpenGLContext, {.gl_context = context, .cleanup = tracked_cleanup});
		ecs_entity_t intermediate = ecs_new(w);
		ecs_add_pair(w, intermediate, EcsChildOf, window);
		ecs_entity_t draw = ecs_new(w);
		ecs_add_pair(w, draw, EcsChildOf, intermediate);
		egg_render_t *renderer = egg_render_init();
		EgDraws_List *draw_list = EgDraws_List_Create();
		test_assert(renderer && draw_list);
		ecs_set(w, draw, AppDrawContext, {.render = renderer, .list = draw_list});
		if (mode == 2) ecs_add_id(w, intermediate, EcsDisabled);
		if (mode == 3) ecs_add_id(w, draw, EcsDisabled);
		renderer_cleanups = 0;
		expected_cleanup_context = context;
		test_assert(SDL_GL_MakeCurrent(other, other_context));
		switch (mode) {
		case 0: ecs_remove(w, draw, AppDrawContext); break;
		case 1: ecs_delete(w, draw); break;
		case 2: ecs_remove(w, window, EgWindowsOpenGLContext); break;
		case 3: ecs_remove(w, window, EgWindowsWindow); break;
		case 4: ecs_delete(w, window); break;
		case 5: ecs_fini(w); w = NULL; break;
		}
		test_int(renderer_cleanups, 1);
		test_assert(SDL_GL_GetCurrentWindow() == other);
		test_assert(SDL_GL_GetCurrentContext() == other_context);
		if (mode == 2 || mode == 3) {
			const AppDrawContext *remaining = ecs_get(w, draw, AppDrawContext);
			test_assert(remaining && !remaining->render);
		}
		if (w) ecs_fini(w);
		test_int(renderer_cleanups, 1);
		SDL_GL_DestroyContext(context);
		SDL_DestroyWindow(native);
	}
	test_assert(SDL_GL_MakeCurrent(other, other_context));
	AppDrawContext draw = {.render = egg_render_init()};
	AppDrawPickState pick = {.target = egg_pick_target_create()};
	test_assert(draw.render && pick.target);
	test_assert(egg_pick_target_begin_region(pick.target, 64, 64, 32, 32));
	GLuint framebuffer = egg_pick_target_framebuffer(pick.target);
	EgWindowsOpenGLContext failing = {.gl_context = other_context, .cleanup = failing_cleanup};
	cleanup_failure_after_destroy = false;
	AppDrawContext_destroy_renderer(&draw, other, &failing);
	AppDrawPickState_destroy_target(&pick, other, &failing);
	test_assert(draw.render && pick.target && glIsFramebuffer(framebuffer));
	cleanup_failure_after_destroy = true;
	AppDrawContext_destroy_renderer(&draw, other, &failing);
	AppDrawPickState_destroy_target(&pick, other, &failing);
	test_assert(!draw.render && !pick.target && !glIsFramebuffer(framebuffer));
	SDL_GL_DestroyContext(other_context);
	SDL_DestroyWindow(other);
	SDL_Quit();
}

static void focus_window(SDL_Window *window)
{
	SDL_RaiseWindow(window);
	SDL_WarpMouseInWindow(window, 32, 32);
	SDL_PumpEvents();
	for (int i = 0; i < 50 && SDL_GetMouseFocus() != window; ++i) {
		SDL_PumpEvents();
		SDL_Delay(10);
	}
	test_assert(SDL_GetMouseFocus() == window);
	EgWindowsSdlUpdateState(world);
}

void Snapshot_test_drag_window_bounds(void)
{
	test_assert(SDL_Init(SDL_INIT_VIDEO));
	SDL_Window *window = SDL_CreateWindow("app drag test", 64, 64, 0);
	test_assert(window != NULL);
	ecs_entity_t e_window = ecs_new(world);
	ecs_set(world, e_window, EgWindowsWindow, {.object = window});
	ecs_add_id(world, e_window, AppDrawGpuPicking);
	focus_window(window);
	SDL_WarpMouseInWindow(window, 24, 28);
	SDL_PumpEvents();
	test_assert(ecs_progress(world, 0));
	const EgWindowsInputState *focus = ecs_singleton_get(world, EgWindowsInputState);
	const EgWindowsWindowState *window_state = ecs_get(world, e_window, EgWindowsWindowState);
	test_assert(focus && focus->mouse_window == e_window && focus->mouse_focus);
	test_assert(window_state && window_state->valid && window_state->mouse_focus);
	test_flt(window_state->mouse_x, 24);
	test_flt(window_state->mouse_y, 28);
	test_int(window_state->width, 64);
	test_int(window_state->height, 64);
	test_assert(window_state->pixel_width > 0 && window_state->pixel_height > 0);
	focus_window(window);
	test_assert(SDL_GetKeyboardFocus() == window);
	test_assert(AppDraw_CaptureBegin(world, NULL));
	test_assert(AppDraw_CaptureValid(world, NULL));

	ecs_entity_t target = entity;
	float x = 0, y = 0;
	test_assert(AppDraw_PickMouse(world, &target, &x, &y, NULL));
	test_assert(target == 0);
	test_flt(x, 32);
	test_flt(y, 32);

	ecs_add_id(world, e_window, EcsDisabled);
	test_assert(!AppDraw_CaptureValid(world, NULL));
	ecs_remove_id(world, e_window, EcsDisabled);
	ecs_add_id(world, e_window, EgWindowsCloseRequest);
	test_assert(!AppDraw_CaptureValid(world, NULL));
	ecs_remove_id(world, e_window, EgWindowsCloseRequest);
	test_assert(AppDraw_CaptureValid(world, NULL));

	test_assert(AppDraw_InsideWindow(0, 0, 64, 64));
	test_assert(AppDraw_InsideWindow(63, 63, 64, 64));
	test_assert(!AppDraw_InsideWindow(-1, 32, 64, 64));
	test_assert(!AppDraw_InsideWindow(64, 32, 64, 64));
	test_assert(!AppDraw_InsideWindow(32, -1, 64, 64));
	test_assert(!AppDraw_InsideWindow(32, 64, 64, 64));
	EgWindowsWindowState *outside = ecs_get_mut(world, e_window, EgWindowsWindowState);
	outside->mouse_x = 64;
	test_assert(!AppDraw_CaptureValid(world, NULL));
	outside->mouse_x = 32;
	test_assert(AppDraw_CaptureValid(world, NULL));
	SDL_WarpMouseInWindow(window, 65, 32);
	for (int i = 0; i < 50 && SDL_GetMouseFocus() == window; ++i) {
		SDL_PumpEvents();
		SDL_Delay(10);
	}
	EgWindowsSdlUpdateState(world);
	test_assert(!AppDraw_CaptureValid(world, NULL));
	AppDraw_CaptureEnd(world, NULL);
	test_assert(!AppDraw_CaptureValid(world, NULL));
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void Snapshot_test_multiple_windows(void)
{
	test_assert(SDL_Init(SDL_INIT_VIDEO));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
	SDL_Window *windows[2];
	SDL_GLContext contexts[2];
	egg_render_t *renderers[2];
	ecs_entity_t window_entities[2], targets[2] = {entity, ecs_new(world)};
	ecs_add_id(world, targets[1], EgUiScriptEventsTarget);
	ecs_singleton_set(world, EgButtonsState, {{0}});
	ecs_vec_t *layer = ecs_vec_get_t(&list->layers, ecs_vec_t, 0);
	EgDraws_Vertex *v = ecs_vec_first_t(layer, EgDraws_Vertex);
	v[0].position[0] = -1; v[0].position[1] = -1;
	v[1].position[0] = 1; v[1].position[1] = -1;
	v[2].position[0] = 0; v[2].position[1] = 1;
	for (int j = 0; j < 3; ++j) {
		v[j].uv[0] = 0.5f / EG_DRAWS_ATLAS_WIDTH;
		v[j].uv[1] = 0.5f / EG_DRAWS_ATLAS_HEIGHT;
	}
	for (int i = 0; i < 2; ++i) {
		windows[i] = SDL_CreateWindow("app picking window", 64, 64, SDL_WINDOW_OPENGL);
		test_assert(windows[i] != NULL);
		contexts[i] = SDL_GL_CreateContext(windows[i]);
		test_assert(contexts[i] != NULL);
		renderers[i] = egg_render_init();
		test_assert(renderers[i] != NULL);
		window_entities[i] = ecs_new(world);
		ecs_set(world, window_entities[i], EgWindowsWindow, {.object = windows[i]});
		ecs_set(world, window_entities[i], EgWindowsOpenGLContext, {.gl_context = contexts[i], .cleanup = EgWindowsSdlGlCleanup});
		ecs_add_id(world, window_entities[i], AppDrawGpuPicking);
		ecs_set(world, window_entities[i], AppDrawPickState,
			{.frame = -1});
	}
	test_assert(SDL_GL_MakeCurrent(windows[1], contexts[1]));
	ecs_entity_t result = 0;
	float x = 0, y = 0;
	focus_window(windows[0]);
	for (int j = 0; j < 3; ++j) v[j].pick_id = (uint32_t)targets[0];
	test_assert(SDL_GL_MakeCurrent(windows[0], contexts[0]));
	complete_pick(window_entities[0], renderers[0]);
	test_assert(SDL_GL_MakeCurrent(windows[1], contexts[1]));
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == targets[0]);
	test_assert(SDL_GL_GetCurrentContext() == contexts[1]);
	test_assert(AppDraw_CaptureBegin(world, NULL));
	focus_window(windows[1]);
	test_assert(!AppDraw_CaptureValid(world, NULL));
	AppDraw_CaptureEnd(world, NULL);
	for (int j = 0; j < 3; ++j) v[j].pick_id = (uint32_t)targets[1];
	complete_pick(window_entities[1], renderers[1]);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == targets[1]);
	focus_window(windows[0]);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == targets[0]);
	SDL_Window *external = SDL_CreateWindow("external window", 64, 64, 0);
	test_assert(external != NULL);
	focus_window(external);
	const EgWindowsInputState *input = ecs_singleton_get(world, EgWindowsInputState);
	test_assert(input && input->mouse_focus && !input->mouse_window);
	test_assert(!AppDraw_PickMouse(world, &result, &x, &y, NULL));
	SDL_DestroyWindow(external);
	for (int i = 0; i < 2; ++i) {
		int other = 1 - i;
		test_assert(SDL_GL_MakeCurrent(windows[other], contexts[other]));
		if (i == 0) {
			ecs_remove(world, window_entities[i], AppDrawPickState);
		} else {
			ecs_remove(world, window_entities[i], EgWindowsOpenGLContext);
			ecs_remove(world, window_entities[i], AppDrawPickState);
		}
		test_assert(SDL_GL_GetCurrentContext() == contexts[other]);
	}
	for (int i = 0; i < 2; ++i) {
		test_assert(SDL_GL_MakeCurrent(windows[i], contexts[i]));
		egg_render_destroy(renderers[i]);
		SDL_GL_DestroyContext(contexts[i]);
		SDL_DestroyWindow(windows[i]);
	}
	SDL_Quit();
}

void Snapshot_test_frame_callbacks(void)
{
	test_assert(SDL_Init(SDL_INIT_VIDEO));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
	SDL_Window *window = SDL_CreateWindow("app frame callback test", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(window != NULL);
	SDL_GLContext context = SDL_GL_CreateContext(window);
	test_assert(context != NULL);
	test_assert(SDL_GL_GetCurrentWindow() == window && SDL_GL_GetCurrentContext() == context);
	test_assert(SDL_GL_MakeCurrent(NULL, NULL));
	test_assert(SDL_GL_GetCurrentContext() == NULL);
	test_assert(SDL_GL_MakeCurrent(window, context));
	ecs_entity_t e_window = ecs_new(world);
	ecs_query_t *systems = ecs_query(world, {
		.terms = {{.id = EcsSystem}},
		.group_by = EcsChildOf,
	});
	test_assert(systems != NULL);
	ecs_set(world, e_window, EgWindowsWindow, {.object = window});
	ecs_set(world, e_window, EgWindowsOpenGLContext, {.gl_context = context, .inner_systems = systems});
	ecs_set(world, e_window, EgShapesRectangle, {.w = 64, .h = 64});
	ecs_set(world, e_window, EgWindowsFrameCallback,
		{.begin = generic_frame_begin, .end = generic_frame_end, .complete = generic_frame_complete});
	ecs_set(world, e_window, EgWindowsSdlGlFrameCallback, {.complete = sdl_frame_complete});
	generic_frames = generic_begins = generic_ends = sdl_frames = 0;
	test_assert(ecs_progress(world, 0));
	test_int(generic_begins, 1);
	test_int(generic_ends, 1);
	test_int(generic_frames, 1);
	test_int(sdl_frames, 1);
	ecs_remove(world, e_window, EgWindowsOpenGLContext);
	ecs_query_fini(systems);
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void Snapshot_test_render_cycle_picking(void)
{
	test_assert(SDL_Init(SDL_INIT_VIDEO));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
	SDL_Window *native = SDL_CreateWindow("render cycle picking", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(native != NULL);
	SDL_GLContext context = SDL_GL_CreateContext(native);
	test_assert(context != NULL);
	ECS_IMPORT(world, EgCameras);
	ecs_entity_t window = ecs_new(world);
	ecs_query_t *systems = ecs_query(world, {.terms = {{.id = EcsSystem}}, .group_by = EcsChildOf});
	test_assert(systems != NULL);
	ecs_set(world, window, EgWindowsWindow, {.object = native});
	ecs_set(world, window, EgWindowsOpenGLContext,
		{.gl_context = context, .inner_systems = systems, .cleanup = EgWindowsSdlGlCleanup});
	ecs_set(world, window, EgShapesRectangle, {.w = 64, .h = 64});
	ecs_add_id(world, window, AppDrawGpuPicking);
	ecs_set(world, window, AppDrawPickState, {.frame = -1});
	ecs_set(world, window, EgWindowsFrameCallback,
		{.begin = AppDrawPickFrame_begin, .end = AppDrawPickFrame_end, .complete = AppDrawPickFrame_complete});
	ecs_entity_t camera = ecs_new(world);
	ecs_set(world, camera, EgCamerasState,
		{.pixel_scale = 1, .vp = {{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1}}});
	ecs_entity_t draw = ecs_new(world);
	ecs_add_pair(world, draw, EcsChildOf, window);
	ecs_add_pair(world, draw, EcsDependsOn, camera);
	EgDraws_List *draw_list = EgDraws_List_Create();
	test_assert(draw_list != NULL);
	ecs_vec_t *layer = EgDraws_List_GetLayer(draw_list, 0);
	EgDraws_Vertex *v = ecs_vec_grow_t(NULL, layer, EgDraws_Vertex, 3);
	test_assert(v != NULL);
	for (int i = 0; i < 3; ++i) {
		v[i] = (EgDraws_Vertex){.rgba = {255,255,255,255}, .pick_id = (uint32_t)entity,
			.uv = {0.5f / EG_DRAWS_ATLAS_WIDTH, 0.5f / EG_DRAWS_ATLAS_HEIGHT}};
	}
	v[0].position[0] = -1; v[0].position[1] = -1;
	v[1].position[0] = 1; v[1].position[1] = -1;
	v[2].position[0] = 0; v[2].position[1] = 1;
	ecs_set(world, draw, AppDrawContext, {.list = draw_list, .pixelScale = 1});
	ecs_entity_t system = ecs_entity(world, {.name = "RenderCyclePickingTest", .parent = window});
	ecs_system_init(world, &(ecs_system_desc_t){
		.entity = system, .callback = Test_Render,
		.query.terms = {
			{.id = ecs_childof(window)},
			{.id = ecs_id(AppDrawContext), .src.id = EcsSelf, .inout = EcsIn},
			{.id = ecs_id(EgCamerasState), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn}
		}});
	focus_window(native);
	test_assert(ecs_progress(world, 0));
	const AppDrawContext *drawn = ecs_get(world, draw, AppDrawContext);
	test_assert(drawn && drawn->render);
	const AppDrawPickState *state = ecs_get(world, window, AppDrawPickState);
	test_assert(state && state->cached && state->picked == entity);
	ecs_entity_t result = 0;
	float x = 0, y = 0;
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	ecs_delete(world, window);
	ecs_query_fini(systems);
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(native);
	SDL_Quit();
}
