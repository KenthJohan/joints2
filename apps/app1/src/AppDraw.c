#include "AppDraw.h"
#include "fs.h"
#include <EgWindows.h>
#include <EgCameras.h>
#include <EgSpatials.h>
#include <ecsx.h>
#include <draw.h>

ECS_COMPONENT_DECLARE(AppDrawContext);
ECS_COMPONENT_DECLARE(AppDrawContextCreate);
ECS_COMPONENT_DECLARE(AppDrawPrintPositionalBinding);

static bool BuildDrawCreateInfo(draw_create_info_t *createInfo)
{
	*createInfo = (draw_create_info_t){0};

	createInfo->shaders[DRAW_SHADER_BACKGROUND_VERTEX]      = fs_read_allocated("data/background.vs");
	createInfo->shaders[DRAW_SHADER_BACKGROUND_FRAGMENT]    = fs_read_allocated("data/background.fs");
	createInfo->shaders[DRAW_SHADER_POINT_VERTEX]           = fs_read_allocated("data/point.vs");
	createInfo->shaders[DRAW_SHADER_POINT_FRAGMENT]         = fs_read_allocated("data/point.fs");
	createInfo->shaders[DRAW_SHADER_LINE_VERTEX]            = fs_read_allocated("data/line.vs");
	createInfo->shaders[DRAW_SHADER_LINE_FRAGMENT]          = fs_read_allocated("data/line.fs");
	createInfo->shaders[DRAW_SHADER_CIRCLE_VERTEX]          = fs_read_allocated("data/circle.vs");
	createInfo->shaders[DRAW_SHADER_CIRCLE_FRAGMENT]        = fs_read_allocated("data/circle.fs");
	createInfo->shaders[DRAW_SHADER_SOLID_CIRCLE_VERTEX]    = fs_read_allocated("data/solid_circle.vs");
	createInfo->shaders[DRAW_SHADER_SOLID_CIRCLE_FRAGMENT]  = fs_read_allocated("data/solid_circle.fs");
	createInfo->shaders[DRAW_SHADER_SOLID_CAPSULE_VERTEX]   = fs_read_allocated("data/solid_capsule.vs");
	createInfo->shaders[DRAW_SHADER_SOLID_CAPSULE_FRAGMENT] = fs_read_allocated("data/solid_capsule.fs");
	createInfo->shaders[DRAW_SHADER_SOLID_POLYGON_VERTEX]   = fs_read_allocated("data/solid_polygon.vs");
	createInfo->shaders[DRAW_SHADER_SOLID_POLYGON_FRAGMENT] = fs_read_allocated("data/solid_polygon.fs");
	createInfo->shaders[DRAW_SHADER_TEXT_VERTEX]            = fs_read_allocated("data/text.vs");
	createInfo->shaders[DRAW_SHADER_TEXT_FRAGMENT]          = fs_read_allocated("data/text.fs");

	for (int i = 0; i < DRAW_SHADER_COUNT; ++i) {
		if (createInfo->shaders[i] == NULL) {
			fprintf(stderr, "Failed to load one or more shader files from apps/app1/data or data\n");
			return false;
		}
	}

	return true;
}

static void FreeDrawCreateInfo(draw_create_info_t *createInfo)
{
	for (int i = 0; i < DRAW_SHADER_COUNT; ++i) {
		free((void *)createInfo->shaders[i]);
	}
	*createInfo = (draw_create_info_t){0};
}

static void Test_Render(ecs_iter_t *it)
{
	AppDrawContext *draw   = ecs_field_self(it, AppDrawContext, 1);
	EgCamerasState *camera = ecs_field_shared(it, EgCamerasState, 2);
	for (int i = 0; i < it->count; ++i, ++draw) {
		// Placeholder for rendering logic. This function will be called every frame to handle rendering tasks.
		// printf("Test_Render called with %d entities\n", it->count);

		/*
		b2WorldTransform transform = {{1.0f, 0.0f}, {1.0f, 0.0f}};
		draw_solid_circle(draw->draw, transform, (b2Pos){0.0f, 0.0f}, 10.0f, b2_colorRed);
		*/

		float pixelScale = 100.1f; // Placeholder for pixel scale, can be adjusted based on window size or other factors
		draw_flush(draw->draw, pixelScale, (float *)&camera->vp);
	}
}

