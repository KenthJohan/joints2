#include "AppFramePlan.h"

#include <EgCameras.h>
#include <EgWindows.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

ECS_COMPONENT_DECLARE(AppFramePass);
ECS_COMPONENT_DECLARE(AppFrameClear);
ECS_TAG_DECLARE(AppFrameCamera);
ECS_TAG_DECLARE(AppFrameSource);

typedef struct {
	ecs_entity_t entity;
	int32_t order;
} FramePass;

typedef struct {
	const EgDraws_List *list;
	EgDraws_Span span;
} FrameSpan;

static bool frame_enabled(const ecs_world_t *world, ecs_entity_t entity)
{
	if (!ecs_is_alive(world, entity))
		return false;
	for (ecs_entity_t e = entity; e; e = ecs_get_parent(world, e)) {
		if (ecs_has_id(world, e, EcsDisabled))
			return false;
	}
	return true;
}

static bool frame_validate_pass_hierarchy(ecs_world_t *world, ecs_entity_t parent,
ecs_entity_t window, bool direct_children)
{
	ecs_iter_t children = ecs_children(world, parent);
	while (ecs_children_next(&children)) {
		for (int32_t i = 0; i < children.count; ++i) {
			ecs_entity_t child = children.entities[i];
			if (ecs_has(world, child, EgWindowsWindow))
				continue;
			if (!direct_children && frame_enabled(world, child) &&
			    ecs_has(world, child, AppFramePass)) {
				ecs_err("AppFrame: pass %llu is not a direct child of window %llu",
					(unsigned long long)child, (unsigned long long)window);
				ecs_iter_fini(&children);
				return false;
			}
			if (!frame_validate_pass_hierarchy(world, child, window, false)) {
				ecs_iter_fini(&children);
				return false;
			}
		}
	}
	return true;
}

static int compare_pass(const void *a, const void *b)
{
	const FramePass *x = a, *y = b;
	return (x->order > y->order) - (x->order < y->order);
}

static int compare_span(const void *a, const void *b)
{
	const FrameSpan *x = a, *y = b;
	if (x->span.layer != y->span.layer)
		return (x->span.layer > y->span.layer) - (x->span.layer < y->span.layer);
	if (x->span.entity != y->span.entity)
		return (x->span.entity > y->span.entity) - (x->span.entity < y->span.entity);
	return (x->span.kind > y->span.kind) - (x->span.kind < y->span.kind);
}

static int compare_source_span(const void *a, const void *b)
{
	const EgDraws_Span *x = a, *y = b;
	if (x->layer != y->layer)
		return (x->layer > y->layer) - (x->layer < y->layer);
	return (x->first > y->first) - (x->first < y->first);
}

void AppFramePlanFini(AppFramePlan *plan)
{
	if (!plan)
		return;
	if (plan->layers) {
		for (int32_t i = 0; i < plan->layer_count; ++i)
			EgDraws_List_Destroy(plan->layers[i]);
	}
	free(plan->layers);
	free(plan->instructions);
	*plan = (AppFramePlan){0};
}

static bool frame_copy_source(const ecs_world_t *world, ecs_entity_t pass,
ecs_entity_t camera, ecs_entity_t source, FrameSpan **spans, size_t *count)
{
	const EgDraws_List *list = ecs_get(world, source, EgDraws_List);
	ecs_entity_t source_camera = ecs_get_target(world, source, EgDraws_Camera, 0);
	const EgCamerasState *pass_state = ecs_get(world, camera, EgCamerasState);
	const EgCamerasState *source_state = source_camera ? ecs_get(world, source_camera, EgCamerasState) : NULL;
	if (!frame_enabled(world, source) || !list || list->failed ||
	    !source_camera || ecs_get_target(world, source, EgDraws_Camera, 1) ||
	    !frame_enabled(world, source_camera) || !source_state ||
	    !isfinite(source_state->pixel_scale) || source_state->pixel_scale <= 0 ||
	    source_state->pixel_scale != pass_state->pixel_scale ||
	    list->pixel_scale != source_state->pixel_scale) {
		ecs_err("AppFrame: pass %llu has invalid or camera-incompatible source %llu",
			(unsigned long long)pass, (unsigned long long)source);
		return false;
	}
	const EgDraws_Span *values = ecs_vec_first_t(&list->spans, EgDraws_Span);
	int32_t span_count = ecs_vec_count(&list->spans);
	size_t total = *count + (size_t)span_count;
	if (total < *count || total > SIZE_MAX / sizeof(**spans)) {
		ecs_err("AppFrame: too many draw spans");
		return false;
	}
	if (span_count) {
		FrameSpan *grown = realloc(*spans, total * sizeof(**spans));
		if (!grown) {
			ecs_err("AppFrame: cannot allocate draw spans");
			return false;
		}
		*spans = grown;
	}
	for (int32_t i = 0; i < span_count; ++i) {
		EgDraws_Span s = values[i];
		if (s.layer < 0 || s.layer >= ecs_vec_count(&list->layers) ||
		    s.first < 0 || s.count <= 0 || s.count % 3 || s.kind < EgDrawsPrimitiveSolid ||
		    s.kind > EgDrawsPrimitiveText ||
		    !s.entity) {
			ecs_err("AppFrame: source %llu has invalid draw span", (unsigned long long)source);
			return false;
		}
		const ecs_vec_t *layer = ecs_vec_get_t(&list->layers, ecs_vec_t, s.layer);
		if (s.first > ecs_vec_count(layer) || s.count > ecs_vec_count(layer) - s.first) {
			ecs_err("AppFrame: source %llu has out-of-range draw span", (unsigned long long)source);
			return false;
		}
		(*spans)[(*count)++] = (FrameSpan){list, s};
	}
	EgDraws_Span *coverage = span_count ? malloc((size_t)span_count * sizeof(*coverage)) : NULL;
	if (span_count && !coverage) {
		ecs_err("AppFrame: cannot allocate source coverage");
		return false;
	}
	if (span_count) {
		memcpy(coverage, values, (size_t)span_count * sizeof(*coverage));
		qsort(coverage, (size_t)span_count, sizeof(*coverage), compare_source_span);
	}
	int32_t next = 0;
	for (int32_t layer = 0; layer < ecs_vec_count(&list->layers); ++layer) {
		int32_t offset = 0;
		while (next < span_count && coverage[next].layer == layer) {
			if (coverage[next].first != offset) {
				ecs_err("AppFrame: source %llu has overlapping or missing draw spans", (unsigned long long)source);
				free(coverage);
				return false;
			}
			offset += coverage[next++].count;
		}
		const ecs_vec_t *vertices = ecs_vec_get_t(&list->layers, ecs_vec_t, layer);
		if (offset != ecs_vec_count(vertices)) {
			ecs_err("AppFrame: source %llu has untracked vertices", (unsigned long long)source);
			free(coverage);
			return false;
		}
	}
	free(coverage);
	return true;
}

