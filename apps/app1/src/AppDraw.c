#include "AppDraw.h"
#include <EgWindows.h>
#include <EgCameras.h>
#include <EgSpatials.h>
#include <EgShapes.h>
#include <EgBase.h>
#include <EgDraws.h>
#include <EgUi.h>
#include <EgButtons.h>
#include <ecsx.h>
#include <egg.h>

ECS_COMPONENT_DECLARE(AppDrawContext);
ECS_COMPONENT_DECLARE(AppDrawContextCreate);
ECS_TAG_DECLARE(AppDrawGpuPicking);

typedef struct {
	egg_pick_async_t *async;
	ecs_entity_t picked;
	int64_t frame;
	int width, height;
	bool drawing;
	bool ready;
	bool cached;
	bool failed;
	bool unavailable;
} AppDrawPickState;

ECS_COMPONENT_DECLARE(AppDrawPickState);

typedef struct {
	ecs_entity_t capture_window;
	void *capture_object;
} AppDrawPickInput;

ECS_COMPONENT_DECLARE(AppDrawPickInput);

static void AppDrawRenderer_destroy(void *resource)
{
	AppDrawContext *draw = resource;
	egg_render_destroy(draw->render);
	draw->render = NULL;
}

static void AppDrawContext_destroy_renderer(AppDrawContext *draw, void *window,
	const EgWindowsOpenGLContext *context)
{
	if (!draw || !draw->render) return;
	if (!window || !context || !context->gl_context || !context->cleanup) {
		ecs_err("AppDraw: cannot destroy renderer without window and OpenGL context");
		return;
	}
	if (!context->cleanup(window, context->gl_context, AppDrawRenderer_destroy, draw)) {
		ecs_err("AppDraw: failed to clean up renderer");
	}
}

static void AppDrawContext_remove(ecs_iter_t *it)
{
	AppDrawContext *draw = ecs_field(it, AppDrawContext, 0);
	for (int32_t i = 0; i < it->count; ++i) {
		ecs_entity_t window = ecs_get_parent(it->world, it->entities[i]);
		while (window && !ecs_has(it->world, window, EgWindowsOpenGLContext)) {
			window = ecs_get_parent(it->world, window);
		}
		const EgWindowsWindow *native = window ? ecs_get(it->world, window, EgWindowsWindow) : NULL;
		const EgWindowsOpenGLContext *context = window ? ecs_get(it->world, window, EgWindowsOpenGLContext) : NULL;
		AppDrawContext_destroy_renderer(&draw[i], native ? native->object : NULL, context);
		EgDraws_List_Destroy(draw[i].list);
		draw[i].list = NULL;
	}
}

static void AppDrawWindow_destroy_renderers(ecs_world_t *world, ecs_entity_t window,
	void *native, const EgWindowsOpenGLContext *context)
{
	ecs_iter_t children = ecs_children(world, window);
	while (ecs_children_next(&children)) {
		for (int32_t i = 0; i < children.count; ++i) {
			ecs_entity_t child = children.entities[i];
			if (ecs_has(world, child, EgWindowsOpenGLContext)) continue;
			AppDrawContext *draw = ecs_get_mut(world, child, AppDrawContext);
			AppDrawContext_destroy_renderer(draw, native, context);
			AppDrawWindow_destroy_renderers(world, child, native, context);
		}
	}
}

static void AppDrawPickTarget_destroy(void *resource)
{
	AppDrawPickState *state = resource;
	egg_pick_async_destroy(state->async);
	state->async = NULL;
}

static void AppDrawPickState_destroy_target(AppDrawPickState *state, void *window,
	const EgWindowsOpenGLContext *context)
{
	if (!state || !state->async) return;
	if (!window || !context || !context->gl_context || !context->cleanup) {
		ecs_err("AppDraw: cannot destroy picking target without window and OpenGL context");
		return;
	}
	if (!context->cleanup(window, context->gl_context, AppDrawPickTarget_destroy, state)) {
		ecs_err("AppDraw: failed to clean up picking target");
	}
}

static void AppDrawPickState_remove(ecs_iter_t *it)
{
	for (int32_t i = 0; i < it->count; ++i) {
		ecs_entity_t entity = it->entities[i];
		AppDrawPickState *state = ecs_field(it, AppDrawPickState, 0) + i;
		const EgWindowsWindow *window = ecs_get(it->world, entity, EgWindowsWindow);
		const EgWindowsOpenGLContext *context = ecs_get(it->world, entity, EgWindowsOpenGLContext);
		AppDrawPickState_destroy_target(state, window ? window->object : NULL, context);
	}
}

