#include <bake_test.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <EgWindowsSdl.h>
#include <EgWindowsSdlGl.h>
#include "../../../src/AppDraw.c"

static ecs_world_t  *world;
static ecs_entity_t  entity;
static EgDraws_List *list;
static const float   identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
static int           generic_frames, generic_begins, generic_ends;
static int           renderer_cleanups;
static int           unrelated_runs;
static ecs_entity_t  frame_windows[2];
static int           frame_results[2], displayed_results[2];
static bool          invalidate_snapshot;
static int           failed_executor_runs, failed_executor_completions;
static SDL_GLContext expected_cleanup_context;
static bool          cleanup_failure_after_destroy;

static void unrelated_system(ecs_iter_t *it)
{
	(void)it;
	++unrelated_runs;
}

static void change_color_after_prepare(ecs_iter_t *it)
{
	EgDraws_Solid *solids = ecs_field(it, EgDraws_Solid, 0);
	for (int32_t i = 0; i < it->count; ++i)
		solids[i].color = 0xffff0000;
}

static void observed_frame_begin(ecs_world_t *w, ecs_entity_t window)
{
	const EgWindowsWindow *native = ecs_get(w, window, EgWindowsWindow);
	const EgWindowsOpenGLContext *gl = ecs_get(w, window, EgWindowsOpenGLContext);
	test_assert(native && gl);
	test_assert(SDL_GL_GetCurrentWindow() == native->object);
	test_assert(SDL_GL_GetCurrentContext() == gl->gl_context);
	if (invalidate_snapshot && window == frame_windows[0]) {
		EgWindowsWindow *mutable_native = ecs_get_mut(w, window, EgWindowsWindow);
		++mutable_native->pixel_width;
	}
}

static void observed_frame_complete(ecs_world_t *w, ecs_entity_t window, bool displayed)
{
	(void)w;
	for (int i = 0; i < 2; ++i) {
		if (window == frame_windows[i]) {
			++frame_results[i];
			if (displayed)
				++displayed_results[i];
		}
	}
}

static bool invalid_context_execute(ecs_world_t *w, ecs_entity_t window)
{
	(void)w;
	(void)window;
	++failed_executor_runs;
	return true;
}

static void invalid_context_complete(ecs_world_t *w, ecs_entity_t window, bool displayed)
{
	(void)w;
	(void)window;
	test_assert(!displayed);
	++failed_executor_completions;
}

static void refresh_window_state(void);

static bool supports_async(void)
{
	return SDL_GL_ExtensionSupported("GL_ARB_gpu_shader_int64") &&
	       SDL_GL_ExtensionSupported("GL_NV_shader_atomic_int64");
}

static void report_async_skip(void)
{
	fprintf(stderr, "AppDraw_test: skipping GPU picking checks; 64-bit atomic extensions unavailable\n");
}

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
	const EgWindowsWindow        *native = ecs_get(w, window, EgWindowsWindow);
	const EgWindowsOpenGLContext *gl     = ecs_get(w, window, EgWindowsOpenGLContext);
	test_assert(native && gl);
	test_assert(SDL_GL_GetCurrentWindow() == native->object);
	test_assert(SDL_GL_GetCurrentContext() == gl->gl_context);
	test_int(generic_begins, generic_frames);
	++generic_begins;
}

static void generic_frame_end(ecs_world_t *w, ecs_entity_t window)
{
	const EgWindowsWindow        *native = ecs_get(w, window, EgWindowsWindow);
	const EgWindowsOpenGLContext *gl     = ecs_get(w, window, EgWindowsOpenGLContext);
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
		v[i] = (EgDraws_Vertex){.position = {(float)i, 0}, .rgba = {255, 255, 255, 255}, .pick_id = (uint32_t)entity};
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
	state       = ecs_get_mut(world, window, AppDrawPickState);
	uint64_t id = 0, frame = 0;
	int      ready = 0;
	for (int i = 0; i < 1000 && !ready; ++i) {
		ready = egg_pick_async_poll(state->async, &id, &frame);
		if (!ready)
			SDL_Delay(1);
	}
	test_int(ready, 1);
	state->picked = id;
	state->frame  = (int64_t)frame;
	state->cached = true;
}