static bool frame_draw(ecs_world_t *world, ecs_entity_t pass, EgDraws_List *out,
AppFrameInstruction *instruction)
{
	ecs_entity_t camera = ecs_get_target(world, pass, AppFrameCamera, 0);
	const EgCamerasState *state = camera ? ecs_get(world, camera, EgCamerasState) : NULL;
	if (!camera || ecs_get_target(world, pass, AppFrameCamera, 1) ||
	    !frame_enabled(world, camera) || !state ||
	    !isfinite(state->pixel_scale) || state->pixel_scale <= 0) {
		ecs_err("AppFrame: pass %llu needs one enabled camera", (unsigned long long)pass);
		return false;
	}
	float matrix[16];
	memcpy(matrix, &state->vp, sizeof(matrix));
	for (int i = 0; i < 16; ++i) {
		if (!isfinite(matrix[i])) {
			ecs_err("AppFrame: pass %llu has a non-finite camera matrix", (unsigned long long)pass);
			return false;
		}
	}
	FrameSpan *spans = NULL;
	size_t count = 0;
	bool ok = true;
	if (!ecs_get_target(world, pass, AppFrameSource, 0)) {
		ecs_err("AppFrame: pass %llu has no source", (unsigned long long)pass);
		return false;
	}
	for (int32_t i = 0;; ++i) {
		ecs_entity_t source = ecs_get_target(world, pass, AppFrameSource, i);
		if (!source)
			break;
		if (!frame_copy_source(world, pass, camera, source, &spans, &count)) {
			ok = false;
			break;
		}
	}
	if (ok && count)
		qsort(spans, count, sizeof(*spans), compare_span);
	for (size_t i = 0; ok && i < count; ++i) {
		if (i && compare_span(&spans[i - 1], &spans[i]) == 0) {
			ecs_err("AppFrame: pass %llu contains duplicate primitive spans for entity %llu",
				(unsigned long long)pass, (unsigned long long)spans[i].span.entity);
			ok = false;
			break;
		}
		const EgDraws_Span *span = &spans[i].span;
		const ecs_vec_t *source = ecs_vec_get_t(&spans[i].list->layers, ecs_vec_t, span->layer);
		ecs_vec_t *dest = EgDraws_List_GetLayer(out, span->layer);
		if (!dest) {
			ecs_err("AppFrame: cannot allocate prepared layer");
			ok = false;
			break;
		}
		EgDraws_Vertex *vertices = ecs_vec_grow_t(NULL, dest, EgDraws_Vertex, span->count);
		if (!vertices) {
			ecs_err("AppFrame: cannot allocate prepared vertices");
			ok = false;
			break;
		}
		memcpy(vertices, &ecs_vec_first_t(source, EgDraws_Vertex)[span->first],
			(size_t)span->count * sizeof(*vertices));
	}
	free(spans);
	if (!ok)
		return false;
	instruction->op = AppFrameDrawLayers;
	instruction->args.draw.pass = pass;
	memcpy(instruction->args.draw.matrix, &state->vp, sizeof(instruction->args.draw.matrix));
	return true;
}

