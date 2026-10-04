#include "AppDraw.h"
#include <assert.h>
#include <EgWindows.h>
#include <EgCameras.h>
#include <EgSpatials.h>
#include <EgShapes.h>
#include <EgBase.h>
#include <EgShapedraw.h>
#include <ecsx.h>
#include <egg.h>
#include <math.h>

ECS_COMPONENT_DECLARE(AppDrawContext);
ECS_COMPONENT_DECLARE(AppDrawContextCreate);
ECS_COMPONENT_DECLARE(AppDrawNameAtPositionRule);

static void Test_Render(ecs_iter_t *it)
{
	AppDrawContext *d  = ecs_field_self(it, AppDrawContext, 1);
	EgCamerasState *c0 = ecs_field_shared(it, EgCamerasState, 2);
	for (int i = 0; i < it->count; ++i, ++d) {
		ecs_assert(d->render != NULL, ECS_INTERNAL_ERROR, NULL);
		ecs_assert(d->list != NULL, ECS_INTERNAL_ERROR, NULL);
		d->pixelScale = c0->pixelScale * 1.0f; // Keep the egg-scale near the camera-derived size.
		EgShapedrawList_SetPixelScale(d->list, d->pixelScale);
		egg_flush(d->render, d->list, (float *)&c0->vp);
		EgShapedrawList_Clear(d->list);
	}
}

static void AppDrawContext_Create(ecs_iter_t *it)
{
	ecs_log_set_level(0);
	AppDrawContextCreate *def      = ecs_field(it, AppDrawContextCreate, 0); // self
	ecs_entity_t          e_window = ecs_field_src(it, 1);
	printf("window_entity: %s\n", ecs_get_name(it->world, e_window));
	for (int i = 0; i < it->count; ++i, ++def) {
		ecs_entity_t     e      = it->entities[i];
		egg_render_t    *render = egg_render_init();
		EgShapedrawList *list   = EgShapedrawList_Create();
		if (render == NULL || list == NULL) {
			ecs_err("Failed to create render or draw context for entity %s", ecs_get_name(it->world, e));
			egg_render_destroy(render);
			EgShapedrawList_Destroy(list);
			ecs_enable(it->world, e, false);
			continue;
		}

		ecs_set(it->world, e, AppDrawContext, {render, list, 1.0f});

		// The window system will call this render system using `ecs_run()` every frame
		// by putting it as a child of the window entity.
		ecs_system(it->world,
		{.entity     = ecs_entity(it->world, {.parent = e_window}),
		.callback    = Test_Render,
		.query.terms = {
		{.id = ecs_childof(e_window)},
		{.id = ecs_id(AppDrawContext), .src.id = EcsSelf, .inout = EcsIn},
		{.id = ecs_id(EgCamerasState), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn}}});
	}
	ecs_log_set_level(-1);
}

void AppDrawNameAtPosition_Draw(ecs_iter_t *it)
{
	AppDrawContext            *d = ecs_field_shared(it, AppDrawContext, 0);
	WorldTransform3           *x = ecs_field_self(it, WorldTransform3, 1);
	AppDrawNameAtPositionRule *b = ecs_field_shared(it, AppDrawNameAtPositionRule, 2);

	for (int i = 0; i < it->count; ++i, ++x) {
		char const *name = ecs_get_name(it->world, it->entities[i]);

		printf("Drawing name '%s' at position (%f, %f) with rotation (c=%f, s=%f)\n", name, x->matrix.c2[0], x->matrix.c2[1], x->matrix.c0[0], x->matrix.c0[1]);
		EgShapedrawList_AddText(d->list, APP_DRAW_Z_TEXT, &(x->matrix), 0.5f, b->color, name);
		// EgShapedrawList_AddRectangleOutline(d->list, APP_DRAW_Z_SHAPES, &(x->matrix), 20, 20, 2.0f, 0x0066FF00u);

		/*
		if (strcmp(name, "cell_d") == 0) {
		    printf("Found cell_d at position (%f, %f) with rotation (c=%f, s=%f)\n", x, y, c, s);
		}
		*/
	}
}

static void AppDrawNameAtPositionRule_Observer(ecs_iter_t *it)
{
	AppDrawNameAtPositionRule *o = ecs_field_self(it, AppDrawNameAtPositionRule, 0);

	for (int i = 0; i < it->count; i++) {
		ecs_entity_t e    = it->entities[i];
		char const  *name = ecs_get_name(it->world, e);
		char         buffer[256];
		snprintf(buffer, sizeof(buffer), "sys_%s_%s", name, ecs_get_name(it->world, o->term));
		if (it->event == EcsOnSet) {
			ecs_system(it->world,
			{.entity     = ecs_entity(it->world, {.name = buffer}),
			.phase       = EcsPostUpdate,
			.callback    = AppDrawNameAtPosition_Draw,
			.query.terms = {
			{.id = ecs_id(AppDrawContext), .src.id = o->draw_e, .inout = EcsIn},
			{.id = ecs_id(WorldTransform3), .src.id = EcsSelf, .inout = EcsIn},
			{.id = ecs_id(AppDrawNameAtPositionRule), .src.id = e},
			{.id = o->term, .src.id = EcsSelf},
			}});
		}
	}
}

static void AppDrawContext_Collect(ecs_iter_t *it)
{
	AppDrawContext  *d0 = ecs_field_shared(it, AppDrawContext, 0);
	EgShapedrawList *l  = ecs_field_self(it, EgShapedrawList, 1);
	for (int i = 0; i < it->count; ++i, ++l) {
		EgShapedrawList_Append(d0->list, l);
	}
}

void AppDrawImport(ecs_world_t *world)
{
	ECS_MODULE(world, AppDraw);
	ecs_set_name_prefix(world, "AppDraw");
	ECS_IMPORT(world, EgWindows);
	ECS_IMPORT(world, EgSpatials);
	ECS_IMPORT(world, EgBase);
	ECS_IMPORT(world, EgShapedraw);

	ECS_COMPONENT_DEFINE(world, AppDrawContext);
	ECS_COMPONENT_DEFINE(world, AppDrawContextCreate);
	ECS_COMPONENT_DEFINE(world, AppDrawNameAtPositionRule);

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

	ecs_struct(world,
	{.entity = ecs_id(AppDrawNameAtPositionRule),
	.members = {
	{.name = "term", .type = ecs_id(ecs_id_t)},
	{.name = "draw_e", .type = ecs_id(ecs_id_t)},
	{.name = "color", .type = ecs_id(ecs_u32_t)},
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
	{.id = ecs_id(EgShapedrawList), .src.id = EcsSelf, .inout = EcsIn},
	}});

	ecs_observer(world,
	{.query   = {.terms = {{.id = ecs_id(AppDrawNameAtPositionRule)}}},
	.events   = {EcsOnSet},
	.callback = AppDrawNameAtPositionRule_Observer});
}