static void AppDrawGpuPicking_remove(ecs_iter_t *it)
{
	for (int32_t i = 0; i < it->count; ++i) {
		AppDrawPickState *state = ecs_get_mut(it->world, it->entities[i], AppDrawPickState);
		if (!state) continue;
		state->cached = false;
		state->picked = 0;
		egg_pick_async_invalidate(state->async);
	}
}

static void AppDrawPickWindow_remove(ecs_iter_t *it)
{
	EgWindowsWindow *windows = ecs_field(it, EgWindowsWindow, 0);
	for (int32_t i = 0; i < it->count; ++i) {
		ecs_entity_t entity = it->entities[i];
		AppDrawPickState *state = ecs_get_mut(it->world, entity, AppDrawPickState);
		const EgWindowsOpenGLContext *context = ecs_get(it->world, entity, EgWindowsOpenGLContext);
		AppDrawPickState_destroy_target(state, windows[i].object, context);
		AppDrawWindow_destroy_renderers(it->world, entity, windows[i].object, context);
	}
}

static void AppDrawPickContext_remove(ecs_iter_t *it)
{
	EgWindowsOpenGLContext *contexts = ecs_field(it, EgWindowsOpenGLContext, 0);
	for (int32_t i = 0; i < it->count; ++i) {
		ecs_entity_t entity = it->entities[i];
		AppDrawPickState *state = ecs_get_mut(it->world, entity, AppDrawPickState);
		const EgWindowsWindow *window = ecs_get(it->world, entity, EgWindowsWindow);
		AppDrawPickState_destroy_target(state, window ? window->object : NULL, &contexts[i]);
		AppDrawWindow_destroy_renderers(it->world, entity, window ? window->object : NULL, &contexts[i]);
	}
}

static void AppDrawPickState_move(void *dst_ptr, void *src_ptr, int32_t count, const ecs_type_info_t *ti)
{
	(void)ti;
	AppDrawPickState *dst = dst_ptr;
	AppDrawPickState *src = src_ptr;
	for (int32_t i = 0; i < count; ++i) {
		dst[i] = src[i];
		src[i] = (AppDrawPickState){0};
	}
}

static bool AppDraw_IsPickable(uint32_t id, void *ctx)
{
	ecs_world_t *world = ctx;
	ecs_entity_t e = ecs_get_alive(world, id);
	if (!e || !ecs_has_id(world, e, EgUiScriptEventsTarget)) return false;
	for (ecs_entity_t p = e; p; p = ecs_get_parent(world, p)) {
		if (ecs_has_id(world, p, EcsDisabled)) return false;
	}
	return true;
}

static uint64_t AppDraw_ResolvePickable(uint32_t id, void *ctx)
{
	ecs_world_t *world = ctx;
	return AppDraw_IsPickable(id, world) ? ecs_get_alive(world, id) : 0;
}