AppFrameResult AppFramePlanCompile(ecs_world_t *world, ecs_entity_t window,
uint64_t frame, AppFramePlan *plan)
{
	AppFramePlanFini(plan);
	const EgWindowsWindow *native = ecs_get(world, window, EgWindowsWindow);
	const EgWindowsOpenGLContext *gl = ecs_get(world, window, EgWindowsOpenGLContext);
	const AppFrameClear *clear = ecs_get(world, window, AppFrameClear);
	if (!frame_enabled(world, window) || !native || !native->object || !gl ||
	    !gl->gl_context || !clear) {
		ecs_err("AppFrame: window %llu lacks enabled window, context, or clear color", (unsigned long long)window);
		return AppFrameInvalid;
	}
	if (!isfinite(clear->r) || !isfinite(clear->g) ||
	    !isfinite(clear->b) || !isfinite(clear->a)) {
		ecs_err("AppFrame: window %llu has a non-finite clear color", (unsigned long long)window);
		return AppFrameInvalid;
	}
	if (!native->valid || native->hidden || native->minimized ||
	    native->pixel_width <= 0 || native->pixel_height <= 0)
		return AppFrameSkipped;
	if (!frame_validate_pass_hierarchy(world, window, window, true))
		return AppFrameInvalid;
	FramePass *passes = NULL;
	size_t count = 0;
	ecs_iter_t children = ecs_children(world, window);
	while (ecs_children_next(&children)) {
		for (int32_t i = 0; i < children.count; ++i) {
			ecs_entity_t e = children.entities[i];
			const AppFramePass *value = ecs_get(world, e, AppFramePass);
			if (!value || !frame_enabled(world, e))
				continue;
			if (count >= INT32_MAX - 2 || count + 1 > SIZE_MAX / sizeof(*passes)) {
				ecs_err("AppFrame: too many passes for window %llu", (unsigned long long)window);
				ecs_iter_fini(&children);
				free(passes);
				return AppFrameInvalid;
			}
			FramePass *grown = realloc(passes, (count + 1) * sizeof(*passes));
			if (!grown) {
				ecs_err("AppFrame: cannot allocate passes");
				ecs_iter_fini(&children);
				free(passes);
				return AppFrameInvalid;
			}
			passes = grown;
			passes[count++] = (FramePass){e, value->order};
		}
	}
	if (count)
		qsort(passes, count, sizeof(*passes), compare_pass);
	for (size_t i = 1; i < count; ++i) {
		if (passes[i - 1].order == passes[i].order) {
			ecs_err("AppFrame: window %llu has duplicate pass order %d",
				(unsigned long long)window, passes[i].order);
			free(passes);
			return AppFrameInvalid;
		}
	}
	AppFramePlan result = {.window = window, .frame = frame, .native_window = native->object,
		.gl_context = gl->gl_context, .width = native->pixel_width, .height = native->pixel_height};
	result.instruction_count = (int32_t)count + 2;
	result.layer_count = (int32_t)count;
	result.instructions = calloc((size_t)result.instruction_count, sizeof(*result.instructions));
	result.layers = count ? calloc(count, sizeof(*result.layers)) : NULL;
	if (!result.instructions || (count && !result.layers)) {
		ecs_err("AppFrame: cannot allocate window plan");
		free(passes);
		AppFramePlanFini(&result);
		return AppFrameInvalid;
	}
	result.instructions[0].op = AppFrameSetViewport;
	result.instructions[0].args.viewport.width = result.width;
	result.instructions[0].args.viewport.height = result.height;
	result.instructions[1].op = AppFrameClearColor;
	result.instructions[1].args.clear = *clear;
	for (size_t i = 0; i < count; ++i) {
		result.layers[i] = EgDraws_List_Create();
		if (!result.layers[i] || !frame_draw(world, passes[i].entity, result.layers[i],
			&result.instructions[i + 2])) {
			ecs_err("AppFrame: failed to compile window %llu pass %llu",
				(unsigned long long)window, (unsigned long long)passes[i].entity);
			free(passes);
			AppFramePlanFini(&result);
			return AppFrameInvalid;
		}
		result.instructions[i + 2].args.draw.layers_index = (int32_t)i;
	}
	free(passes);
	*plan = result;
	return AppFrameReady;
}

void AppFramePlanImport(ecs_world_t *world)
{
	ECS_COMPONENT_DEFINE(world, AppFramePass);
	ECS_COMPONENT_DEFINE(world, AppFrameClear);
	ECS_TAG_DEFINE(world, AppFrameCamera);
	ECS_TAG_DEFINE(world, AppFrameSource);
	ecs_struct(world, {.entity = ecs_id(AppFramePass),
		.members = {{.name = "order", .type = ecs_id(ecs_i32_t)}}});
	ecs_struct(world, {.entity = ecs_id(AppFrameClear), .members = {
		{.name = "r", .type = ecs_id(ecs_f32_t)}, {.name = "g", .type = ecs_id(ecs_f32_t)},
		{.name = "b", .type = ecs_id(ecs_f32_t)}, {.name = "a", .type = ecs_id(ecs_f32_t)}}});
}
