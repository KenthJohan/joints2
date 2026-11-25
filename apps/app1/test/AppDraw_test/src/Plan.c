#include <bake_test.h>
#include <EgCameras.h>
#include <EgWindows.h>
#include <string.h>

#include "../../../src/AppFramePlan.c"

static ecs_world_t *world;
static ecs_entity_t window, camera, scene, hud, source;
static AppFramePlan plan;
static int native_window, native_context;

void Plan_setup(void)
{
	world = ecs_init();
	ECS_IMPORT(world, EgWindows);
	ECS_IMPORT(world, EgCameras);
	ecs_import(world, EgDraws_Import, "EgDraws");
	AppFramePlanImport(world);
	window = ecs_new(world);
	ecs_set(world, window, EgWindowsWindow, {
		.object = &native_window, .valid = true, .pixel_width = 640, .pixel_height = 480});
	ecs_set(world, window, EgWindowsOpenGLContext, {.gl_context = &native_context});
	ecs_set(world, window, AppFrameClear, {.b = 1, .a = 1});
	camera = ecs_new(world);
	ecs_set(world, camera, EgCamerasState, {
		.pixel_scale = 1, .vp = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}});
	scene = ecs_new_w_pair(world, EcsChildOf, window);
	hud = ecs_new_w_pair(world, EcsChildOf, window);
	ecs_set(world, scene, AppFramePass, {.order = 10});
	ecs_set(world, hud, AppFramePass, {.order = 20});
	ecs_add_pair(world, scene, AppFrameCamera, camera);
	ecs_add_pair(world, hud, AppFrameCamera, camera);
	source = ecs_new(world);
	ecs_add(world, source, EgDraws_List);
	ecs_add_pair(world, source, EgDraws_Camera, camera);
	ecs_add_pair(world, scene, AppFrameSource, source);
	ecs_add_pair(world, hud, AppFrameSource, source);
	plan = (AppFramePlan){0};
}

void Plan_teardown(void)
{
	AppFramePlanFini(&plan);
	ecs_fini(world);
}

static void add_span(ecs_entity_t root, ecs_entity_t entity,
EgDraws_PrimitiveKind kind, int32_t layer, uint32_t marker)
{
	EgDraws_List *list = ecs_get_mut(world, root, EgDraws_List);
	ecs_vec_t *vertices = EgDraws_List_GetLayer(list, layer);
	test_assert(vertices != NULL);
	int32_t first = ecs_vec_count(vertices);
	EgDraws_Vertex *vertex = ecs_vec_grow_t(NULL, vertices, EgDraws_Vertex, 3);
	test_assert(vertex != NULL);
	for (int i = 0; i < 3; ++i)
		vertex[i] = (EgDraws_Vertex){.pick_id = marker};
	EgDraws_Span *span = ecs_vec_append_t(NULL, &list->spans, EgDraws_Span);
	test_assert(span != NULL);
	*span = (EgDraws_Span){entity, layer, first, 3, kind};
}

void Plan_test_compile_snapshot(void)
{
	ecs_entity_t low = ecs_new(world), high = ecs_new(world);
	ecs_entity_t other_source = ecs_new(world);
	ecs_add(world, other_source, EgDraws_List);
	ecs_add_pair(world, other_source, EgDraws_Camera, camera);
	ecs_add_pair(world, scene, AppFrameSource, other_source);
	add_span(source, high, EgDrawsPrimitiveSolid, 0, 3);
	add_span(source, low, EgDrawsPrimitiveBorder, 0, 2);
	add_span(other_source, low, EgDrawsPrimitiveSolid, 0, 1);
	test_int(AppFramePlanCompile(world, window, 42, &plan), AppFrameReady);
	test_int(plan.instruction_count, 4);
	test_int(plan.instructions[0].op, AppFrameSetViewport);
	test_int(plan.instructions[0].args.viewport.width, 640);
	test_int(plan.instructions[0].args.viewport.height, 480);
	test_int(plan.instructions[1].op, AppFrameClearColor);
	test_flt(plan.instructions[1].args.clear.b, 1);
	test_int(plan.instructions[2].op, AppFrameDrawLayers);
	test_assert(plan.instructions[2].args.draw.pass == scene);
	test_assert(plan.instructions[3].args.draw.pass == hud);
	test_flt(plan.instructions[2].args.draw.matrix[0], 1);
	ecs_vec_t *layer = EgDraws_List_GetLayer(plan.layers[0], 0);
	test_int(ecs_vec_count(layer), 9);
	const EgDraws_Vertex *vertices = ecs_vec_first_t(layer, EgDraws_Vertex);
	test_int(vertices[0].pick_id, 1);
	test_int(vertices[3].pick_id, 2);
	test_int(vertices[6].pick_id, 3);
	ecs_entity_t churn_tag = ecs_new(world);
	ecs_add_id(world, low, churn_tag);
	ecs_add_id(world, high, churn_tag);
	ecs_remove_id(world, low, churn_tag);
	test_int(AppFramePlanCompile(world, window, 43, &plan), AppFrameReady);
	layer = EgDraws_List_GetLayer(plan.layers[0], 0);
	vertices = ecs_vec_first_t(layer, EgDraws_Vertex);
	test_int(ecs_vec_count(layer), 9);
	test_int(vertices[0].pick_id, 1);
	test_int(vertices[3].pick_id, 2);
	test_int(vertices[6].pick_id, 3);
	EgDraws_List_Clear(ecs_get_mut(world, source, EgDraws_List));
	EgCamerasState *mutable_camera = ecs_get_mut(world, camera, EgCamerasState);
	mutable_camera->vp.c0[0] = 2;
	test_int(ecs_vec_count(layer), 9);
	test_int(vertices[0].pick_id, 1);
	test_flt(plan.instructions[2].args.draw.matrix[0], 1);
}