void Snapshot_test_publish_and_invalidate(void)
{
	ecs_entity_t window = ecs_new(world);
	ecs_add_id(world, window, AppDrawGpuPicking);
	ecs_set(world, window, AppDrawPickState, {.frame = -1});
	AppDrawPickState *state = ecs_get_mut(world, window, AppDrawPickState);
	state->cached           = true;
	state->picked           = entity;
	AppDrawPickFrame_complete(world, window, false);
	test_assert(state->cached && state->picked == entity);
	AppDrawPickFrame_begin(world, window);
	test_assert(!state->drawing);
	test_assert(!state->cached && !state->picked);
	AppDrawPickFrame_complete(world, window, true);
	test_assert(!state->cached);
	state->cached = true;
	state->picked = entity;
	ecs_remove_id(world, window, AppDrawGpuPicking);
	state = ecs_get_mut(world, window, AppDrawPickState);
	test_assert(state && !state->cached && !state->picked);
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
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
	SDL_Window *window = SDL_CreateWindow("app picking test", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(window != NULL);
	SDL_GLContext context = SDL_GL_CreateContext(window);
	test_assert(context != NULL);
	if (!supports_async()) {
		report_async_skip();
		SDL_GL_DestroyContext(context);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return;
	}
	egg_render_t *renderer = egg_render_init();
	test_assert(renderer != NULL);
	ecs_entity_t e_window = ecs_new(world);
	ecs_set(world, e_window, EgWindowsWindow, {.object = window});
	ecs_set(world, e_window, EgWindowsOpenGLContext, {.gl_context = context, .cleanup = EgWindowsSdlGlCleanup});
	ecs_add_id(world, e_window, AppDrawGpuPicking);
	ecs_set(world, e_window, AppDrawPickState,
	{.frame = -1});
	ecs_singleton_set(world, EgButtonsState, {{0}});
	ecs_vec_t      *layer = ecs_vec_get_t(&list->layers, ecs_vec_t, 0);
	EgDraws_Vertex *v     = ecs_vec_first_t(layer, EgDraws_Vertex);
	v[0].position[0]      = -1;
	v[0].position[1]      = -1;
	v[1].position[0]      = 1;
	v[1].position[1]      = -1;
	v[2].position[0]      = 0;
	v[2].position[1]      = 1;
	for (int i = 0; i < 3; ++i) {
		v[i].uv[0] = 0.5f / EG_DRAWS_ATLAS_WIDTH;
		v[i].uv[1] = 0.5f / EG_DRAWS_ATLAS_HEIGHT;
	}
	AppDrawPickState *state = ecs_get_mut(world, e_window, AppDrawPickState);
	SDL_RaiseWindow(window);
	SDL_WarpMouseInWindow(window, 32, 32);
	for (int i = 0; i < 50 && SDL_GetMouseFocus() != window; ++i) {
		SDL_PumpEvents();
		SDL_Delay(10);
	}
	test_assert(SDL_GetMouseFocus() == window);
	refresh_window_state();
	complete_pick(e_window, renderer);
	state               = ecs_get_mut(world, e_window, AppDrawPickState);
	ecs_entity_t result = 0;
	float        x = 0, y = 0;
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	test_assert(state->cached);

	ecs_entity_t top = ecs_new(world);
	ecs_add_id(world, top, EgUiScriptEventsTarget);
	AppDrawPickFrame_begin(world, e_window);
	AppDrawPickFrame_draw(world, state, renderer, list, identity);
	for (int i = 0; i < 3; ++i)
		v[i].pick_id = (uint32_t)top;
	AppDrawPickFrame_draw(world, state, renderer, list, identity);
	AppDrawPickFrame_end(world, e_window);
	AppDrawPickFrame_complete(world, e_window, true);
	uint64_t id = 0, frame = 0;
	int      ready = 0;
	for (int i = 0; i < 1000 && !ready; ++i) {
		ready = egg_pick_async_poll(state->async, &id, &frame);
		if (!ready)
			SDL_Delay(1);
	}
	test_int(ready, 1);
	state->picked = id;
	state->frame  = (int64_t)frame;
	state->cached = true;
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == top);
	for (int i = 0; i < 3; ++i)
		v[i].pick_id = (uint32_t)entity;
	complete_pick(e_window, renderer);

	EgWindowsOpenGLContext *gl = ecs_get_mut(world, e_window, EgWindowsOpenGLContext);
	gl->gl_context             = NULL;
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == 0 && !state->cached);
	gl->gl_context = context;
	complete_pick(e_window, renderer);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity && state->cached);

	SDL_WarpMouseInWindow(window, 2, 32);
	SDL_PumpEvents();
	refresh_window_state();
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	complete_pick(e_window, renderer);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == 0);
	SDL_WarpMouseInWindow(window, 32, 32);
	SDL_PumpEvents();
	refresh_window_state();
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
	for (int i = 0; i < 3; ++i)
		v[i].pick_id = (uint32_t)entity;
	complete_pick(e_window, renderer);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	for (int i = 0; i < 3; ++i)
		v[i].position[0] += 4;
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
	refresh_window_state();
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == 0);
	test_assert(!state->cached);
	state->cached  = true;
	state->drawing = true;
	AppDrawPickFrame_complete(world, e_window, false);
	test_assert(state->cached && !state->drawing);
	AppDrawPickFrame_complete(world, e_window, true);
	test_assert(state->cached);

	SDL_Window   *current_window  = SDL_GL_GetCurrentWindow();
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
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));

	for (int mode = 0; mode < 5; ++mode) {
		SDL_Window *window = SDL_CreateWindow("asynchronous pick lifecycle", 64, 64, SDL_WINDOW_OPENGL);
		test_assert(window != NULL);
		SDL_GLContext context = SDL_GL_CreateContext(window);
		test_assert(context != NULL);
		if (!supports_async()) {
			report_async_skip();
			SDL_GL_DestroyContext(context);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return;
		}
		ecs_world_t *w = ecs_init();
		ECS_IMPORT(w, EgWindowsSdl);
		ECS_IMPORT(w, EgWindowsSdlGl);
		ECS_IMPORT(w, AppDraw);
		ecs_entity_t e = ecs_new(w);
		ecs_set(w, e, EgWindowsWindow, {.object = window});
		ecs_set(w, e, EgWindowsOpenGLContext, {.gl_context = context, .cleanup = tracked_cleanup});
		ecs_set(w, e, AppDrawPickState, {.frame = -1});
		AppDrawPickState *state = ecs_get_mut(w, e, AppDrawPickState);
		state->async            = egg_pick_async_create();
		test_assert(state->async != NULL);
		test_int(egg_pick_async_begin(state->async, 32, 32, 1), 1);
		test_assert(egg_pick_async_end(state->async));
		egg_pick_async_complete(state->async, true);
		renderer_cleanups        = 0;
		expected_cleanup_context = context;
		test_assert(SDL_GL_MakeCurrent(NULL, NULL));
		switch (mode) {
		case 0:
			ecs_remove(w, e, AppDrawPickState);
			break;
		case 1:
			ecs_remove(w, e, EgWindowsOpenGLContext);
			break;
		case 2:
			ecs_remove(w, e, EgWindowsWindow);
			break;
		case 3:
			ecs_delete(w, e);
			break;
		case 4:
			ecs_fini(w);
			w = NULL;
			break;
		}
		test_int(renderer_cleanups, 1);
		test_assert(!SDL_GL_GetCurrentWindow() && !SDL_GL_GetCurrentContext());
		if (mode == 1 || mode == 2) {
			state = ecs_get_mut(w, e, AppDrawPickState);
			test_assert(state && !state->async);
		}
		if (w)
			ecs_fini(w);
		test_int(renderer_cleanups, 1);
		SDL_GL_DestroyContext(context);
		SDL_DestroyWindow(window);
	}
	SDL_Quit();
}

