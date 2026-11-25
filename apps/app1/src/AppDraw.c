#include "AppDraw.h"
#include "AppFramePlan.h"
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
#include <SDL3/SDL_opengl.h>
#include <string.h>

ECS_COMPONENT_DECLARE(AppDrawContext);
ECS_TAG_DECLARE(AppDrawGpuPicking);

typedef struct {
	egg_pick_async_t *async;
	ecs_entity_t      picked;
	int64_t           frame;
	int               width, height;
	bool              drawing;
	bool              ready;
	bool              cached;
	bool              failed;
	bool              unavailable;
} AppDrawPickState;

ECS_COMPONENT_DECLARE(AppDrawPickState);

typedef struct {
	ecs_entity_t capture_window;
	void        *capture_object;
} AppDrawPickInput;

ECS_COMPONENT_DECLARE(AppDrawPickInput);

typedef struct {
	AppFramePlan plan;
	AppFrameResult result;
} AppDrawFrame;

ECS_COMPONENT_DECLARE(AppDrawFrame);

static void AppDrawFrame_ctor(void *ptr, int32_t count, const ecs_type_info_t *ti)
{
	(void)ti;
	memset(ptr, 0, (size_t)count * sizeof(AppDrawFrame));
}

static void AppDrawFrame_dtor(void *ptr, int32_t count, const ecs_type_info_t *ti)
{
	(void)ti;
	AppDrawFrame *frames = ptr;
	for (int32_t i = 0; i < count; ++i)
		AppFramePlanFini(&frames[i].plan);
}

static void AppDrawFrame_move(void *dst, void *src, int32_t count, const ecs_type_info_t *ti)
{
	(void)ti;
	AppDrawFrame *to = dst, *from = src;
	for (int32_t i = 0; i < count; ++i) {
		AppFramePlanFini(&to[i].plan);
		to[i] = from[i];
		from[i] = (AppDrawFrame){0};
	}
}

static void AppDrawRenderer_destroy(void *resource)
{
	AppDrawContext *draw = resource;
	egg_render_destroy(draw->render);
	draw->render = NULL;
}

static bool AppDrawContext_destroy_renderer(AppDrawContext *draw, void *window,
const EgWindowsOpenGLContext *context)
{
	if (!draw || !draw->render)
		return true;
	if (!window || !context || !context->gl_context || !context->cleanup) {
		ecs_err("AppDraw: cannot destroy renderer without window and OpenGL context");
		return false;
	}
	if (!context->cleanup(window, context->gl_context, AppDrawRenderer_destroy, draw)) {
		ecs_err("AppDraw: failed to clean up renderer");
		return false;
	}
	return true;
}

static void AppDrawContext_remove(ecs_iter_t *it)
{
	if (it->event_id != ecs_id(AppDrawContext))
		return;

	AppDrawContext               *draw    = ecs_field_self(it, AppDrawContext, 0);
	const EgWindowsWindow        *native  = ecs_field_shared(it, EgWindowsWindow, 1);
	const EgWindowsOpenGLContext *context = ecs_field_shared(it, EgWindowsOpenGLContext, 2);

	ecs_log(-1, "AppDraw: removing draw context %i", it->count);
	for (int32_t i = 0; i < it->count; ++i) {
		ecs_log(-1, "AppDraw: removing draw context %s", ecs_get_name(it->world, it->entities[i]));
		AppDrawContext_destroy_renderer(&draw[i], native ? native->object : NULL, context);
	}
}

static bool AppDrawWindow_destroy_renderers(ecs_world_t *world, ecs_entity_t window,
void *native, const EgWindowsOpenGLContext *context)
{
	bool ok = true;
	ecs_iter_t children = ecs_children(world, window);
	while (ecs_children_next(&children)) {
		for (int32_t i = 0; i < children.count; ++i) {
			ecs_entity_t child = children.entities[i];
			if (ecs_has(world, child, EgWindowsOpenGLContext))
				continue;
			AppDrawContext *draw = ecs_get_mut(world, child, AppDrawContext);
			if (!AppDrawContext_destroy_renderer(draw, native, context))
				ok = false;
			if (!AppDrawWindow_destroy_renderers(world, child, native, context))
				ok = false;
		}
	}
	return ok;
}