static void AppDrawPickFrame_begin(ecs_world_t *world, ecs_entity_t window)
{
	AppDrawPickState *state = ecs_get_mut(world, window, AppDrawPickState);
	if (!state) return;
	state->drawing = false;
	state->ready = false;
	state->failed = false;
	const EgWindowsInputState *focus = ecs_singleton_get(world, EgWindowsInputState);
	const EgWindowsWindowState *window_state = ecs_get(world, window, EgWindowsWindowState);
	const EgWindowsWindow *window_object = ecs_get(world, window, EgWindowsWindow);
	if (!ecs_has_id(world, window, AppDrawGpuPicking) ||
		!focus || focus->mouse_window != window || !window_state || !window_state->valid ||
		!window_object || window_state->object != window_object->object ||
		!window_state->mouse_focus || window_state->hidden || window_state->minimized ||
		window_state->width <= 0 || window_state->height <= 0 ||
		window_state->pixel_width <= 0 || window_state->pixel_height <= 0 ||
		window_state->mouse_x < 0 || window_state->mouse_y < 0 ||
		window_state->mouse_x >= window_state->width || window_state->mouse_y >= window_state->height) {
		state->cached = false;
		state->picked = 0;
		egg_pick_async_invalidate(state->async);
		if (state->async) {
			uint64_t ignored_id, ignored_frame;
			if (egg_pick_async_poll(state->async, &ignored_id, &ignored_frame) < 0)
				ecs_err("AppDraw: failed to retire invalid picking results");
		}
		return;
	}
	if (state->width != window_state->pixel_width || state->height != window_state->pixel_height) {
		state->cached = false;
		state->picked = 0;
		egg_pick_async_invalidate(state->async);
	}
	state->width = window_state->pixel_width;
	state->height = window_state->pixel_height;
	if (state->unavailable) return;
	if (!state->async) {
		state->async = egg_pick_async_create();
		if (!state->async) {
			ecs_err("AppDraw: GPU picking is unavailable for this context");
			state->unavailable = true;
			return;
		}
	}
	uint64_t id = 0, completed_frame = 0;
	int poll = egg_pick_async_poll(state->async, &id, &completed_frame);
	if (poll < 0) {
		ecs_err("AppDraw: failed to poll GPU picking");
		state->cached = false;
		state->picked = 0;
		state->unavailable = true;
		egg_pick_async_invalidate(state->async);
		return;
	}
	if (poll > 0 && (!state->cached || completed_frame > (uint64_t)state->frame)) {
		state->picked = id;
		state->frame = (int64_t)completed_frame;
		state->cached = true;
	}
	int x = (int)(window_state->mouse_x * state->width / window_state->width);
	int y = state->height - 1 - (int)(window_state->mouse_y * state->height / window_state->height);
	int started = egg_pick_async_begin(state->async, x, y,
		(uint64_t)ecs_get_world_info(world)->frame_count_total);
	if (started < 0) {
		ecs_err("AppDraw: failed to begin GPU picking");
		state->cached = false;
		state->picked = 0;
		state->unavailable = true;
		egg_pick_async_invalidate(state->async);
	}
	state->drawing = started > 0;
}

static void AppDrawPickFrame_draw(ecs_world_t *world, AppDrawPickState *state,
	egg_render_t *render, const EgDraws_List *list, const float *projection)
{
	if (!state || !state->drawing || state->failed) {
		egg_draw_layers(render, &list->layers, projection);
		return;
	}
	if (!egg_pick_async_draw(state->async, render, &list->layers, projection, AppDraw_ResolvePickable, world)) {
		state->failed = true;
		state->unavailable = true;
		state->cached = false;
		state->picked = 0;
		egg_pick_async_invalidate(state->async);
		ecs_err("AppDraw: failed to render asynchronous picking");
	}
}

static void AppDrawPickFrame_end(ecs_world_t *world, ecs_entity_t window)
{
	AppDrawPickState *state = ecs_get_mut(world, window, AppDrawPickState);
	if (!state || !state->drawing) return;
	state->drawing = false;
	state->ready = egg_pick_async_end(state->async);
	if (!state->ready) {
		ecs_err("AppDraw: failed to finish GPU picking");
		state->cached = false;
		state->picked = 0;
		state->unavailable = true;
		egg_pick_async_invalidate(state->async);
	}
}

static void AppDrawPickFrame_complete(ecs_world_t *world, ecs_entity_t window, bool displayed)
{
	AppDrawPickState *state = ecs_get_mut(world, window, AppDrawPickState);
	if (!state) return;
	state->drawing = false;
	egg_pick_async_complete(state->async, displayed && state->ready && !state->failed);
	state->ready = false;
}

static void AppDraw_ToLocal(ecs_world_t *world, ecs_entity_t entity,
	float screen_x, float screen_y, float *local_x, float *local_y, void *ctx)
{
	(void)screen_x;
	(void)screen_y;
	(void)ctx;
	EgUiLocalMousePosition const *local = ecs_get(world, entity, EgUiLocalMousePosition);
	if (local) {
		*local_x = local->x;
		*local_y = local->y;
	}
}

static bool AppDraw_CaptureBegin(ecs_world_t *world, void *ctx)
{
	(void)ctx;
	const EgWindowsInputState *focus = ecs_singleton_get(world, EgWindowsInputState);
	AppDrawPickInput *input = ecs_singleton_ensure(world, AppDrawPickInput);
	input->capture_window = focus ? focus->mouse_window : 0;
	const EgWindowsWindow *window = input->capture_window ? ecs_get(world, input->capture_window, EgWindowsWindow) : NULL;
	input->capture_object = window ? window->object : NULL;
	if (!input->capture_object) {
		ecs_err("AppDraw: cannot begin drag without a focused window");
		return false;
	}
	return true;
}

static bool AppDraw_InsideWindow(float x, float y, int width, int height)
{
	return x >= 0 && y >= 0 && x < width && y < height;
}

