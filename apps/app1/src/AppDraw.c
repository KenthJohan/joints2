#include "AppDraw.h"
#include <EgWindows.h>
#include <EgCameras.h>
#include <EgSpatials.h>
#include <EgShapes.h>
#include <EgBase.h>
#include <EgDraws.h>
#include <EgUi.h>
#include <ecsx.h>
#include <egg.h>
#include <SDL3/SDL.h>

ECS_COMPONENT_DECLARE(AppDrawContext);
ECS_COMPONENT_DECLARE(AppDrawContextCreate);
ECS_TAG_DECLARE(AppDrawGpuPicking);

typedef struct {
	egg_pick_target_t *target;
	SDL_Window *window;
	SDL_GLContext gl_context;
	int64_t frame;
	int width;
	int height;
	bool valid;
} AppDrawPickState;

ECS_COMPONENT_DECLARE(AppDrawPickState);

static void AppDrawPickState_dtor(void *ptr, int32_t count, const ecs_type_info_t *ti)
{
	(void)ti;
	AppDrawPickState *states = ptr;
	for (int32_t i = 0; i < count; ++i) {
		if (!states[i].target) continue;
		SDL_Window *previous_window = SDL_GL_GetCurrentWindow();
		SDL_GLContext previous_context = SDL_GL_GetCurrentContext();
		if (!SDL_GL_MakeCurrent(states[i].window, states[i].gl_context)) {
			ecs_err("AppDraw: cannot make picking context current: %s", SDL_GetError());
			continue;
		}
		egg_pick_target_destroy(states[i].target);
		if (!SDL_GL_MakeCurrent(previous_window, previous_context)) {
			ecs_err("AppDraw: cannot restore OpenGL context: %s", SDL_GetError());
		}
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

static bool AppDraw_PickMouse(ecs_world_t *world, ecs_entity_t *target,
	float *screen_x, float *screen_y, void *ctx)
{
	(void)ctx;
	SDL_Window *focus = SDL_GetMouseFocus();
	ecs_entity_t window = 0;
	bool any_gpu_window = false;
	ecs_iter_t it = ecs_each_id(world, ecs_id(EgWindowsWindow));
	ecs_iter_t *iter = &it;
	while (ecs_each_next(iter)) {
		EgWindowsWindow const *windows = ecs_field(iter, EgWindowsWindow, 0);
		for (int32_t i = 0; i < it.count; ++i) {
			bool gpu = ecs_has_id(world, it.entities[i], AppDrawGpuPicking);
			any_gpu_window |= gpu;
			if (windows[i].object == focus) window = it.entities[i];
		}
	}
	if (!window) {
		return focus == NULL && any_gpu_window;
	}
	if (!ecs_has_id(world, window, AppDrawGpuPicking)) return false;

	*target = 0;
	float x, y;
	SDL_GetMouseState(&x, &y);
	*screen_x = x;
	*screen_y = y;
	AppDrawPickState const *state = ecs_get(world, window, AppDrawPickState);
	if (!state || !state->target || !state->valid ||
		state->frame != ecs_get_world_info(world)->frame_count_total - 1) return true;
	int logical_w, logical_h, pixel_w, pixel_h;
	if (!SDL_GetWindowSize(focus, &logical_w, &logical_h) ||
		!SDL_GetWindowSizeInPixels(focus, &pixel_w, &pixel_h)) {
		ecs_err("AppDraw: cannot get window size: %s", SDL_GetError());
		return true;
	}
	if (logical_w <= 0 || logical_h <= 0 || pixel_w != state->width || pixel_h != state->height ||
		x < 0 || y < 0 || x >= logical_w || y >= logical_h) return true;

	SDL_Window *previous_window = SDL_GL_GetCurrentWindow();
	SDL_GLContext previous_context = SDL_GL_GetCurrentContext();
	if (!SDL_GL_MakeCurrent(state->window, state->gl_context)) {
		ecs_err("AppDraw: cannot make picking context current: %s", SDL_GetError());
		return true;
	}
	uint32_t id = 0;
	bool read = egg_pick_target_read(state->target,
		(int)(x * pixel_w / logical_w), (int)(y * pixel_h / logical_h), &id);
	if (!read) ecs_err("AppDraw: failed to read picking target");
	if (!SDL_GL_MakeCurrent(previous_window, previous_context)) {
		ecs_err("AppDraw: cannot restore OpenGL context: %s", SDL_GetError());
		return true;
	}
	if (read && AppDraw_IsPickable(id, world)) *target = ecs_get_alive(world, id);
	return true;
}

static void Test_Render(ecs_iter_t *it)
{
	AppDrawContext *d  = ecs_field_self(it, AppDrawContext, 1);
	EgCamerasState *c0 = ecs_field_shared(it, EgCamerasState, 2);
	ecs_entity_t window = ecs_pair_second(it->world, ecs_field_id(it, 0));
	AppDrawPickState *pick = ecs_get_mut(it->world, window, AppDrawPickState);
	for (int i = 0; i < it->count; ++i, ++d) {
		ecs_assert(d->render != NULL, ECS_INTERNAL_ERROR, NULL);
		ecs_assert(d->list != NULL, ECS_INTERNAL_ERROR, NULL);
		d->pixelScale = c0->pixel_scale * 1.0f; // Keep the egg-scale near the camera-derived size.
		EgDraws_List_SetPixelScale(d->list, d->pixelScale);
		egg_draw_layers(d->render, &d->list->layers, (float *)&c0->vp);
		if (pick) {
			int64_t frame = ecs_get_world_info(it->world)->frame_count_total;
			if (pick->frame != frame) {
				pick->frame = frame;
				pick->valid = false;
				if (!pick->target) pick->target = egg_pick_target_create();
				if (!SDL_GetWindowSizeInPixels(pick->window, &pick->width, &pick->height)) {
					ecs_err("AppDraw: cannot get framebuffer size: %s", SDL_GetError());
					pick->width = pick->height = 0;
				}
				if (pick->target && pick->width > 0 && pick->height > 0) {
					pick->valid = egg_pick_target_begin(pick->target, pick->width, pick->height);
					if (!pick->valid) ecs_err("AppDraw: cannot begin picking frame");
				}
			}
			if (pick->valid &&
				!egg_pick_draw_layers(pick->target, d->render, &d->list->layers,
					(float *)&c0->vp, AppDraw_IsPickable, it->world)) {
				ecs_err("AppDraw: cannot draw picking layers");
				pick->valid = false;
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
	if (!window) {
		ecs_err("AppDraw: window for render context not found");
		return;
	}
	SDL_Window *previous_window = SDL_GL_GetCurrentWindow();
	SDL_GLContext previous_context = SDL_GL_GetCurrentContext();
	if (!SDL_GL_MakeCurrent(window->object, gl->gl_context)) {
		ecs_err("AppDraw: cannot make render context current: %s", SDL_GetError());
		return;
	}
	if (ecs_has_id(it->world, e_window, AppDrawGpuPicking)) {
		if (!ecs_has(it->world, e_window, AppDrawPickState)) {
			ecs_set(it->world, e_window, AppDrawPickState,
				{.window = window->object, .gl_context = gl->gl_context, .frame = -1});
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
	if (!SDL_GL_MakeCurrent(previous_window, previous_context)) {
		ecs_err("AppDraw: cannot restore OpenGL context: %s", SDL_GetError());
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
	ecs_set_hooks(world, AppDrawPickState, {.dtor = AppDrawPickState_dtor, .move = AppDrawPickState_move});
	ecs_singleton_set(world, EgUiScriptEventsPicker, {.pick = AppDraw_PickMouse});

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
