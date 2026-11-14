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
#include <stdlib.h>
#include <string.h>

ECS_COMPONENT_DECLARE(AppDrawContext);
ECS_COMPONENT_DECLARE(AppDrawContextCreate);
ECS_TAG_DECLARE(AppDrawGpuPicking);

typedef struct {
	egg_render_t *render;
	EgDraws_List *list;
	float projection[16];
	uint32_t *ids;
	size_t id_count;
	size_t id_capacity;
} AppDrawPickBatch;

typedef struct {
	AppDrawPickBatch *batches;
	size_t count;
	size_t capacity;
	int64_t frame;
	int width, height;
	bool valid;
} AppDrawPickScene;

typedef struct {
	egg_pick_target_t *target;
	EgWindowsGlBackend gl;
	AppDrawPickScene shown;
	AppDrawPickScene building;
	float mouse_x, mouse_y;
	uint32_t cached_id;
	bool cached;
} AppDrawPickState;

ECS_COMPONENT_DECLARE(AppDrawPickState);

typedef struct {
	ecs_entity_t focus;
	uint8_t buttons[EG_BUTTONS_MOUSE_MAX];
	ecs_entity_t capture_window;
	void *capture_object;
} AppDrawPickInput;

ECS_COMPONENT_DECLARE(AppDrawPickInput);

static void AppDrawPickScene_free(AppDrawPickScene *scene)
{
	for (size_t i = 0; i < scene->capacity; ++i) {
		EgDraws_List_Destroy(scene->batches[i].list);
		free(scene->batches[i].ids);
	}
	free(scene->batches);
	*scene = (AppDrawPickScene){0};
}

static void AppDrawPickState_dtor(void *ptr, int32_t count, const ecs_type_info_t *ti)
{
	(void)ti;
	AppDrawPickState *states = ptr;
	for (int32_t i = 0; i < count; ++i) {
		AppDrawPickScene_free(&states[i].shown);
		AppDrawPickScene_free(&states[i].building);
	}
}

static void AppDrawPickState_destroy_target(AppDrawPickState *state, void *window, void *context)
{
	if (!state || !state->target) return;
	if (!window || !context) {
		ecs_err("AppDraw: cannot destroy picking target without window and OpenGL context");
		return;
	}
	EgWindowsGlBinding binding = {window, context};
	EgWindowsGlBinding previous = state->gl.current();
	if (!state->gl.make_current(binding)) {
		ecs_err("AppDraw: cannot make picking context current to destroy target");
		return;
	}
	egg_pick_target_destroy(state->target);
	state->target = NULL;
	if (!state->gl.make_current(previous)) {
		ecs_err("AppDraw: cannot restore OpenGL context after destroying picking target");
	}
}

static void AppDrawPickState_remove(ecs_iter_t *it)
{
	for (int32_t i = 0; i < it->count; ++i) {
		ecs_entity_t entity = it->entities[i];
		AppDrawPickState *state = ecs_field(it, AppDrawPickState, 0) + i;
		const EgWindowsWindow *window = ecs_get(it->world, entity, EgWindowsWindow);
		const EgWindowsOpenGLContext *context = ecs_get(it->world, entity, EgWindowsOpenGLContext);
		AppDrawPickState_destroy_target(state, window ? window->object : NULL,
			context ? context->gl_context : NULL);
	}
}

static void AppDrawPickWindow_remove(ecs_iter_t *it)
{
	EgWindowsWindow *windows = ecs_field(it, EgWindowsWindow, 0);
	for (int32_t i = 0; i < it->count; ++i) {
		ecs_entity_t entity = it->entities[i];
		AppDrawPickState *state = ecs_get_mut(it->world, entity, AppDrawPickState);
		const EgWindowsOpenGLContext *context = ecs_get(it->world, entity, EgWindowsOpenGLContext);
		AppDrawPickState_destroy_target(state, windows[i].object,
			context ? context->gl_context : NULL);
	}
}

static void AppDrawPickContext_remove(ecs_iter_t *it)
{
	EgWindowsOpenGLContext *contexts = ecs_field(it, EgWindowsOpenGLContext, 0);
	for (int32_t i = 0; i < it->count; ++i) {
		ecs_entity_t entity = it->entities[i];
		AppDrawPickState *state = ecs_get_mut(it->world, entity, AppDrawPickState);
		const EgWindowsWindow *window = ecs_get(it->world, entity, EgWindowsWindow);
		AppDrawPickState_destroy_target(state, window ? window->object : NULL,
			contexts[i].gl_context);
	}
}