void Snapshot_test_renderer_lifecycle(void)
{
	test_assert(SDL_Init(SDL_INIT_VIDEO));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4));
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
		test_assert(renderer);
		ecs_add(w, draw, EgDraws_List);
		ecs_set(w, draw, AppDrawContext, {.render = renderer});
		if (mode == 2)
			ecs_add_id(w, intermediate, EcsDisabled);
		if (mode == 3)
			ecs_add_id(w, draw, EcsDisabled);
		renderer_cleanups        = 0;
		expected_cleanup_context = context;
		test_assert(SDL_GL_MakeCurrent(other, other_context));
		switch (mode) {
		case 0:
			ecs_remove(w, draw, AppDrawContext);
			test_assert(ecs_has(w, draw, EgDraws_List));
			break;
		case 1:
			ecs_delete(w, draw);
			break;
		case 2:
			ecs_remove(w, window, EgWindowsOpenGLContext);
			break;
		case 3:
			ecs_remove(w, window, EgWindowsWindow);
			break;
		case 4:
			ecs_delete(w, window);
			break;
		case 5:
			ecs_fini(w);
			w = NULL;
			break;
		}
		test_int(renderer_cleanups, 1);
		test_assert(SDL_GL_GetCurrentWindow() == other);
		test_assert(SDL_GL_GetCurrentContext() == other_context);
		if (mode == 2 || mode == 3) {
			const AppDrawContext *remaining = ecs_get(w, draw, AppDrawContext);
			test_assert(remaining && !remaining->render);
		}
		if (w)
			ecs_fini(w);
		test_int(renderer_cleanups, 1);
		SDL_GL_DestroyContext(context);
		SDL_DestroyWindow(native);
	}
	test_assert(SDL_GL_MakeCurrent(other, other_context));
	AppDrawContext draw = {.render = egg_render_init()};
	if (!supports_async()) {
		report_async_skip();
		test_assert(draw.render);
		egg_render_destroy(draw.render);
		SDL_GL_DestroyContext(other_context);
		SDL_DestroyWindow(other);
		SDL_Quit();
		return;
	}
	AppDrawPickState pick = {.async = egg_pick_async_create()};
	test_assert(draw.render && pick.async);
	EgWindowsOpenGLContext failing = {.gl_context = other_context, .cleanup = failing_cleanup};
	cleanup_failure_after_destroy  = false;
	AppDrawContext_destroy_renderer(&draw, other, &failing);
	AppDrawPickState_destroy_target(&pick, other, &failing);
	test_assert(draw.render && pick.async);
	cleanup_failure_after_destroy = true;
	AppDrawContext_destroy_renderer(&draw, other, &failing);
	AppDrawPickState_destroy_target(&pick, other, &failing);
	test_assert(!draw.render && !pick.async);
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
	refresh_window_state();
}

