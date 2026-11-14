#include <bake_test.h>
#include <SDL3/SDL.h>
#include <EgWindowsSdl.h>
#include <EgWindowsSdlGl.h>
#include "../../../src/AppDraw.c"

static ecs_world_t *world;
static ecs_entity_t entity;
static EgDraws_List *list;
static const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
static int generic_frames, sdl_frames;

static void generic_frame_complete(ecs_world_t *w, ecs_entity_t window, bool displayed)
{
	(void)w;
	(void)window;
	if (displayed) ++generic_frames;
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

static void capture(AppDrawPickScene *scene, int64_t frame, const float *projection)
{
	scene->count = 0;
	scene->frame = frame;
	scene->width = 64;
	scene->height = 64;
	scene->valid = true;
	test_assert(AppDrawPickScene_add(scene, NULL, list, projection, world));
}

void Snapshot_test_publish_and_invalidate(void)
{
	AppDrawPickState state = {.shown = {.frame = -1}, .building = {.frame = -1}};
	capture(&state.building, 1, identity);
	test_assert(AppDrawPickState_publish(&state, 1));
	test_assert(!state.cached);
	state.cached = true;
	capture(&state.building, 2, identity);
	test_assert(AppDrawPickState_publish(&state, 2));
	test_assert(state.cached);

	ecs_vec_t *layer = ecs_vec_get_t(&list->layers, ecs_vec_t, 0);
	EgDraws_Vertex *vertex = ecs_vec_first_t(layer, EgDraws_Vertex);
	vertex[0].position[0] += 1;
	capture(&state.building, 3, identity);
	test_assert(AppDrawPickState_publish(&state, 3));
	test_assert(!state.cached);
	state.cached = true;
	float changed_projection[16];
	memcpy(changed_projection, identity, sizeof(changed_projection));
	changed_projection[12] = 2;
	capture(&state.building, 4, changed_projection);
	test_assert(AppDrawPickState_publish(&state, 4));
	test_assert(!state.cached);
	state.cached = true;
	capture(&state.building, 5, changed_projection);
	state.building.width = 32;
	test_assert(AppDrawPickState_publish(&state, 5));
	test_assert(!state.cached);
	state.cached = true;
	capture(&state.building, 6, changed_projection);
	state.building.width = 32;
	test_assert(AppDrawPickScene_add(&state.building, NULL, list, identity, world));
	test_assert(AppDrawPickState_publish(&state, 6));
	test_assert(!state.cached);
	state.cached = true;
	test_assert(!AppDrawPickState_publish(&state, 7));
	test_assert(!state.cached);
	AppDrawPickScene_free(&state.shown);
	AppDrawPickScene_free(&state.building);
}

void Snapshot_test_live_eligibility(void)
{
	AppDrawPickScene scene = {0};
	capture(&scene, 1, identity);
	test_assert(!AppDrawPickScene_update_eligibility(&scene, world));
	ecs_remove_id(world, entity, EgUiScriptEventsTarget);
	test_assert(AppDrawPickScene_update_eligibility(&scene, world));
	test_assert(!AppDrawPickScene_update_eligibility(&scene, world));
	ecs_add_id(world, entity, EgUiScriptEventsTarget);
	test_assert(AppDrawPickScene_update_eligibility(&scene, world));
	ecs_entity_t parent = ecs_new(world);
	ecs_add_pair(world, entity, EcsChildOf, parent);
	ecs_add_id(world, parent, EcsDisabled);
	test_assert(AppDrawPickScene_update_eligibility(&scene, world));
	ecs_remove_id(world, parent, EcsDisabled);
	test_assert(AppDrawPickScene_update_eligibility(&scene, world));
	ecs_delete(world, entity);
	test_assert(AppDrawPickScene_update_eligibility(&scene, world));
	AppDrawPickScene_free(&scene);
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
	ecs_set(world, e_window, EgWindowsOpenGLContext, {.gl_context = context});
	ecs_add_id(world, e_window, AppDrawGpuPicking);
	ecs_set(world, e_window, AppDrawPickState,
		{.gl = *ecs_singleton_get(world, EgWindowsGlBackend),
		 .shown = {.frame = -2}, .building = {.frame = -2}});
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
	capture(&state->building, ecs_get_world_info(world)->frame_count_total - 1, identity);
	state->building.batches[0].render = renderer;
	SDL_WarpMouseInWindow(window, 32, 32);
	for (int i = 0; i < 50 && SDL_GetMouseFocus() != window; ++i) {
		SDL_PumpEvents();
		SDL_Delay(10);
	}
	test_assert(SDL_GetMouseFocus() == window);
	EgWindowsSdlUpdateState(world);
	state = ecs_get_mut(world, e_window, AppDrawPickState);
	ecs_entity_t result = 0;
	float x = 0, y = 0;
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	test_assert(state->cached);

	EgWindowsOpenGLContext *gl = ecs_get_mut(world, e_window, EgWindowsOpenGLContext);
	gl->gl_context = NULL;
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == 0 && !state->cached);
	gl->gl_context = context;
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity && state->cached);

	// A sentinel cached result proves that an unchanged call does not render.
	state->cached_id = 0;
	result = entity;
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == 0);
	SDL_WarpMouseInWindow(window, 33, 32);
	SDL_PumpEvents();
	EgWindowsSdlUpdateState(world);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	state->cached_id = 0;
	ecs_singleton_set(world, EgButtonsState, {.mouse = {[1] = EG_BUTTONS_STATE_HELD}});
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	state->cached_id = 0;
	ecs_singleton_set(world, EgButtonsState, {{0}});
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	ecs_remove_id(world, entity, EgUiScriptEventsTarget);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == 0);
	ecs_add_id(world, entity, EgUiScriptEventsTarget);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == entity);
	for (int i = 0; i < 3; ++i) v[i].position[0] += 4;
	state->shown.frame--;
	capture(&state->building, ecs_get_world_info(world)->frame_count_total - 1, identity);
	state->building.batches[0].render = renderer;
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
	state->building.valid = true;
	AppDrawPickFrame_complete(world, e_window, false);
	test_assert(!state->cached && !state->building.valid);
	AppDrawPickFrame_complete(world, e_window, true);
	test_assert(state->building.valid);
	test_int(state->building.count, 0);
	test_int(state->building.width, 96);

	const EgWindowsGlBackend *backend = ecs_singleton_get(world, EgWindowsGlBackend);
	EgWindowsGlBinding current = backend->current();
	ecs_remove(world, e_window, AppDrawPickState);
	EgWindowsGlBinding restored = backend->current();
	test_assert(restored.window == current.window && restored.context == current.context);
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
	const EgWindowsGlBackend *backend = ecs_singleton_get(world, EgWindowsGlBackend);
	test_assert(backend && backend->current && backend->make_current);

	SDL_Window *context_window = SDL_CreateWindow("pick target context removal", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(context_window != NULL);
	SDL_GLContext context = SDL_GL_CreateContext(context_window);
	test_assert(context != NULL);
	ecs_entity_t e_context = ecs_new(world);
	ecs_set(world, e_context, EgWindowsWindow, {.object = context_window});
	ecs_set(world, e_context, EgWindowsOpenGLContext, {.gl_context = context});
	ecs_set(world, e_context, AppDrawPickState,
		{.gl = *backend, .shown = {.frame = -1}, .building = {.frame = -1}});
	AppDrawPickState *state = ecs_get_mut(world, e_context, AppDrawPickState);
	state->target = egg_pick_target_create();
	test_assert(state->target != NULL);
	test_assert(backend->make_current((EgWindowsGlBinding){0}));
	ecs_remove(world, e_context, EgWindowsOpenGLContext);
	state = ecs_get_mut(world, e_context, AppDrawPickState);
	test_assert(state && state->target == NULL);
	EgWindowsGlBinding current = backend->current();
	test_assert(!current.window && !current.context);
	ecs_remove(world, e_context, AppDrawPickState);
	test_assert(SDL_GL_MakeCurrent(context_window, context));
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(context_window);

	SDL_Window *state_window = SDL_CreateWindow("pick target state removal", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(state_window != NULL);
	context = SDL_GL_CreateContext(state_window);
	test_assert(context != NULL);
	ecs_entity_t e_state = ecs_new(world);
	ecs_set(world, e_state, EgWindowsWindow, {.object = state_window});
	ecs_set(world, e_state, EgWindowsOpenGLContext, {.gl_context = context});
	ecs_set(world, e_state, AppDrawPickState,
		{.gl = *backend, .shown = {.frame = -1}, .building = {.frame = -1}});
	state = ecs_get_mut(world, e_state, AppDrawPickState);
	state->target = egg_pick_target_create();
	test_assert(state->target != NULL);
	test_assert(backend->make_current((EgWindowsGlBinding){0}));
	ecs_remove(world, e_state, AppDrawPickState);
	current = backend->current();
	test_assert(!current.window && !current.context);
	test_assert(SDL_GL_MakeCurrent(state_window, context));
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(state_window);

	SDL_Window *window = SDL_CreateWindow("pick target window removal", 64, 64, SDL_WINDOW_OPENGL);
	test_assert(window != NULL);
	context = SDL_GL_CreateContext(window);
	test_assert(context != NULL);
	ecs_entity_t e_window = ecs_new(world);
	ecs_set(world, e_window, EgWindowsWindow, {.object = window});
	ecs_set(world, e_window, EgWindowsOpenGLContext, {.gl_context = context});
	ecs_set(world, e_window, AppDrawPickState,
		{.gl = *backend, .shown = {.frame = -1}, .building = {.frame = -1}});
	state = ecs_get_mut(world, e_window, AppDrawPickState);
	state->target = egg_pick_target_create();
	test_assert(state->target != NULL);
	test_assert(backend->make_current((EgWindowsGlBinding){0}));
	ecs_remove(world, e_window, EgWindowsWindow);
	state = ecs_get_mut(world, e_window, AppDrawPickState);
	test_assert(state && state->target == NULL);
	current = backend->current();
	test_assert(!current.window && !current.context);
	ecs_remove(world, e_window, EgWindowsOpenGLContext);
	ecs_remove(world, e_window, AppDrawPickState);
	test_assert(SDL_GL_MakeCurrent(window, context));
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(window);
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
		ecs_set(world, window_entities[i], EgWindowsOpenGLContext, {.gl_context = contexts[i]});
		ecs_add_id(world, window_entities[i], AppDrawGpuPicking);
		ecs_set(world, window_entities[i], AppDrawPickState,
			{.gl = *ecs_singleton_get(world, EgWindowsGlBackend),
			 .shown = {.frame = -2}, .building = {.frame = -2}});
		for (int j = 0; j < 3; ++j) v[j].pick_id = (uint32_t)targets[i];
		AppDrawPickState *state = ecs_get_mut(world, window_entities[i], AppDrawPickState);
		capture(&state->building, ecs_get_world_info(world)->frame_count_total - 1, identity);
		state->building.batches[0].render = renderers[i];
	}
	test_assert(SDL_GL_MakeCurrent(windows[1], contexts[1]));
	ecs_entity_t result = 0;
	float x = 0, y = 0;
	focus_window(windows[0]);
	test_assert(AppDraw_PickMouse(world, &result, &x, &y, NULL));
	test_assert(result == targets[0]);
	test_assert(SDL_GL_GetCurrentContext() == contexts[1]);
	test_assert(AppDraw_CaptureBegin(world, NULL));
	focus_window(windows[1]);
	test_assert(!AppDraw_CaptureValid(world, NULL));
	AppDraw_CaptureEnd(world, NULL);
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
	const EgWindowsGlBackend *gl = ecs_singleton_get(world, EgWindowsGlBackend);
	test_assert(gl && gl->current && gl->make_current);
	EgWindowsGlBinding binding = gl->current();
	test_assert(binding.window == window && binding.context == context);
	test_assert(gl->make_current((EgWindowsGlBinding){0}));
	test_assert(gl->current().context == NULL);
	test_assert(gl->make_current(binding));
	ecs_entity_t e_window = ecs_new(world);
	ecs_query_t *systems = ecs_query(world, {
		.terms = {{.id = EcsSystem}},
		.group_by = EcsChildOf,
	});
	test_assert(systems != NULL);
	ecs_set(world, e_window, EgWindowsWindow, {.object = window});
	ecs_set(world, e_window, EgWindowsOpenGLContext, {.gl_context = context, .inner_systems = systems});
	ecs_set(world, e_window, EgShapesRectangle, {.w = 64, .h = 64});
	ecs_set(world, e_window, EgWindowsFrameCallback, {.complete = generic_frame_complete});
	ecs_set(world, e_window, EgWindowsSdlGlFrameCallback, {.complete = sdl_frame_complete});
	generic_frames = sdl_frames = 0;
	test_assert(ecs_progress(world, 0));
	test_int(generic_frames, 1);
	test_int(sdl_frames, 1);
	ecs_remove(world, e_window, EgWindowsOpenGLContext);
	ecs_query_fini(systems);
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
}