static bool AppDraw_CaptureValid(ecs_world_t *world, void *ctx)
{
	(void)ctx;
	const AppDrawPickInput *input = ecs_singleton_get(world, AppDrawPickInput);
	if (!input || !input->capture_window || !ecs_is_alive(world, input->capture_window)) return false;
	const EgWindowsInputState *focus = ecs_singleton_get(world, EgWindowsInputState);
	if (!focus || focus->mouse_window != input->capture_window) return false;
	const EgWindowsWindow *window = ecs_get(world, input->capture_window, EgWindowsWindow);
	const EgWindowsWindowState *state = ecs_get(world, input->capture_window, EgWindowsWindowState);
	if (!window || window->object != input->capture_object || !state ||
		state->object != window->object || !state->valid) return false;
	if (!ecs_has_id(world, input->capture_window, AppDrawGpuPicking) ||
		ecs_has_id(world, input->capture_window, EcsDisabled) ||
		ecs_has_id(world, input->capture_window, EgWindowsCloseRequest) ||
		!state->mouse_focus || !state->keyboard_focus || state->hidden || state->minimized) return false;
	return AppDraw_InsideWindow(state->mouse_x, state->mouse_y, state->width, state->height);
}

static void AppDraw_CaptureEnd(ecs_world_t *world, void *ctx)
{
	(void)ctx;
	AppDrawPickInput *input = ecs_singleton_get_mut(world, AppDrawPickInput);
	if (!input) return;
	input->capture_window = 0;
	input->capture_object = NULL;
}

static bool AppDraw_PickMouse(ecs_world_t *world, ecs_entity_t *target,
	float *screen_x, float *screen_y, void *ctx)
{
	(void)ctx;
	const EgWindowsInputState *focus = ecs_singleton_get(world, EgWindowsInputState);
	ecs_entity_t window = focus ? focus->mouse_window : 0;
	if (window && !ecs_is_alive(world, window)) window = 0;
	const EgWindowsWindowState *window_state = window ? ecs_get(world, window, EgWindowsWindowState) : NULL;
	const EgWindowsWindow *window_object = window ? ecs_get(world, window, EgWindowsWindow) : NULL;
	if (!window_object || !window_state || window_state->object != window_object->object ||
		!window_state->mouse_focus || window_state->hidden || window_state->minimized) {
		window_state = NULL;
	}
	AppDrawPickInput *input = ecs_singleton_ensure(world, AppDrawPickInput);
	if (input->capture_window && window == input->capture_window && window_state && window_state->valid &&
		window_state->object == input->capture_object) {
		*screen_x = window_state->mouse_x;
		*screen_y = window_state->mouse_y;
		*target = 0;
		return true;
	}
	if (!window) {
		ecs_iter_t states = ecs_each(world, AppDrawPickState);
		ecs_iter_t *iter = &states;
		while (ecs_each_next(iter)) {
			AppDrawPickState *values = ecs_field(iter, AppDrawPickState, 0);
			for (int32_t i = 0; i < states.count; ++i) {
				values[i].cached = false;
				values[i].picked = 0;
				egg_pick_async_invalidate(values[i].async);
			}
		}
		if (!focus || focus->mouse_focus) return false;
		ecs_iter_t it = ecs_each_id(world, AppDrawGpuPicking);
		while (ecs_each_next(&it)) {
			for (int32_t i = 0; i < it.count; ++i) {
				if (ecs_has(world, it.entities[i], EgWindowsWindow)) {
					ecs_iter_fini(&it);
					return true;
				}
			}
		}
		return false;
	}
	if (!ecs_has_id(world, window, AppDrawGpuPicking)) return false;

	*target = 0;
	if (!window_state || !window_state->valid) {
		AppDrawPickState *state = ecs_get_mut(world, window, AppDrawPickState);
		if (state) {
			state->cached = false;
			egg_pick_async_invalidate(state->async);
		}
		return true;
	}
	float x = window_state->mouse_x, y = window_state->mouse_y;
	*screen_x = x;
	*screen_y = y;
	AppDrawPickState *state = ecs_get_mut(world, window, AppDrawPickState);
	if (!state) return true;
	const EgWindowsOpenGLContext *gl_context = ecs_get(world, window, EgWindowsOpenGLContext);
	if (!window_object || !window_object->object || window_state->object != window_object->object ||
		!gl_context || !gl_context->gl_context) {
		state->cached = false;
		egg_pick_async_invalidate(state->async);
		return true;
	}
	if (!state->cached) return true;
	int logical_w = window_state->width, logical_h = window_state->height;
	int pixel_w = window_state->pixel_width, pixel_h = window_state->pixel_height;
	if (logical_w <= 0 || logical_h <= 0 || pixel_w != state->width || pixel_h != state->height ||
		!AppDraw_InsideWindow(x, y, logical_w, logical_h)) {
		state->cached = false;
		egg_pick_async_invalidate(state->async);
		return true;
	}

	if (state->picked && ecs_is_alive(world, state->picked) &&
		AppDraw_IsPickable((uint32_t)state->picked, world)) *target = state->picked;
	return true;
}