void Plan_test_reject_invalid(void)
{
	test_int(AppFramePlanCompile(world, window, 1, &plan), AppFrameReady);
	ecs_set(world, hud, AppFramePass, {.order = 10});
	test_int(AppFramePlanCompile(world, window, 2, &plan), AppFrameInvalid);
	test_assert(!plan.instructions && !plan.layers);
	ecs_set(world, hud, AppFramePass, {.order = 20});
	ecs_remove_pair(world, scene, AppFrameCamera, camera);
	test_int(AppFramePlanCompile(world, window, 3, &plan), AppFrameInvalid);
	test_assert(!plan.instructions);
	ecs_add_pair(world, scene, AppFrameCamera, camera);
	ecs_enable(world, source, false);
	test_int(AppFramePlanCompile(world, window, 4, &plan), AppFrameInvalid);
	ecs_enable(world, source, true);
	EgDraws_List *list = ecs_get_mut(world, source, EgDraws_List);
	ecs_vec_t *layer = EgDraws_List_GetLayer(list, 0);
	test_assert(ecs_vec_grow_t(NULL, layer, EgDraws_Vertex, 3));
	test_int(AppFramePlanCompile(world, window, 5, &plan), AppFrameInvalid);
	test_assert(!plan.instructions);
	EgDraws_List_Clear(list);
	ecs_remove_pair(world, scene, AppFrameSource, source);
	test_int(AppFramePlanCompile(world, window, 6, &plan), AppFrameInvalid);
	test_assert(!plan.instructions);
	ecs_add_pair(world, scene, AppFrameSource, source);
	ecs_entity_t intermediate = ecs_new_w_pair(world, EcsChildOf, window);
	ecs_add_pair(world, scene, EcsChildOf, intermediate);
	test_int(AppFramePlanCompile(world, window, 7, &plan), AppFrameInvalid);
	test_assert(!plan.instructions);
}

void Plan_test_source_camera_and_skip(void)
{
	ecs_entity_t other = ecs_new(world);
	ecs_set(world, other, EgCamerasState, {.pixel_scale = 2});
	ecs_remove_pair(world, scene, AppFrameCamera, camera);
	ecs_add_pair(world, scene, AppFrameCamera, other);
	test_int(AppFramePlanCompile(world, window, 1, &plan), AppFrameInvalid);
	ecs_set(world, other, EgCamerasState, {.pixel_scale = 1});
	test_int(AppFramePlanCompile(world, window, 2, &plan), AppFrameReady);
	EgWindowsWindow *native = ecs_get_mut(world, window, EgWindowsWindow);
	native->pixel_width = 0;
	test_int(AppFramePlanCompile(world, window, 3, &plan), AppFrameSkipped);
	test_assert(!plan.instructions);
}

void Plan_test_deleted_references(void)
{
	test_int(AppFramePlanCompile(world, window, 1, &plan), AppFrameReady);
	ecs_delete(world, source);
	test_int(AppFramePlanCompile(world, window, 2, &plan), AppFrameInvalid);
	test_assert(!plan.instructions && !plan.layers);

	source = ecs_new(world);
	ecs_add(world, source, EgDraws_List);
	ecs_add_pair(world, source, EgDraws_Camera, camera);
	ecs_add_pair(world, scene, AppFrameSource, source);
	ecs_add_pair(world, hud, AppFrameSource, source);
	test_int(AppFramePlanCompile(world, window, 3, &plan), AppFrameReady);
	ecs_delete(world, camera);
	test_int(AppFramePlanCompile(world, window, 4, &plan), AppFrameInvalid);
	test_assert(!plan.instructions && !plan.layers);
}
