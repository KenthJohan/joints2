#include "AppDrawBox2d.h"
#include <EgSpatials.h>
#include <EgCameras.h>
#include <EgPhysics.h>
#include <EgShapes.h>
#include <EgPhysicsBox2d.h>
#include "AppDraw.h"
#include "b2DebugDraw_init.h"
#include <ecsx.h>

ECS_COMPONENT_DECLARE(AppDrawBox2dContextCreate);
ECS_COMPONENT_DECLARE(AppDrawBox2dContext);

static void AppDrawBox2dContext_Create(ecs_iter_t *it)
{
	ecs_log_set_level(0);
	AppDrawContext            *draw = ecs_field(it, AppDrawContext, 0);            // shared, up
	AppDrawBox2dContextCreate *def  = ecs_field(it, AppDrawBox2dContextCreate, 1); // self
	for (int i = 0; i < it->count; ++i, ++def) {
		b2DebugDraw d = {0};
		b2DebugDraw_init(&d, draw->draw);
		ecs_set(it->world, it->entities[i], AppDrawBox2dContext, {d});
	}
	ecs_log_set_level(-1);
}

static void b2WorldId_Draw(ecs_iter_t *it)
{
	b2WorldId           *w = ecs_field(it, b2WorldId, 0);
	AppDrawBox2dContext *d = ecs_field(it, AppDrawBox2dContext, 1);
	for (int i = 0; i < it->count; ++i, ++w) {
		b2World_Draw(w[0], &d->debugDraw);
	}
}

void AppDrawBox2dImport(ecs_world_t *world)
{
	ECS_MODULE(world, AppDrawBox2d);
	ecs_set_name_prefix(world, "AppDrawBox2d");

	ECS_IMPORT(world, EgPhysics);
	ECS_IMPORT(world, EgSpatials);
	ECS_IMPORT(world, EgCameras);
	ECS_IMPORT(world, AppDraw);

	ECS_COMPONENT_DEFINE(world, AppDrawBox2dContextCreate);
	ECS_COMPONENT_DEFINE(world, AppDrawBox2dContext);

	ecs_system(world,
	{.entity  = ecs_entity(world, {.name = "AppDrawBox2dContext_Create", .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
	.callback = AppDrawBox2dContext_Create,
	.query.terms =
	{
	{.id = ecs_id(AppDrawContext), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(AppDrawBox2dContextCreate), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(AppDrawBox2dContext), .oper = EcsNot}, // Adds this
	}});

	ecs_system(world,
	{.entity  = ecs_entity(world, {.name = "b2WorldId_Draw", .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
	.callback = b2WorldId_Draw,
	.query.terms =
	{
	{.id = ecs_id(b2WorldId), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(AppDrawBox2dContext), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
	}});
}