static void refresh_window_state(void)
{
	ecs_entity_t events_update = 0;
	ecs_entity_t window_update = 0;
	ecs_iter_t   systems       = ecs_each_id(world, EcsSystem);
	ecs_iter_t  *iter          = &systems;
	while (ecs_each_next(iter)) {
		for (int32_t i = 0; i < iter->count; ++i) {
			const char *name = ecs_get_name(world, iter->entities[i]);
			if (name && !strcmp(name, "System_Events_Update"))
				events_update = iter->entities[i];
			else if (name && !strcmp(name, "System_EgWindowsWindow_UpdateState"))
				window_update = iter->entities[i];
		}
		if (events_update && window_update) {
			ecs_iter_fini(iter);
			break;
		}
	}
	test_assert(events_update != 0 && window_update != 0);
	ecs_run(world, events_update, 0, NULL);
	ecs_run(world, window_update, 0, NULL);
}

void Snapshot_test_window_state_system(void)
{
	test_assert(SDL_Init(SDL_INIT_VIDEO));
	SDL_Window *window = SDL_CreateWindow("window state system test", 64, 48, 0);
	test_assert(window != NULL);
	ecs_entity_t e_window = ecs_new(world);
	ecs_set(world, e_window, EgWindowsWindow, {.object = window});
	refresh_window_state();
	const EgWindowsWindow *state = ecs_get(world, e_window, EgWindowsWindow);
	test_assert(state && state->valid);
	test_assert(ecs_progress(world, 0));
	state = ecs_get(world, e_window, EgWindowsWindow);
	test_assert(state && state->valid);
	test_int(state->width, 64);
	test_int(state->height, 48);
	test_assert(state->pixel_width > 0 && state->pixel_height > 0);
	test_assert(ecs_singleton_get(world, EgWindowsInputState) != NULL);
	ecs_delete(world, e_window);
	SDL_DestroyWindow(window);
	SDL_Quit();
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
	const EgWindowsInputState *focus        = ecs_singleton_get(world, EgWindowsInputState);
	const EgWindowsWindow     *window_state = ecs_get(world, e_window, EgWindowsWindow);
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
	float        x = 0, y = 0;
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
	EgWindowsWindow *outside = ecs_get_mut(world, e_window, EgWindowsWindow);
	outside->mouse_x         = 64;
	test_assert(!AppDraw_CaptureValid(world, NULL));
	outside->mouse_x = 32;
	test_assert(AppDraw_CaptureValid(world, NULL));
	SDL_WarpMouseInWindow(window, 65, 32);
	for (int i = 0; i < 50 && SDL_GetMouseFocus() == window; ++i) {
		SDL_PumpEvents();
		SDL_Delay(10);
	}
	refresh_window_state();
	test_assert(!AppDraw_CaptureValid(world, NULL));
	AppDraw_CaptureEnd(world, NULL);
	test_assert(!AppDraw_CaptureValid(world, NULL));
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void Snapshot_test_multiple_windows(void)
{
	test_assert(SDL_Init(SDL_INIT_VIDEO));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
	SDL_Window   *windows[2];
	SDL_GLContext contexts[2];
	egg_render_t *renderers[2];
	ecs_entity_t  window_entities[2], targets[2] = {entity, ecs_new(world)};
	ecs_add_id(world, targets[1], EgUiScriptEventsTarget);
	ecs_singleton_set(world, EgButtonsState, {{0}});
	ecs_vec_t      *layer = ecs_vec_get_t(&list->layers, ecs_vec_t, 0);
	EgDraws_Vertex *v     = ecs_vec_first_t(layer, EgDraws_Vertex);
	v[0].position[0]      = -1;
	v[0].position[1]      = -1;
	v[1].position[0]      = 1;
	v[1].position[1]      = -1;
	v[2].position[0]      = 0;
	v[2].position[1]      = 1;
	for (int j = 0; j < 3; ++j) {
		v[j].uv[0] = 0.5f / EG_DRAWS_ATLAS_WIDTH;
		v[j].uv[1] = 0.5f / EG_DRAWS_ATLAS_HEIGHT;
	}
	for (int i = 0; i < 2; ++i) {
		windows[i] = SDL_CreateWindow("app picking window", 64, 64, SDL_WINDOW_OPENGL);
		test_assert(windows[i] != NULL);
		contexts[i] = SDL_GL_CreateContext(windows[i]);
		test_assert(contexts[i] != NULL);
		if (i == 0 && !supports_async()) {
			report_async_skip();
			SDL_GL_DestroyContext(contexts[i]);
			SDL_DestroyWindow(windows[i]);
			SDL_Quit();
			return;
		}
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
	float        x = 0, y = 0;
	focus_window(windows[0]);
	for (int j = 0; j < 3; ++j)
		v[j].pick_id = (uint32_t)targets[0];
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
	for (int j = 0; j < 3; ++j)
		v[j].pick_id = (uint32_t)targets[1];
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
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4));
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
	ecs_set(world, e_window, EgWindowsWindow, {.object = window});
	ecs_set(world, e_window, EgWindowsOpenGLContext, {.gl_context = context});
	ecs_set(world, e_window, EgShapesRectangle, {.w = 64, .h = 64});
	ecs_set(world, e_window, EgWindowsFrameCallback,
	{.begin = generic_frame_begin, .end = generic_frame_end, .complete = generic_frame_complete});
	generic_frames = generic_begins = generic_ends = 0;
	test_assert(ecs_progress(world, 0));
	test_int(generic_begins, 1);
	test_int(generic_ends, 1);
	test_int(generic_frames, 1);
	ecs_remove(world, e_window, EgWindowsOpenGLContext);
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void Snapshot_test_render_cycle_picking(void)
{
	test_assert(SDL_Init(SDL_INIT_VIDEO));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
	SDL_Window *native = SDL_CreateWindow("render cycle picking", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(native != NULL);
	SDL_GLContext context = SDL_GL_CreateContext(native);
	test_assert(context != NULL);
	ECS_IMPORT(world, EgCameras);
	ecs_entity_t window  = ecs_new(world);
	ecs_set(world, window, EgWindowsWindow, {.object = native});
	ecs_set(world, window, EgWindowsOpenGLContext,
	{.gl_context = context, .cleanup = EgWindowsSdlGlCleanup});
	ecs_set(world, window, AppFrameClear, {.b = 1, .a = 1});
	ecs_set(world, window, EgShapesRectangle, {.w = 64, .h = 64});
	ecs_add_id(world, window, AppDrawGpuPicking);
	ecs_set(world, window, AppDrawPickState, {.frame = -1});
	ecs_entity_t camera = ecs_new(world);
	ecs_set(world, camera, EgCamerasState,
	{.pixel_scale = 1, .vp = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}});
	ecs_entity_t draw = ecs_new(world);
	ecs_add_pair(world, draw, EcsChildOf, window);
	ecs_set(world, draw, AppFramePass, {.order = 10});
	ecs_add_pair(world, draw, AppFrameCamera, camera);
	ecs_entity_t source = ecs_new(world);
	ecs_add(world, source, EgDraws_List);
	ecs_add_pair(world, source, EgDraws_Camera, camera);
	ecs_add_pair(world, draw, AppFrameSource, source);
	ecs_add_pair(world, entity, EcsChildOf, source);
	ecs_set(world, entity, EgDraws_Solid, {.color = 0xffffffff});
	ecs_set(world, entity, EgShapesRectangle, {.w = 2, .h = 2});
	ecs_set(world, entity, WorldTransform3, {.matrix = M3_IDENTITY});
	focus_window(native);
	test_assert(ecs_progress(world, 0));
	const AppDrawFrame *compiled = ecs_get(world, window, AppDrawFrame);
	test_assert(compiled != NULL);
	test_int(compiled->result, AppFrameReady);
	test_int(compiled->plan.instruction_count, 3);
	test_assert(ecs_has(world, window, EgWindowsFrameExecutor));
	const AppDrawContext *drawn = ecs_get(world, draw, AppDrawContext);
	test_assert(drawn && drawn->render);
	const AppDrawPickState *state = ecs_get(world, window, AppDrawPickState);
	if (!supports_async()) {
		test_assert(state && state->unavailable && !state->cached);
		ecs_delete(world, window);
		SDL_GL_DestroyContext(context);
		SDL_DestroyWindow(native);
		SDL_Quit();
		return;
	}
	for (int i = 0; i < 1000 && !state->cached; ++i) {
		SDL_Delay(1);
		test_assert(ecs_progress(world, 0));
		state = ecs_get(world, window, AppDrawPickState);
	}
	test_assert(state && state->cached && state->picked == entity);
	ecs_entity_t result = 0;
	float        x = 0, y = 0;
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	ecs_remove(world, entity, EgDraws_Solid);
	for (int i = 0; i < 1000 && state->picked; ++i) {
		SDL_Delay(1);
		test_assert(ecs_progress(world, 0));
		state = ecs_get(world, window, AppDrawPickState);
	}
	test_assert(state->cached && !state->picked);
	ecs_delete(world, window);
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(native);
	SDL_Quit();
}

void Snapshot_test_frame_plan_two_windows(void)
{
	ecs_entity_t windows[2], passes[2][2];
	SDL_WindowID ids[2];
	for (int i = 0; i < 2; ++i) {
		windows[i] = ecs_new(world);
		frame_windows[i] = windows[i];
		frame_results[i] = displayed_results[i] = 0;
		ecs_set(world, windows[i], EgWindowsWindowCreateInfo, {.enable_opengl = true});
		ecs_set(world, windows[i], EgWindowsOpenGLContextCreate, {0});
		ecs_set(world, windows[i], EgShapesRectangle, {.w = 64, .h = 64});
		ecs_set(world, windows[i], AppFrameClear, {.b = 1, .a = 1});
		ecs_set(world, windows[i], EgWindowsFrameCallback,
			{.begin = observed_frame_begin, .complete = observed_frame_complete});
		for (int j = 0; j < 2; ++j) {
			ecs_entity_t camera = ecs_new(world);
			ecs_set(world, camera, EgCamerasState,
				{.pixel_scale = 1, .vp = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}});
			passes[i][j] = ecs_new_w_pair(world, EcsChildOf, windows[i]);
			ecs_set(world, passes[i][j], AppFramePass, {.order = j * 10});
			ecs_add_pair(world, passes[i][j], AppFrameCamera, camera);
			ecs_entity_t root = ecs_new(world);
			ecs_add(world, root, EgDraws_List);
			ecs_add_pair(world, root, EgDraws_Camera, camera);
			ecs_add_pair(world, passes[i][j], AppFrameSource, root);
			ecs_entity_t shape = ecs_new_w_pair(world, EcsChildOf, root);
			ecs_set(world, shape, EgDraws_Solid, {.color = 0xffffffff});
			ecs_set(world, shape, EgShapesRectangle, {.w = 1, .h = 1});
			ecs_set(world, shape, WorldTransform3, {.matrix = M3_IDENTITY});
		}
	}
	ecs_entity_t unrelated = ecs_entity(world, {.name = "unrelated_window_child", .parent = windows[0]});
	ecs_system(world, {.entity = unrelated, .callback = unrelated_system,
		.query.terms = {{.id = ecs_id(EgWindowsWindow)}}});
	unrelated_runs = 0;
	for (int i = 0; i < 2; ++i)
		test_assert(ecs_progress(world, 0));
	test_int(unrelated_runs, 0);
	for (int i = 0; i < 2; ++i) {
		test_int(frame_results[i], 2);
		test_int(displayed_results[i], 2);
		const EgWindowsNativeOwner *owner = ecs_get(world, windows[i], EgWindowsNativeOwner);
		test_assert(owner && owner->window && owner->context && owner->release);
		ids[i] = SDL_GetWindowID(owner->window);
		const AppDrawFrame *frame = ecs_get(world, windows[i], AppDrawFrame);
		test_assert(frame && frame->result == AppFrameReady);
		test_int(frame->plan.instruction_count, 4);
		test_assert(frame->plan.instructions[2].args.draw.pass == passes[i][0]);
		test_assert(frame->plan.instructions[3].args.draw.pass == passes[i][1]);
		for (int j = 0; j < 2; ++j) {
			const AppDrawContext *draw = ecs_get(world, passes[i][j], AppDrawContext);
			test_assert(draw && draw->render);
			ecs_vec_t *layer = EgDraws_List_GetLayer(frame->plan.layers[j], 0);
			test_int(ecs_vec_count(layer), 6);
		}
	}
	ecs_set(world, passes[0][1], AppFramePass, {.order = 0});
	test_assert(ecs_progress(world, 0));
	test_int(frame_results[0], 3);
	test_int(displayed_results[0], 2);
	test_int(frame_results[1], 3);
	test_int(displayed_results[1], 3);
	const AppDrawFrame *invalid = ecs_get(world, windows[0], AppDrawFrame);
	test_assert(invalid && invalid->result == AppFrameInvalid && !invalid->plan.instructions);
	ecs_set(world, passes[0][1], AppFramePass, {.order = 10});
	invalidate_snapshot = true;
	test_assert(ecs_progress(world, 0));
	invalidate_snapshot = false;
	test_int(frame_results[0], 4);
	test_int(displayed_results[0], 2);
	test_int(frame_results[1], 4);
	test_int(displayed_results[1], 4);
	ecs_remove(world, windows[0], EgWindowsOpenGLContext);
	test_assert(SDL_GetWindowFromID(ids[0]) == NULL);
	ecs_delete(world, windows[0]);
	ecs_delete(world, windows[1]);
	test_assert(SDL_GetWindowFromID(ids[1]) == NULL);
}

