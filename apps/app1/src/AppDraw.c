#include "AppDraw.h"
#include "fs.h"
#include <EgWindows.h>

ECS_COMPONENT_DECLARE(AppDrawContext);
ECS_COMPONENT_DECLARE(AppDrawContextCreate);

static bool BuildDrawCreateInfo(DrawCreateInfo *createInfo)
{
	*createInfo = (DrawCreateInfo){0};

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

static void FreeDrawCreateInfo(DrawCreateInfo *createInfo)
{
	for (int i = 0; i < DRAW_SHADER_COUNT; ++i) {
		free((void *)createInfo->shaders[i]);
	}
	*createInfo = (DrawCreateInfo){0};
}

static void Test_Render(ecs_iter_t *it)
{
	// Placeholder for rendering logic. This function will be called every frame to handle rendering tasks.
	printf("Test_Render called with %d entities\n", it->count);
}




static void AppDrawContext_Create(ecs_iter_t *it)
{
	ecs_log_set_level(0);
	AppDrawContextCreate *def = ecs_field(it, AppDrawContextCreate, 0); // self
	for (int i = 0; i < it->count; ++i, ++def) {
		DrawCreateInfo drawCreateInfo;
		if (!BuildDrawCreateInfo(&drawCreateInfo)) {
			continue; // Skip this entity if shader loading failed
		}
		Draw *draw = CreateDraw(&drawCreateInfo);
		FreeDrawCreateInfo(&drawCreateInfo);
		ecs_set(it->world, it->entities[i], AppDrawContext, {draw});
		ecs_system(it->world,
		{.entity  = it->entities[i],
		.callback = Test_Render,
		.query.terms =
		{
		{.id = ecs_pair(EcsChildOf, it->entities[i])},
		}});
	}
	ecs_log_set_level(-1);
}



void AppDrawImport(ecs_world_t *world)
{
	ECS_MODULE(world, AppDraw);
	ecs_set_name_prefix(world, "AppDraw");

	ECS_COMPONENT_DEFINE(world, AppDrawContext);
	ECS_COMPONENT_DEFINE(world, AppDrawContextCreate);

	ecs_struct(world,
	{.entity = ecs_id(AppDrawContextCreate),
	.members = {
	{.name = "dummy", .type = ecs_id(ecs_i32_t)},
	}});


	ecs_system(world,
	{.entity  = ecs_entity(world, {.name = "AppDrawContext_Create", .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
	.callback = AppDrawContext_Create,
	.immediate = true,
	.query.terms =
	{
	{.id = ecs_id(AppDrawContextCreate), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(AppDrawContext), .oper = EcsNot}, // Adds this
	}});
}