static void AppDrawPickTarget_destroy(void *resource)
{
	AppDrawPickState *state = resource;
	egg_pick_async_destroy(state->async);
	state->async = NULL;
}

static bool AppDrawPickState_destroy_target(AppDrawPickState *state, void *window,
const EgWindowsOpenGLContext *context)
{
	if (!state || !state->async)
		return true;
	if (!window || !context || !context->gl_context || !context->cleanup) {
		ecs_err("AppDraw: cannot destroy picking target without window and OpenGL context");
		return false;
	}
	if (!context->cleanup(window, context->gl_context, AppDrawPickTarget_destroy, state)) {
		ecs_err("AppDraw: failed to clean up picking target");
		return false;
	}
	return true;
}

static bool AppDraw_ReleaseNative(ecs_world_t *world, ecs_entity_t entity,
void *native, void *native_context)
{
	AppDrawFrame *frame = ecs_get_mut(world, entity, AppDrawFrame);
	if (frame) {
		AppFramePlanFini(&frame->plan);
		frame->result = AppFrameInvalid;
	}
	const EgWindowsOpenGLContext *context = ecs_get(world, entity, EgWindowsOpenGLContext);
	if (native_context && (!context || context->gl_context != native_context)) {
		ecs_err("AppDraw: owning GL context is unavailable during release");
		return false;
	}
	AppDrawPickState *pick = ecs_get_mut(world, entity, AppDrawPickState);
	bool pick_ok = AppDrawPickState_destroy_target(pick, native, context);
	bool renderers_ok = AppDrawWindow_destroy_renderers(world, entity, native, context);
	return pick_ok && renderers_ok;
}

static void AppDrawPickState_remove(ecs_iter_t *it)
{
	for (int32_t i = 0; i < it->count; ++i) {
		ecs_entity_t                  entity  = it->entities[i];
		AppDrawPickState             *state   = ecs_field(it, AppDrawPickState, 0) + i;
		const EgWindowsWindow        *window  = ecs_get(it->world, entity, EgWindowsWindow);
		const EgWindowsOpenGLContext *context = ecs_get(it->world, entity, EgWindowsOpenGLContext);
		AppDrawPickState_destroy_target(state, window ? window->object : NULL, context);
	}
}

static void AppDrawGpuPicking_remove(ecs_iter_t *it)
{
	for (int32_t i = 0; i < it->count; ++i) {
		AppDrawPickState *state = ecs_get_mut(it->world, it->entities[i], AppDrawPickState);
		if (!state)
			continue;
		state->cached = false;
		state->picked = 0;
		egg_pick_async_invalidate(state->async);
	}
}

static void AppDrawPickWindow_remove(ecs_iter_t *it)
{
	EgWindowsWindow *windows = ecs_field(it, EgWindowsWindow, 0);
	for (int32_t i = 0; i < it->count; ++i) {
		ecs_entity_t                  entity  = it->entities[i];
		AppDrawPickState             *state   = ecs_get_mut(it->world, entity, AppDrawPickState);
		const EgWindowsOpenGLContext *context = ecs_get(it->world, entity, EgWindowsOpenGLContext);
		AppDrawPickState_destroy_target(state, windows[i].object, context);
		AppDrawWindow_destroy_renderers(it->world, entity, windows[i].object, context);
	}
}