void Snapshot_test_native_world_fini(void)
{
	ecs_world_t *w = ecs_init();
	ECS_IMPORT(w, EgWindowsSdl);
	ECS_IMPORT(w, EgWindowsSdlGl);
	ECS_IMPORT(w, AppDraw);
	ecs_entity_t window = ecs_new(w);
	ecs_set(w, window, EgWindowsWindowCreateInfo, {.enable_opengl = true});
	ecs_set(w, window, EgWindowsOpenGLContextCreate, {0});
	ecs_set(w, window, EgShapesRectangle, {.w = 64, .h = 64});
	ecs_set(w, window, AppFrameClear, {.b = 1, .a = 1});
	ecs_entity_t camera = ecs_new(w);
	ecs_set(w, camera, EgCamerasState, {
		.pixel_scale = 1, .vp = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}});
	ecs_entity_t pass = ecs_new_w_pair(w, EcsChildOf, window);
	ecs_set(w, pass, AppFramePass, {.order = 1});
	ecs_add_pair(w, pass, AppFrameCamera, camera);
	ecs_entity_t root = ecs_new(w);
	ecs_add(w, root, EgDraws_List);
	ecs_add_pair(w, root, EgDraws_Camera, camera);
	ecs_add_pair(w, pass, AppFrameSource, root);
	test_assert(ecs_progress(w, 0));
	const EgWindowsNativeOwner *owner = ecs_get(w, window, EgWindowsNativeOwner);
	test_assert(owner && owner->window && owner->context);
	SDL_WindowID id = SDL_GetWindowID(owner->window);
	const AppDrawContext *draw = ecs_get(w, pass, AppDrawContext);
	test_assert(draw && draw->render);
	ecs_fini(w);
	test_assert(SDL_GetWindowFromID(id) == NULL);
}

