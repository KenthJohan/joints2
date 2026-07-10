#include "b2.h"
#include <EgSpatials.h>

ECS_COMPONENT_DECLARE(EgB2World);
ECS_COMPONENT_DECLARE(EgB2WorldDef);
ECS_COMPONENT_DECLARE(EgB2Body);
ECS_COMPONENT_DECLARE(EgB2BodyDef);
ECS_COMPONENT_DECLARE(EgB2Box);

static void EgB2World_Create(ecs_iter_t *it)
{
	ecs_log_set_level(0);
	EgB2WorldDef *def = ecs_field(it, EgB2WorldDef, 0); // self
	for (int i = 0; i < it->count; ++i, ++def) {
		b2WorldDef worldDef = b2DefaultWorldDef();
		worldDef.gravity    = (b2Vec2){def->gravity_x, def->gravity_y};
		b2WorldId bw        = b2CreateWorld(&worldDef);
		ecs_set(it->world, it->entities[i], EgB2World, {bw});
	}
	ecs_log_set_level(-1);
}

static void EgB2Body_Create(ecs_iter_t *it)
{
	ecs_log_set_level(0);
	EgB2World   *bw  = ecs_field(it, EgB2World, 0);   // shared,up
	Position2   *pos = ecs_field(it, Position2, 1);   // self
	EgB2BodyDef *def = ecs_field(it, EgB2BodyDef, 2); // self
	EgB2Box     *box = ecs_field(it, EgB2Box, 3);     // self
	for (int i = 0; i < it->count; ++i, ++def, ++pos, ++box) {
		b2BodyDef body_def          = b2DefaultBodyDef();
		body_def.type               = def->type;
		body_def.position           = (b2Pos){pos->x, pos->y};
		body_def.userData           = (void *)(uintptr_t)it->entities[i]; // Use the ECS entity as user data
		b2BodyId   body_id          = b2CreateBody(bw->id, &body_def);
		b2Polygon  poly             = b2MakeBox(box->half_width, box->half_height);
		b2ShapeDef shape_def        = b2DefaultShapeDef();
		shape_def.density           = box->density;
		shape_def.material.friction = box->friction;
		b2ShapeId shape_id          = b2CreatePolygonShape(body_id, &shape_def, &poly);
		ecs_set(it->world, it->entities[i], EgB2Body, {body_id, shape_id});
	}
	ecs_log_set_level(-1);
}

void EgB2World_Destroy(ecs_iter_t *it)
{
	EgB2World *w = ecs_field(it, EgB2World, 0);
	for (int i = 0; i < it->count; ++i, ++w) {
		b2DestroyWorld(w->id);
	}
}

void EgB2Import(ecs_world_t *world)
{
	ECS_MODULE(world, EgB2);
	ecs_set_name_prefix(world, "EgB2");

	ECS_IMPORT(world, EgSpatials);

	ECS_COMPONENT_DEFINE(world, EgB2World);
	ECS_COMPONENT_DEFINE(world, EgB2WorldDef);
	ECS_COMPONENT_DEFINE(world, EgB2Body);
	ECS_COMPONENT_DEFINE(world, EgB2BodyDef);
	ECS_COMPONENT_DEFINE(world, EgB2Box);

	ecs_system(world,
	{.entity  = ecs_entity(world, {.name = "EgB2World_Create", .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
	.callback = EgB2World_Create,
	.query.terms =
	{
	{.id = ecs_id(EgB2WorldDef), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgB2World), .oper = EcsNot}, // Adds this
	}});

	ecs_system(world,
	{.entity  = ecs_entity(world, {.name = "EgB2Body_Create", .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
	.callback = EgB2Body_Create,
	.query.terms =
	{
	{.id = ecs_id(EgB2World), .trav = EcsChildOf, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(Position2), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgB2BodyDef), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgB2Box), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgB2Body), .oper = EcsNot}, // Adds this
	}});

	ecs_observer(world,
	{.query   = {.terms = {{.id = ecs_id(EgB2World)}}},
	.events   = {EcsOnRemove},
	.callback = EgB2World_Destroy});

	ecs_struct_init(world,
	&(ecs_struct_desc_t){
	.entity  = ecs_id(EgB2WorldDef),
	.members = {
	{.name = "gravity_x", .type = ecs_id(ecs_f32_t)},
	{.name = "gravity_y", .type = ecs_id(ecs_f32_t)},
	}});
}