static void AppDrawPickState_move(void *dst_ptr, void *src_ptr, int32_t count, const ecs_type_info_t *ti)
{
	AppDrawPickState_dtor(dst_ptr, count, ti);
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

static bool AppDrawPickScene_add(AppDrawPickScene *scene, egg_render_t *render,
	const EgDraws_List *list, const float *projection, ecs_world_t *world)
{
	if (scene->count == scene->capacity) {
		size_t capacity = scene->capacity ? scene->capacity * 2 : 4;
		AppDrawPickBatch *batches = realloc(scene->batches, capacity * sizeof(*batches));
		if (!batches) {
			ecs_err("AppDraw: cannot allocate picking batches");
			return false;
		}
		memset(batches + scene->capacity, 0, (capacity - scene->capacity) * sizeof(*batches));
		scene->batches = batches;
		scene->capacity = capacity;
	}
	AppDrawPickBatch *batch = &scene->batches[scene->count++];
	if (!batch->list) batch->list = EgDraws_List_Create();
	if (!batch->list) {
		ecs_err("AppDraw: cannot allocate picking list");
		return false;
	}
	batch->render = render;
	memcpy(batch->projection, projection, sizeof(batch->projection));
	EgDraws_List_Clear(batch->list);
	const ecs_vec_t *layers = ecs_vec_first_t(&list->layers, ecs_vec_t);
	size_t total = 0;
	for (int32_t i = 0; i < ecs_vec_count(&list->layers); ++i) total += (size_t)ecs_vec_count(&layers[i]);
	if (total > batch->id_capacity) {
		uint32_t *ids = realloc(batch->ids, total * sizeof(*ids));
		if (!ids) {
			ecs_err("AppDraw: cannot allocate picking IDs");
			return false;
		}
		batch->ids = ids;
		batch->id_capacity = total;
	}
	batch->id_count = total;
	size_t offset = 0;
	uint32_t last_id = 0, last_eligible = 0;
	for (int32_t i = 0; i < ecs_vec_count(&list->layers); ++i) {
		int32_t count = ecs_vec_count(&layers[i]);
		if (!count) continue;
		ecs_vec_t *dst = EgDraws_List_GetLayer(batch->list, i);
		if (!dst) {
			ecs_err("AppDraw: cannot allocate picking layer");
			return false;
		}
		EgDraws_Vertex *vertices = ecs_vec_grow_t(NULL, dst, EgDraws_Vertex, count);
		if (!vertices) {
			ecs_err("AppDraw: cannot allocate picking vertices");
			return false;
		}
		memcpy(vertices, ecs_vec_first(&layers[i]), (size_t)count * sizeof(*vertices));
		for (int32_t j = 0; j < count; ++j) {
			uint32_t id = vertices[j].pick_id;
			batch->ids[offset++] = id;
			if (id != last_id) {
				last_id = id;
				last_eligible = id && AppDraw_IsPickable(id, world) ? id : 0;
			}
			vertices[j].pick_id = last_eligible;
		}
	}
	return true;
}

static bool AppDrawPickScene_equal(const AppDrawPickScene *a, const AppDrawPickScene *b)
{
	if (!a->valid || !b->valid || a->width != b->width || a->height != b->height || a->count != b->count) return false;
	for (size_t i = 0; i < a->count; ++i) {
		const AppDrawPickBatch *x = &a->batches[i], *y = &b->batches[i];
		if (x->render != y->render || x->id_count != y->id_count ||
			memcmp(x->projection, y->projection, sizeof(x->projection)) ||
			(x->id_count && memcmp(x->ids, y->ids, x->id_count * sizeof(*x->ids)))) return false;
		const ecs_vec_t *xl = ecs_vec_first_t(&x->list->layers, ecs_vec_t);
		const ecs_vec_t *yl = ecs_vec_first_t(&y->list->layers, ecs_vec_t);
		int32_t xc = ecs_vec_count(&x->list->layers), yc = ecs_vec_count(&y->list->layers);
		int32_t layers = xc > yc ? xc : yc;
		for (int32_t j = 0; j < layers; ++j) {
			int32_t xn = j < xc ? ecs_vec_count(&xl[j]) : 0;
			int32_t yn = j < yc ? ecs_vec_count(&yl[j]) : 0;
			if (xn != yn || (xn && memcmp(ecs_vec_first(&xl[j]), ecs_vec_first(&yl[j]),
				(size_t)xn * sizeof(EgDraws_Vertex)))) return false;
		}
	}
	return true;
}

static bool AppDrawPickScene_update_eligibility(AppDrawPickScene *scene, ecs_world_t *world)
{
	bool changed = false;
	for (size_t i = 0; i < scene->count; ++i) {
		AppDrawPickBatch *batch = &scene->batches[i];
		ecs_vec_t *layers = ecs_vec_first_t(&batch->list->layers, ecs_vec_t);
		size_t offset = 0;
		uint32_t last_id = 0, last_eligible = 0;
		for (int32_t j = 0; j < ecs_vec_count(&batch->list->layers); ++j) {
			EgDraws_Vertex *vertices = ecs_vec_first_t(&layers[j], EgDraws_Vertex);
			for (int32_t k = 0; k < ecs_vec_count(&layers[j]); ++k) {
				uint32_t id = batch->ids[offset++];
				if (id != last_id) {
					last_id = id;
					last_eligible = id && AppDraw_IsPickable(id, world) ? id : 0;
				}
				if (vertices[k].pick_id != last_eligible) {
					vertices[k].pick_id = last_eligible;
					changed = true;
				}
			}
		}
	}
	return changed;
}

static bool AppDrawPickState_publish(AppDrawPickState *state, int64_t frame)
{
	if (state->shown.frame != frame && state->building.frame == frame && state->building.valid) {
		if (!AppDrawPickScene_equal(&state->shown, &state->building)) state->cached = false;
		AppDrawPickScene previous = state->shown;
		state->shown = state->building;
		state->building = previous;
	}
	if (!state->shown.valid || state->shown.frame != frame) {
		state->cached = false;
		return false;
	}
	return true;
}

static void AppDrawPickFrame_complete(ecs_world_t *world, ecs_entity_t window, bool displayed)
{
	AppDrawPickState *state = ecs_get_mut(world, window, AppDrawPickState);
	if (!state || !ecs_has_id(world, window, AppDrawGpuPicking)) return;
	AppDrawPickScene *scene = &state->building;
	int64_t frame = ecs_get_world_info(world)->frame_count_total;
	if (!displayed) {
		scene->valid = false;
		state->cached = false;
		return;
	}
	if (scene->frame != frame) {
		scene->frame = frame;
		scene->count = 0;
		const EgWindowsWindowState *window_state = ecs_get(world, window, EgWindowsWindowState);
		const EgWindowsWindow *window_object = ecs_get(world, window, EgWindowsWindow);
		scene->valid = window_state && window_state->valid && window_object &&
			window_state->object == window_object->object;
		if (scene->valid) {
			scene->width = window_state->pixel_width;
			scene->height = window_state->pixel_height;
		}
		if (scene->width <= 0 || scene->height <= 0) scene->valid = false;
	}
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
	if (!window_object || !window_state || window_state->object != window_object->object || !window_state->mouse_focus) {
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
	bool focus_changed = input->focus != window;
	input->focus = window;
	bool buttons_changed = false;
	const EgButtonsState *buttons = ecs_singleton_get(world, EgButtonsState);
	for (int i = 1; i < EG_BUTTONS_MOUSE_MAX; ++i) {
		uint8_t held = buttons ? buttons->mouse[i] & EG_BUTTONS_STATE_HELD : 0;
		if (input->buttons[i] != held) buttons_changed = true;
		input->buttons[i] = held;
	}
	if (!window) {
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
		if (state) state->cached = false;
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
		return true;
	}
	EgWindowsGlBinding binding = {window_object->object, gl_context->gl_context};
	int64_t expected_frame = ecs_get_world_info(world)->frame_count_total - 1;
	if (!AppDrawPickState_publish(state, expected_frame)) return true;
	int logical_w = window_state->width, logical_h = window_state->height;
	int pixel_w = window_state->pixel_width, pixel_h = window_state->pixel_height;
	if (logical_w <= 0 || logical_h <= 0 || pixel_w != state->shown.width || pixel_h != state->shown.height ||
		!AppDraw_InsideWindow(x, y, logical_w, logical_h)) {
		state->cached = false;
		return true;
	}

	if (AppDrawPickScene_update_eligibility(&state->shown, world)) state->cached = false;
	if (focus_changed || buttons_changed || x != state->mouse_x || y != state->mouse_y) state->cached = false;
	state->mouse_x = x;
	state->mouse_y = y;
	if (state->cached) {
		if (state->cached_id && AppDraw_IsPickable(state->cached_id, world)) {
			*target = ecs_get_alive(world, state->cached_id);
		}
		return true;
	}

	EgWindowsGlBinding previous = state->gl.current();
	if (!state->gl.make_current(binding)) {
		return true;
	}
	uint32_t id = 0;
	if (!state->target) state->target = egg_pick_target_create();
	bool read = state->target && egg_pick_target_begin_region(state->target, pixel_w, pixel_h,
		(int)(x * pixel_w / logical_w), (int)(y * pixel_h / logical_h));
	for (size_t i = 0; read && i < state->shown.count; ++i) {
		AppDrawPickBatch *batch = &state->shown.batches[i];
		read = egg_pick_draw_layers(state->target, batch->render, &batch->list->layers,
			batch->projection, NULL, NULL);
	}
	if (read) read = egg_pick_target_read_region(state->target, &id);
	if (!read) ecs_err("AppDraw: failed to render or read picking region");
	if (!state->gl.make_current(previous)) {
		return true;
	}
	if (read) {
		state->cached = true;
		state->cached_id = id;
		if (AppDraw_IsPickable(id, world)) *target = ecs_get_alive(world, id);
	}
	return true;
}

static void Test_Render(ecs_iter_t *it)
{
	AppDrawContext *d  = ecs_field_self(it, AppDrawContext, 1);
	EgCamerasState *c0 = ecs_field_shared(it, EgCamerasState, 2);
	ecs_entity_t window = ecs_pair_second(it->world, ecs_field_id(it, 0));
	AppDrawPickState *pick = ecs_has_id(it->world, window, AppDrawGpuPicking) ? ecs_get_mut(it->world, window, AppDrawPickState) : NULL;

	for (int i = 0; i < it->count; ++i, ++d) {
		ecs_assert(d->render != NULL, ECS_INTERNAL_ERROR, NULL);
		ecs_assert(d->list != NULL, ECS_INTERNAL_ERROR, NULL);
		d->pixelScale = c0->pixel_scale * 1.0f; // Keep the egg-scale near the camera-derived size.
		EgDraws_List_SetPixelScale(d->list, d->pixelScale);
		egg_draw_layers(d->render, &d->list->layers, (float *)&c0->vp);
		if (pick) {
			int64_t frame = ecs_get_world_info(it->world)->frame_count_total;
			AppDrawPickScene *scene = &pick->building;
			if (scene->frame != frame) {
				scene->frame = frame;
				scene->count = 0;
				const EgWindowsWindowState *window_state = ecs_get(it->world, window, EgWindowsWindowState);
				const EgWindowsWindow *window_object = ecs_get(it->world, window, EgWindowsWindow);
				scene->valid = window_state && window_state->valid && window_object &&
					window_state->object == window_object->object;
				if (scene->valid) {
					scene->width = window_state->pixel_width;
					scene->height = window_state->pixel_height;
				}
				if (scene->width <= 0 || scene->height <= 0) scene->valid = false;
			}
			if (scene->valid && !AppDrawPickScene_add(scene, d->render, d->list, (float *)&c0->vp, it->world)) {
				scene->valid = false;
			}
		}
		EgDraws_List_Clear(d->list);
	}
}

static void AppDrawContext_Create(ecs_iter_t *it)
{
	AppDrawContextCreate *def      = ecs_field_self(it, AppDrawContextCreate, 0);
	ecs_entity_t          e_window = ecs_field_src(it, 1);
	EgWindowsOpenGLContext const *gl = ecs_field_shared(it, EgWindowsOpenGLContext, 1);
	EgWindowsWindow const *window = ecs_get(it->world, e_window, EgWindowsWindow);
	const EgWindowsGlBackend *backend = ecs_singleton_get(it->world, EgWindowsGlBackend);
	if (!window || !backend || !backend->current || !backend->make_current) {
		ecs_err("AppDraw: window or OpenGL backend not found");
		return;
	}
	EgWindowsGlBackend gl_backend = *backend;
	EgWindowsGlBinding previous = gl_backend.current();
	EgWindowsGlBinding binding = {window->object, gl->gl_context};
	if (!gl_backend.make_current(binding)) {
		return;
	}
	if (ecs_has_id(it->world, e_window, AppDrawGpuPicking)) {
		if (!ecs_has(it->world, e_window, AppDrawPickState)) {
			ecs_set(it->world, e_window, AppDrawPickState,
				{.gl = gl_backend,
				 .shown = {.frame = -1}, .building = {.frame = -1}});
			ecs_set(it->world, e_window, EgWindowsFrameCallback, {.complete = AppDrawPickFrame_complete});
			ecs_script_set_event_hooks(it->world, &(ecs_script_event_hooks_t){.to_local = AppDraw_ToLocal});
		}
	}

	ecs_log_set_level(0);
	ecs_log(-1, "AppDrawContext_Create: '%s'", ecs_get_name(it->world, e_window));

	for (int i = 0; i < it->count; ++i, ++def) {
		ecs_entity_t  e      = it->entities[i];
		egg_render_t *render = egg_render_init();
		EgDraws_List *list   = EgDraws_List_Create();
		if (render == NULL || list == NULL) {
			ecs_err("Failed to create render or draw context for entity %s", ecs_get_name(it->world, e));
			egg_render_destroy(render);
			EgDraws_List_Destroy(list);
			ecs_enable(it->world, e, false);
			continue;
		}

		ecs_set(it->stage, e, AppDrawContext, {render, list, 1.0f});

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
	gl_backend.make_current(previous);
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
	ecs_set_hooks(world, AppDrawPickState, {.dtor = AppDrawPickState_dtor, .move = AppDrawPickState_move});
	ecs_observer(world,
	{.query = {.terms = {{.id = ecs_id(AppDrawPickState)}}},
	.events = {EcsOnRemove},
	.callback = AppDrawPickState_remove});
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