void Snapshot_test_frame_snapshot_order(void)
{
	ecs_entity_t window = ecs_new(world);
	ecs_set(world, window, EgWindowsWindowCreateInfo, {.enable_opengl = true});
	ecs_set(world, window, EgWindowsOpenGLContextCreate, {0});
	ecs_set(world, window, EgShapesRectangle, {.w = 64, .h = 64});
	ecs_set(world, window, AppFrameClear, {.b = 1, .a = 1});
	ecs_entity_t camera = ecs_new(world);
	ecs_set(world, camera, EgCamerasState, {
		.pixel_scale = 1, .vp = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}});
	ecs_entity_t pass = ecs_new_w_pair(world, EcsChildOf, window);
	ecs_set(world, pass, AppFramePass, {.order = 1});
	ecs_add_pair(world, pass, AppFrameCamera, camera);
	ecs_entity_t root = ecs_new(world);
	ecs_add(world, root, EgDraws_List);
	ecs_add_pair(world, root, EgDraws_Camera, camera);
	ecs_add_pair(world, pass, AppFrameSource, root);
	ecs_entity_t shape = ecs_new_w_pair(world, EcsChildOf, root);
	ecs_set(world, shape, EgDraws_Solid, {.color = 0xffffffff});
	ecs_set(world, shape, EgShapesRectangle, {.w = 1, .h = 1});
	ecs_set(world, shape, WorldTransform3, {.matrix = M3_IDENTITY});
	ecs_system(world, {.entity = ecs_entity(world, {.name = "ChangeColorAfterPrepare"}),
		.phase = EcsPreStore, .callback = change_color_after_prepare,
		.query.terms = {{.id = ecs_id(EgDraws_Solid), .inout = EcsInOut}}});
	test_assert(ecs_progress(world, 0));
	const AppDrawFrame *frame = ecs_get(world, window, AppDrawFrame);
	test_assert(frame && frame->result == AppFrameReady);
	ecs_vec_t *layer = EgDraws_List_GetLayer(frame->plan.layers[0], 0);
	const EgDraws_Vertex *vertices = ecs_vec_first_t(layer, EgDraws_Vertex);
	test_int(ecs_vec_count(layer), 6);
	test_int(vertices[0].rgba[1], 255);
	test_assert(ecs_progress(world, 0));
	frame = ecs_get(world, window, AppDrawFrame);
	layer = EgDraws_List_GetLayer(frame->plan.layers[0], 0);
	vertices = ecs_vec_first_t(layer, EgDraws_Vertex);
	test_int(vertices[0].rgba[0], 255);
	test_int(vertices[0].rgba[1], 0);
	test_int(vertices[0].rgba[2], 0);
	ecs_delete(world, window);
}