static void AppDrawPickContext_remove(ecs_iter_t *it)
{
	EgWindowsOpenGLContext *contexts = ecs_field(it, EgWindowsOpenGLContext, 0);
	for (int32_t i = 0; i < it->count; ++i) {
		ecs_entity_t           entity = it->entities[i];
		AppDrawPickState      *state  = ecs_get_mut(it->world, entity, AppDrawPickState);
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
	ecs_entity_t e     = ecs_get_alive(world, id);
	if (!e || !ecs_has_id(world, e, EgUiScriptEventsTarget))
		return false;
	for (ecs_entity_t p = e; p; p = ecs_get_parent(world, p)) {
		if (ecs_has_id(world, p, EcsDisabled))
			return false;
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
	if (!state)
		return;
	state->drawing                          = false;
	state->ready                            = false;
	state->failed                           = false;
	const EgWindowsInputState *focus        = ecs_singleton_get(world, EgWindowsInputState);
	const EgWindowsWindow     *window_state = ecs_get(world, window, EgWindowsWindow);
	if (!ecs_has_id(world, window, AppDrawGpuPicking) ||
	    !focus || focus->mouse_window != window || !window_state || !window_state->valid ||
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
	state->width  = window_state->pixel_width;
	state->height = window_state->pixel_height;
	if (state->unavailable)
		return;
	if (!state->async) {
		state->async = egg_pick_async_create();
		if (!state->async) {
			ecs_err("AppDraw: GPU picking is unavailable for this context");
			state->unavailable = true;
			return;
		}
	}
	uint64_t id = 0, completed_frame = 0;
	int      poll = egg_pick_async_poll(state->async, &id, &completed_frame);
	if (poll < 0) {
		ecs_err("AppDraw: failed to poll GPU picking");
		state->cached      = false;
		state->picked      = 0;
		state->unavailable = true;
		egg_pick_async_invalidate(state->async);
		return;
	}
	if (poll > 0 && (!state->cached || completed_frame > (uint64_t)state->frame)) {
		state->picked = id;
		state->frame  = (int64_t)completed_frame;
		state->cached = true;
	}
	int x       = (int)(window_state->mouse_x * state->width / window_state->width);
	int y       = state->height - 1 - (int)(window_state->mouse_y * state->height / window_state->height);
	int started = egg_pick_async_begin(state->async, x, y,
	(uint64_t)ecs_get_world_info(world)->frame_count_total);
	if (started < 0) {
		ecs_err("AppDraw: failed to begin GPU picking");
		state->cached      = false;
		state->picked      = 0;
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
		state->failed      = true;
		state->unavailable = true;
		state->cached      = false;
		state->picked      = 0;
		egg_pick_async_invalidate(state->async);
		ecs_err("AppDraw: failed to render asynchronous picking");
	}
}

static void AppDrawPickFrame_end(ecs_world_t *world, ecs_entity_t window)
{
	AppDrawPickState *state = ecs_get_mut(world, window, AppDrawPickState);
	if (!state || !state->drawing)
		return;
	state->drawing = false;
	state->ready   = egg_pick_async_end(state->async);
	if (!state->ready) {
		ecs_err("AppDraw: failed to finish GPU picking");
		state->cached      = false;
		state->picked      = 0;
		state->unavailable = true;
		egg_pick_async_invalidate(state->async);
	}
}

static void AppDrawPickFrame_complete(ecs_world_t *world, ecs_entity_t window, bool displayed)
{
	AppDrawPickState *state = ecs_get_mut(world, window, AppDrawPickState);
	if (!state)
		return;
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
	AppDrawPickInput          *input = ecs_singleton_ensure(world, AppDrawPickInput);
	input->capture_window            = focus ? focus->mouse_window : 0;
	const EgWindowsWindow *window    = input->capture_window ? ecs_get(world, input->capture_window, EgWindowsWindow) : NULL;
	input->capture_object            = window ? window->object : NULL;
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
	if (!input || !input->capture_window || !ecs_is_alive(world, input->capture_window))
		return false;
	const EgWindowsInputState *focus = ecs_singleton_get(world, EgWindowsInputState);
	if (!focus || focus->mouse_window != input->capture_window)
		return false;
	const EgWindowsWindow *window = ecs_get(world, input->capture_window, EgWindowsWindow);
	if (!window || window->object != input->capture_object || !window->valid)
		return false;
	if (!ecs_has_id(world, input->capture_window, AppDrawGpuPicking) ||
	    ecs_has_id(world, input->capture_window, EcsDisabled) ||
	    ecs_has_id(world, input->capture_window, EgWindowsCloseRequest) ||
	    !window->mouse_focus || !window->keyboard_focus || window->hidden || window->minimized)
		return false;
	return AppDraw_InsideWindow(window->mouse_x, window->mouse_y, window->width, window->height);
}

static void AppDraw_CaptureEnd(ecs_world_t *world, void *ctx)
{
	(void)ctx;
	AppDrawPickInput *input = ecs_singleton_get_mut(world, AppDrawPickInput);
	if (!input)
		return;
	input->capture_window = 0;
	input->capture_object = NULL;
}

static bool AppDraw_PickMouse(ecs_world_t *world, ecs_entity_t *target,
float *screen_x, float *screen_y, void *ctx)
{
	(void)ctx;
	const EgWindowsInputState *focus  = ecs_singleton_get(world, EgWindowsInputState);
	ecs_entity_t               window = focus ? focus->mouse_window : 0;
	if (window && !ecs_is_alive(world, window))
		window = 0;
	const EgWindowsWindow *window_state = window ? ecs_get(world, window, EgWindowsWindow) : NULL;
	if (!window_state ||
	    !window_state->mouse_focus || window_state->hidden || window_state->minimized) {
		window_state = NULL;
	}
	AppDrawPickInput *input = ecs_singleton_ensure(world, AppDrawPickInput);
	if (input->capture_window && window == input->capture_window && window_state && window_state->valid &&
	    window_state->object == input->capture_object) {
		*screen_x = window_state->mouse_x;
		*screen_y = window_state->mouse_y;
		*target   = 0;
		return true;
	}
	if (!window) {
		ecs_iter_t  states = ecs_each(world, AppDrawPickState);
		ecs_iter_t *iter   = &states;
		while (ecs_each_next(iter)) {
			AppDrawPickState *values = ecs_field(iter, AppDrawPickState, 0);
			for (int32_t i = 0; i < states.count; ++i) {
				values[i].cached = false;
				values[i].picked = 0;
				egg_pick_async_invalidate(values[i].async);
			}
		}
		if (!focus || focus->mouse_focus)
			return false;
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
	if (!ecs_has_id(world, window, AppDrawGpuPicking))
		return false;

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
	*screen_x               = x;
	*screen_y               = y;
	AppDrawPickState *state = ecs_get_mut(world, window, AppDrawPickState);
	if (!state)
		return true;
	const EgWindowsOpenGLContext *gl_context = ecs_get(world, window, EgWindowsOpenGLContext);
	if (!window_state->object || !gl_context || !gl_context->gl_context) {
		state->cached = false;
		egg_pick_async_invalidate(state->async);
		return true;
	}
	if (!state->cached)
		return true;
	int logical_w = window_state->width, logical_h = window_state->height;
	int pixel_w = window_state->pixel_width, pixel_h = window_state->pixel_height;
	if (logical_w <= 0 || logical_h <= 0 || pixel_w != state->width || pixel_h != state->height ||
	    !AppDraw_InsideWindow(x, y, logical_w, logical_h)) {
		state->cached = false;
		egg_pick_async_invalidate(state->async);
		return true;
	}

	if (state->picked && ecs_is_alive(world, state->picked) &&
	    AppDraw_IsPickable((uint32_t)state->picked, world))
		*target = state->picked;
	return true;
}

static bool AppDraw_ExecuteFrame(ecs_world_t *world, ecs_entity_t window)
{
	AppDrawFrame *frame = ecs_get_mut(world, window, AppDrawFrame);
	const EgWindowsWindow *native = ecs_get(world, window, EgWindowsWindow);
	const EgWindowsOpenGLContext *context = ecs_get(world, window, EgWindowsOpenGLContext);
	if (!frame || frame->result != AppFrameReady)
		return false;
	AppFramePlan *plan = &frame->plan;
	if (!native || !context || !native->valid || native->object != plan->native_window ||
	    context->gl_context != plan->gl_context || native->pixel_width != plan->width ||
	    native->pixel_height != plan->height) {
		ecs_err("AppDraw: frame snapshot no longer matches window %llu", (unsigned long long)window);
		return false;
	}
	for (int32_t i = 2; i < plan->instruction_count; ++i) {
		ecs_entity_t pass = plan->instructions[i].args.draw.pass;
		if (!ecs_is_alive(world, pass) || ecs_has_id(world, pass, EcsDisabled) ||
		    ecs_get_parent(world, pass) != window) {
			ecs_err("AppDraw: render pass %llu is no longer available", (unsigned long long)pass);
			return false;
		}
		AppDrawContext *draw = ecs_get_mut(world, pass, AppDrawContext);
		if (!draw) {
			ecs_err("AppDraw: render pass %llu has no renderer state", (unsigned long long)pass);
			return false;
		}
		if (!draw->render) {
			draw->render = egg_render_init();
			if (!draw->render) {
				ecs_err("AppDraw: renderer initialization failed for pass %llu", (unsigned long long)pass);
				return false;
			}
		}
	}
	AppDrawPickFrame_begin(world, window);
	AppDrawPickState *pick = ecs_has_id(world, window, AppDrawGpuPicking) ?
		ecs_get_mut(world, window, AppDrawPickState) : NULL;
	bool ok = true;
	for (int32_t i = 0; i < plan->instruction_count; ++i) {
		const AppFrameInstruction *instruction = &plan->instructions[i];
		switch (instruction->op) {
		case AppFrameSetViewport:
			glViewport(instruction->args.viewport.x, instruction->args.viewport.y,
				instruction->args.viewport.width, instruction->args.viewport.height);
			break;
		case AppFrameClearColor:
			glClearColor(instruction->args.clear.r, instruction->args.clear.g,
				instruction->args.clear.b, instruction->args.clear.a);
			glClear(GL_COLOR_BUFFER_BIT);
			break;
		case AppFrameDrawLayers: {
			int32_t index = instruction->args.draw.layers_index;
			AppDrawContext *draw = ecs_get_mut(world, instruction->args.draw.pass, AppDrawContext);
			if (index < 0 || index >= plan->layer_count || !draw || !draw->render) {
				ecs_err("AppDraw: invalid draw instruction");
				ok = false;
				break;
			}
			AppDrawPickFrame_draw(world, pick, draw->render, plan->layers[index], instruction->args.draw.matrix);
			if (pick && pick->failed)
				ok = false;
			break;
		}
		default:
			ecs_err("AppDraw: invalid frame instruction");
			ok = false;
		}
		if (!ok)
			break;
	}
	AppDrawPickFrame_end(world, window);
	return ok && (!pick || !pick->failed);
}

static void AppDrawFrame_Create(ecs_iter_t *it)
{
	for (int32_t i = 0; i < it->count; ++i) {
		ecs_entity_t window = it->entities[i];
		EgWindowsNativeOwner *owner = ecs_get_mut(it->world, window, EgWindowsNativeOwner);
		if (ecs_has(it->world, window, EgWindowsFrameExecutor) ||
		    (owner && owner->release && owner->release != AppDraw_ReleaseNative)) {
			ecs_err("AppDraw: window %llu already has a frame or resource owner",
				(unsigned long long)window);
			continue;
		}
		if (owner)
			owner->release = AppDraw_ReleaseNative;
		ecs_add(it->stage, window, AppDrawFrame);
		ecs_set(it->stage, window, EgWindowsFrameExecutor,
			{.execute = AppDraw_ExecuteFrame, .complete = AppDrawPickFrame_complete});
	}
}

static void AppDrawPick_Create(ecs_iter_t *it)
{
	for (int32_t i = 0; i < it->count; ++i)
		ecs_set(it->stage, it->entities[i], AppDrawPickState, {.frame = -1});
	ecs_script_set_event_hooks(it->world, &(ecs_script_event_hooks_t){.to_local = AppDraw_ToLocal});
}

static void AppDrawContext_Create(ecs_iter_t *it)
{
	for (int32_t i = 0; i < it->count; ++i)
		ecs_set(it->stage, it->entities[i], AppDrawContext, {NULL});
}

static void AppDrawFrame_Compile(ecs_iter_t *it)
{
	AppDrawFrame *frames = ecs_field(it, AppDrawFrame, 0);
	for (int32_t i = 0; i < it->count; ++i)
		frames[i].result = AppFramePlanCompile(it->world, it->entities[i],
			(uint64_t)ecs_get_world_info(it->world)->frame_count_total, &frames[i].plan);
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
	ecs_set_name_prefix(world, "AppFrame");
	AppFramePlanImport(world);
	ecs_set_name_prefix(world, "AppDraw");

	ECS_COMPONENT_DEFINE(world, AppDrawContext);
	ECS_COMPONENT_DEFINE(world, AppDrawFrame);
	ECS_TAG_DEFINE(world, AppDrawGpuPicking);
	ECS_COMPONENT_DEFINE(world, AppDrawPickState);
	ECS_COMPONENT_DEFINE(world, AppDrawPickInput);
	ecs_set_hooks(world, AppDrawPickState, {.move = AppDrawPickState_move});
	ecs_set_hooks(world, AppDrawFrame, {
		.ctor = AppDrawFrame_ctor, .dtor = AppDrawFrame_dtor, .move = AppDrawFrame_move});

	ecs_observer(world,
	{.query   = {.terms = {
               {.id = ecs_id(AppDrawContext)},
               {.id = ecs_id(EgWindowsWindow), .trav = EcsChildOf, .src.id = EcsUp, .inout = EcsIn},
               {.id = ecs_id(EgWindowsOpenGLContext), .trav = EcsChildOf, .src.id = EcsUp, .inout = EcsIn}}},
	.events   = {EcsOnRemove},
	.callback = AppDrawContext_remove});

	ecs_observer(world,
	{.query   = {.terms = {{.id = ecs_id(AppDrawPickState)}}},
	.events   = {EcsOnRemove},
	.callback = AppDrawPickState_remove});

	ecs_observer(world,
	{.query   = {.terms = {{.id = AppDrawGpuPicking}}},
	.events   = {EcsOnRemove},
	.callback = AppDrawGpuPicking_remove});

	ecs_observer(world,
	{.query   = {.terms = {{.id = ecs_id(EgWindowsWindow)}}},
	.events   = {EcsOnRemove},
	.callback = AppDrawPickWindow_remove});

	ecs_observer(world,
	{.query   = {.terms = {{.id = ecs_id(EgWindowsOpenGLContext)}}},
	.events   = {EcsOnRemove},
	.callback = AppDrawPickContext_remove});

	ecs_singleton_set(world, EgUiScriptEventsPicker, {.pick = AppDraw_PickMouse, .capture_begin = AppDraw_CaptureBegin, .capture_valid = AppDraw_CaptureValid, .capture_end = AppDraw_CaptureEnd});

	ecs_struct(world,
	{.entity = ecs_id(AppDrawContext),
	.members = {
	{.name = "render", .type = ecs_id(ecs_uptr_t)},
	}});

	ecs_system(world,
	{.entity     = ecs_entity(world, {.name = "AppDrawContext_Create"}),
	.phase       = EcsOnUpdate,
	.callback    = AppDrawContext_Create,
	.immediate   = true,
	.query.terms = {
	{.id = ecs_id(AppFramePass), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgWindowsOpenGLContext), .trav = EcsChildOf, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(AppDrawContext), .oper = EcsNot}, // Adds this
	}});

	ecs_system(world,
	{.entity     = ecs_entity(world, {.name = "AppDrawFrame_Create"}),
	.phase       = EcsOnUpdate,
	.callback    = AppDrawFrame_Create,
	.immediate   = true,
	.query.terms = {
	{.id = ecs_id(AppFrameClear)},
	{.id = ecs_id(EgWindowsWindow)},
	{.id = ecs_id(EgWindowsOpenGLContext)},
	{.id = ecs_id(AppDrawFrame), .oper = EcsNot},
	}});

	ecs_system(world,
	{.entity     = ecs_entity(world, {.name = "AppDrawPick_Create"}),
	.phase       = EcsOnUpdate,
	.callback    = AppDrawPick_Create,
	.immediate   = true,
	.query.terms = {
	{.id = AppDrawGpuPicking},
	{.id = ecs_id(EgWindowsOpenGLContext)},
	{.id = ecs_id(AppDrawPickState), .oper = EcsNot},
	}});

	ecs_entity_t compile_phase = ecs_entity(world, {.name = "CompilePhase"});
	ecs_add_id(world, compile_phase, EcsPhase);
	ecs_add_pair(world, compile_phase, EcsDependsOn, EgDraws_GeometryPhase);
	ecs_add_pair(world, EcsPreStore, EcsDependsOn, compile_phase);
	ecs_add_pair(world, EcsOnStore, EcsDependsOn, EcsPreStore);
	ecs_add_pair(world, EcsPostFrame, EcsDependsOn, EcsOnStore);
	ecs_system(world,
	{.entity     = ecs_entity(world, {.name = "AppDrawFrame_Compile"}),
	.phase       = compile_phase,
	.callback    = AppDrawFrame_Compile,
	.immediate   = true,
	.query.terms = {
	{.id = ecs_id(AppDrawFrame), .inout = EcsInOut},
	}});
}
