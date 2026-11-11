#include "AppDraw.h"
#include <EgWindows.h>
#include <EgCameras.h>
#include <EgSpatials.h>
#include <EgShapes.h>
#include <EgBase.h>
#include <EgDraws.h>
#include <ecsx.h>
#include <egg.h>

ECS_COMPONENT_DECLARE(AppDrawContext);
ECS_COMPONENT_DECLARE(AppDrawContextCreate);

static void Test_Render(ecs_iter_t *it)
{
	AppDrawContext *d  = ecs_field_self(it, AppDrawContext, 1);
	EgCamerasState *c0 = ecs_field_shared(it, EgCamerasState, 2);
	for (int i = 0; i < it->count; ++i, ++d) {
		ecs_assert(d->render != NULL, ECS_INTERNAL_ERROR, NULL);
		ecs_assert(d->list != NULL, ECS_INTERNAL_ERROR, NULL);
		d->pixelScale = c0->pixel_scale * 1.0f; // Keep the egg-scale near the camera-derived size.
		EgDraws_List_SetPixelScale(d->list, d->pixelScale);
		egg_draw_layers(d->render, &d->list->layers, (float *)&c0->vp);
		EgDraws_List_Clear(d->list);
	}
}

static void AppDrawContext_Create(ecs_iter_t *it)
{
	ecs_log_set_level(0);
	AppDrawContextCreate *def      = ecs_field_self(it, AppDrawContextCreate, 0);
	ecs_entity_t          e_window = ecs_field_src(it, 1);

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
	ecs_import(world, EgDraws_Import, "EgDraws");

	ECS_COMPONENT_DEFINE(world, AppDrawContext);
	ECS_COMPONENT_DEFINE(world, AppDrawContextCreate);

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