static void Test_Render(ecs_iter_t *it)
{
	AppDrawContext *d  = ecs_field_self(it, AppDrawContext, 1);
	EgCamerasState *c0 = ecs_field_shared(it, EgCamerasState, 2);
	ecs_entity_t window = ecs_pair_second(it->world, ecs_field_id(it, 0));
	AppDrawPickState *pick = ecs_has_id(it->world, window, AppDrawGpuPicking) ? ecs_get_mut(it->world, window, AppDrawPickState) : NULL;

	for (int i = 0; i < it->count; ++i, ++d) {
		ecs_assert(d->list != NULL, ECS_INTERNAL_ERROR, NULL);
		if (!d->render) {
			d->render = egg_render_init();
			if (!d->render) {
				ecs_err("AppDraw: failed to initialize renderer for entity %s",
					ecs_get_name(it->world, it->entities[i]));
				if (pick) pick->failed = true;
				EgDraws_List_Clear(d->list);
				ecs_enable(it->world, it->entities[i], false);
				continue;
			}
		}
		d->pixelScale = c0->pixel_scale * 1.0f; // Keep the egg-scale near the camera-derived size.
		EgDraws_List_SetPixelScale(d->list, d->pixelScale);
		AppDrawPickFrame_draw(it->world, pick, d->render, d->list, (float *)&c0->vp);
		EgDraws_List_Clear(d->list);
	}
}