static void AppDrawContext_Create(ecs_iter_t *it)
{
	ecs_log_set_level(0);
	AppDrawContextCreate *def      = ecs_field(it, AppDrawContextCreate, 0); // self
	ecs_entity_t          e_window = ecs_field_src(it, 1);
	printf("window_entity: %s\n", ecs_get_name(it->world, e_window));
	for (int i = 0; i < it->count; ++i, ++def) {
		draw_create_info_t drawCreateInfo;
		if (!BuildDrawCreateInfo(&drawCreateInfo)) {
			continue; // Skip this entity if shader loading failed
		}
		draw_t *draw = draw_init(&drawCreateInfo);
		FreeDrawCreateInfo(&drawCreateInfo);

		ecs_set(it->world, it->entities[i], AppDrawContext, {draw});

		// The window system will call this render system using `ecs_run()` every frame
		// by putting it as a child of the window entity.
		ecs_system(it->world,
		{.entity     = ecs_entity(it->world, {.parent = e_window}),
		.callback    = Test_Render,
		.query.terms = {
		{.id = ecs_childof(e_window)},
		{.id = ecs_id(AppDrawContext), .src.id = EcsSelf, .inout = EcsIn},
		{.id = ecs_id(EgCamerasState), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
		}});
	}
	ecs_log_set_level(-1);
}

void AppDrawPrintPositional_Draw(ecs_iter_t *it)
{
	AppDrawContext *d = ecs_field_shared(it, AppDrawContext, 0);
	Position2      *p = ecs_field_self(it, Position2, 1);
	for (int i = 0; i < it->count; i++) {
		char const *name = ecs_get_name(it->world, it->entities[i]);
		draw_string(d->draw, (b2Pos){p->x, p->y}, b2_colorViolet, "%s", name);
	}
}

void AppDrawPrintPositionalBinding_Observer(ecs_iter_t *it)
{
	AppDrawPrintPositionalBinding *o = ecs_field_self(it, AppDrawPrintPositionalBinding, 0);

	for (int i = 0; i < it->count; i++) {
		ecs_entity_t e    = it->entities[i];
		char const  *name = ecs_get_name(it->world, e);
		char         buffer[256];
		snprintf(buffer, sizeof(buffer), "sys_%s_%s", name, ecs_get_name(it->world, o->term));
		if (it->event == EcsOnSet) {
			ecs_system(it->world,
			{.entity  = ecs_entity(it->world, {.name = buffer, .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
			.callback = AppDrawPrintPositional_Draw,
			.query.terms =
			{
			{.id = ecs_id(AppDrawContext), .src.id = o->draw_e, .inout = EcsIn},
			{.id = ecs_id(Position2), .src.id = EcsSelf},
			{.id = ecs_id(AppDrawPrintPositionalBinding), .src.id = e},
			{.id = o->term, .src.id = EcsSelf},
			}});
		}
	}
}

void AppDrawImport(ecs_world_t *world)
{
	ECS_MODULE(world, AppDraw);
	ecs_set_name_prefix(world, "AppDraw");
	ECS_IMPORT(world, EgWindows);
	ECS_IMPORT(world, EgSpatials);

	ECS_COMPONENT_DEFINE(world, AppDrawContext);
	ECS_COMPONENT_DEFINE(world, AppDrawContextCreate);
	ECS_COMPONENT_DEFINE(world, AppDrawPrintPositionalBinding);

	ecs_struct(world,
	{.entity = ecs_id(AppDrawContextCreate),
	.members = {
	{.name = "dummy", .type = ecs_id(ecs_i32_t)},
	}});

	ecs_struct(world,
	{.entity = ecs_id(AppDrawContext),
	.members = {
	{.name = "draw", .type = ecs_id(ecs_uptr_t)},
	}});

	ecs_struct(world,
	{.entity = ecs_id(AppDrawPrintPositionalBinding),
	.members = {
	{.name = "term", .type = ecs_id(ecs_id_t)},
	{.name = "draw_e", .type = ecs_id(ecs_id_t)},
	}});

	ecs_system(world,
	{.entity   = ecs_entity(world, {.name = "AppDrawContext_Create", .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
	.callback  = AppDrawContext_Create,
	.immediate = true,
	.query.terms =
	{
	{.id = ecs_id(AppDrawContextCreate), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgWindowsOpenGLContext), .trav = EcsChildOf, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(AppDrawContext), .oper = EcsNot}, // Adds this
	}});

	ecs_observer(world,
	{.query   = {.terms = {{.id = ecs_id(AppDrawPrintPositionalBinding)}}},
	.events   = {EcsOnSet},
	.callback = AppDrawPrintPositionalBinding_Observer});
}