void Snapshot_test_frame_plan_pick_hardware(void)
{
	ecs_entity_t window = ecs_new(world);
	ecs_set(world, window, EgWindowsWindowCreateInfo, {.enable_opengl = true});
	ecs_set(world, window, EgWindowsOpenGLContextCreate, {0});
	ecs_set(world, window, EgShapesRectangle, {.w = 64, .h = 64});
	ecs_set(world, window, AppFrameClear, {.b = 1, .a = 1});
	ecs_add_id(world, window, AppDrawGpuPicking);
	ecs_entity_t camera = ecs_new(world);
	ecs_set(world, camera, EgCamerasState, {
		.pixel_scale = 1, .vp = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}});
	ecs_entity_t top = 0;
	for (int i = 0; i < 2; ++i) {
		ecs_entity_t pass = ecs_new_w_pair(world, EcsChildOf, window);
		ecs_set(world, pass, AppFramePass, {.order = i});
		ecs_add_pair(world, pass, AppFrameCamera, camera);
		ecs_entity_t root = ecs_new(world);
		ecs_add(world, root, EgDraws_List);
		ecs_add_pair(world, root, EgDraws_Camera, camera);
		ecs_add_pair(world, pass, AppFrameSource, root);
		ecs_entity_t shape = ecs_new_w_pair(world, EcsChildOf, root);
		ecs_add_id(world, shape, EgUiScriptEventsTarget);
		ecs_set(world, shape, EgDraws_Solid, {.color = 0xffffffff});
		ecs_set(world, shape, EgShapesRectangle, {.w = 2, .h = 2});
		ecs_set(world, shape, WorldTransform3, {.matrix = M3_IDENTITY});
		top = shape;
	}
	test_assert(ecs_progress(world, 0));
	EgWindowsWindow *native = ecs_get_mut(world, window, EgWindowsWindow);
	const EgWindowsOpenGLContext *gl = ecs_get(world, window, EgWindowsOpenGLContext);
	test_assert(native && gl && SDL_GL_MakeCurrent(native->object, gl->gl_context));
	if (!supports_async()) {
		report_async_skip();
		ecs_delete(world, window);
		return;
	}
	native->valid = true;
	native->hidden = false;
	native->minimized = false;
	native->mouse_focus = true;
	native->mouse_x = native->width / 2.0f;
	native->mouse_y = native->height / 2.0f;
	EgWindowsInputState *input = ecs_singleton_get_mut(world, EgWindowsInputState);
	input->mouse_window = window;
	input->mouse_focus = true;
	const AppDrawFrame *frame = ecs_get(world, window, AppDrawFrame);
	test_assert(frame && frame->result == AppFrameReady);
	AppDrawPickState *state = ecs_get_mut(world, window, AppDrawPickState);
	test_assert(state != NULL);
	for (int i = 0; i < 1000 && !state->cached; ++i) {
		test_assert(AppDraw_ExecuteFrame(world, window));
		bool displayed = SDL_GL_SwapWindow(native->object);
		test_assert(displayed);
		AppDrawPickFrame_complete(world, window, displayed);
		SDL_Delay(1);
	}
	test_assert(state->cached);
	test_assert(state->picked == top);
	ecs_delete(world, window);
}