static void AppDrawContext_Create(ecs_iter_t *it)
{
	AppDrawContextCreate *def      = ecs_field_self(it, AppDrawContextCreate, 0);
	ecs_entity_t          e_window = ecs_field_src(it, 1);
	EgWindowsWindow const *window = ecs_get(it->world, e_window, EgWindowsWindow);
	const EgWindowsOpenGLContext *context = ecs_get(it->world, e_window, EgWindowsOpenGLContext);
	if (!window || !window->object || !context || !context->gl_context || !context->cleanup) {
		ecs_err("AppDraw: window and OpenGL context with cleanup are required");
		for (int i = 0; i < it->count; ++i) ecs_enable(it->world, it->entities[i], false);
		return;
	}
	if (ecs_has_id(it->world, e_window, AppDrawGpuPicking)) {
		if (!ecs_has(it->world, e_window, AppDrawPickState)) {
			ecs_set(it->world, e_window, AppDrawPickState, {.frame = -1});
			ecs_set(it->world, e_window, EgWindowsFrameCallback,
				{.begin = AppDrawPickFrame_begin, .end = AppDrawPickFrame_end,
				 .complete = AppDrawPickFrame_complete});
			ecs_script_set_event_hooks(it->world, &(ecs_script_event_hooks_t){.to_local = AppDraw_ToLocal});
		}
	}

	ecs_log_set_level(0);
	ecs_log(-1, "AppDrawContext_Create: '%s'", ecs_get_name(it->world, e_window));

	for (int i = 0; i < it->count; ++i, ++def) {
		ecs_entity_t  e      = it->entities[i];
		EgDraws_List *list   = EgDraws_List_Create();
		if (list == NULL) {
			ecs_err("Failed to create draw context for entity %s", ecs_get_name(it->world, e));
			ecs_enable(it->world, e, false);
			continue;
		}

		ecs_set(it->stage, e, AppDrawContext, {NULL, list, 1.0f});

		// The window system will call this render system using `ecs_run()` every frame
		// by putting it as a child of the window entity.
		char name[256];
		snprintf(name, sizeof(name), "System_%s", ecs_get_name(it->world, e));
		ecs_entity_t sys = ecs_entity(it->world, {.name = name, .parent = e_window});
		ecs_log(-1, "create system: '%s'", ecs_get_name(it->world, sys));
		ecs_system_init(it->world,
		&(ecs_system_desc_t){
		.entity      = sys,
		.callback    = Test_Render,
		.query.terms = {
		{.id = ecs_childof(e_window)},
		{.id = ecs_id(AppDrawContext), .src.id = EcsSelf, .inout = EcsIn},
		{.id = ecs_id(EgCamerasState), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
		}});
	}
	ecs_log_set_level(-1);
}

static void AppDrawContext_Collect(ecs_iter_t *it)
{
	AppDrawContext *d0 = ecs_field_shared(it, AppDrawContext, 0);
	EgDraws_List   *l  = ecs_field_self(it, EgDraws_List, 1);
	for (int i = 0; i < it->count; ++i, ++l) {
		EgDraws_List_Append(d0->list, l);
		// Per-entity lists tessellate pixel-sized thickness during collection, so they need the camera scale (applied next frame).
		EgDraws_List_SetPixelScale(l, d0->pixelScale);
	}
}

void AppDrawImport(ecs_world_t *world)
{
	ECS_MODULE(world, AppDraw);
	ecs_set_name_prefix(world, "AppDraw");
	ECS_IMPORT(world, EgWindows);
	ECS_IMPORT(world, EgSpatials);
	ECS_IMPORT(world, EgBase);
	ECS_IMPORT(world, EgUi);
	ecs_import(world, EgDraws_Import, "EgDraws");

	ECS_COMPONENT_DEFINE(world, AppDrawContext);
	ECS_COMPONENT_DEFINE(world, AppDrawContextCreate);
	ECS_TAG_DEFINE(world, AppDrawGpuPicking);
	ECS_COMPONENT_DEFINE(world, AppDrawPickState);
	ECS_COMPONENT_DEFINE(world, AppDrawPickInput);
	ecs_set_hooks(world, AppDrawPickState, {.move = AppDrawPickState_move});
	ecs_observer(world,
	{.query = {.terms = {{.id = ecs_id(AppDrawContext)}}},
	.events = {EcsOnRemove},
	.callback = AppDrawContext_remove});
	ecs_observer(world,
	{.query = {.terms = {{.id = ecs_id(AppDrawPickState)}}},
	.events = {EcsOnRemove},
	.callback = AppDrawPickState_remove});
	ecs_observer(world,
	{.query = {.terms = {{.id = AppDrawGpuPicking}}},
	.events = {EcsOnRemove},
	.callback = AppDrawGpuPicking_remove});
	ecs_observer(world,
	{.query = {.terms = {{.id = ecs_id(EgWindowsWindow)}}},
	.events = {EcsOnRemove},
	.callback = AppDrawPickWindow_remove});
	ecs_observer(world,
	{.query = {.terms = {{.id = ecs_id(EgWindowsOpenGLContext)}}},
	.events = {EcsOnRemove},
	.callback = AppDrawPickContext_remove});
	ecs_singleton_set(world, EgUiScriptEventsPicker, {
		.pick = AppDraw_PickMouse,
		.capture_begin = AppDraw_CaptureBegin,
		.capture_valid = AppDraw_CaptureValid,
		.capture_end = AppDraw_CaptureEnd
	});

	ecs_struct(world,
	{.entity = ecs_id(AppDrawContextCreate),
	.members = {
	{.name = "dummy", .type = ecs_id(ecs_i32_t)},
	}});

	ecs_struct(world,
	{.entity = ecs_id(AppDrawContext),
	.members = {
	{.name = "render", .type = ecs_id(ecs_uptr_t)},
	{.name = "list", .type = ecs_id(ecs_uptr_t)},
	{.name = "pixelScale", .type = ecs_id(ecs_f32_t)},
	}});

	ecs_system(world,
	{.entity     = ecs_entity(world, {.name = "AppDrawContext_Create"}),
	.phase       = EcsOnUpdate,
	.callback    = AppDrawContext_Create,
	.immediate   = true,
	.query.terms = {
	{.id = ecs_id(AppDrawContextCreate), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgWindowsOpenGLContext), .trav = EcsChildOf, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(AppDrawContext), .oper = EcsNot}, // Adds this
	}});

	ecs_system(world,
	{.entity     = ecs_entity(world, {.name = "AppDrawContext_Collect"}),
	.phase       = EcsPostUpdate,
	.callback    = AppDrawContext_Collect,
	.query.terms = {
	{.id = ecs_id(AppDrawContext), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(EgDraws_List), .src.id = EcsSelf, .inout = EcsIn},
	}});
}