void Snapshot_test_external_window_ownership(void)
{
	test_assert(SDL_Init(SDL_INIT_VIDEO));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
	test_assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
	SDL_Window *native = SDL_CreateWindow("external native ownership", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(native != NULL);
	SDL_WindowID id = SDL_GetWindowID(native);
	ecs_entity_t window = ecs_new(world);
	ecs_set(world, window, EgWindowsWindow, {.object = native});
	ecs_set(world, window, EgWindowsOpenGLContextCreate, {0});
	ecs_set(world, window, AppFrameClear, {.b = 1, .a = 1});
	test_assert(ecs_progress(world, 0));
	const EgWindowsNativeOwner *owner = ecs_get(world, window, EgWindowsNativeOwner);
	test_assert(owner && owner->window == native && owner->context && !owner->owns_window);
	ecs_remove(world, window, EgWindowsOpenGLContext);
	test_assert(SDL_GetWindowFromID(id) == native);
	ecs_delete(world, window);
	test_assert(SDL_GetWindowFromID(id) == native);
	SDL_DestroyWindow(native);
	SDL_Quit();
}

void Snapshot_test_missing_context_outcome(void)
{
	test_assert(SDL_Init(SDL_INIT_VIDEO));
	SDL_Window *native = SDL_CreateWindow("missing GL context", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(native != NULL);
	ecs_entity_t window = ecs_new(world);
	ecs_set(world, window, EgWindowsWindow, {.object = native});
	ecs_set(world, window, EgWindowsOpenGLContext, {.gl_context = NULL});
	ecs_set(world, window, EgWindowsFrameExecutor,
		{.execute = invalid_context_execute, .complete = invalid_context_complete});
	failed_executor_runs = failed_executor_completions = 0;
	test_assert(ecs_progress(world, 0));
	test_int(failed_executor_runs, 0);
	test_int(failed_executor_completions, 1);
	ecs_delete(world, window);
	SDL_DestroyWindow(native);
	SDL_Quit();
}
