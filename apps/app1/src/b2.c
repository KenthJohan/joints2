#include "b2.h"
#include <EgSpatials.h>

b2BodyId b2_create_body(ecs_world_t *world, b2WorldId worldId, b2BodyType type, b2Pos position, b2Rot rotation)
{
	b2BodyDef bodyDef = b2DefaultBodyDef();
	bodyDef.type      = type;
	bodyDef.position  = position;
	bodyDef.rotation  = rotation;
	bodyDef.userData  = ecs_new(world); // Create a new entity in the ECS world and assign it as user data
	b2BodyId bodyId   = b2CreateBody(worldId, &bodyDef);
	return bodyId;
}



ECS_COMPONENT_DECLARE(EgB2World);
ECS_COMPONENT_DECLARE(EgB2WorldDef);
ECS_COMPONENT_DECLARE(EgB2Body);
ECS_COMPONENT_DECLARE(EgB2BodyDef);
ECS_COMPONENT_DECLARE(EgB2Box);

static void EgB2World_Create(ecs_iter_t *it)
{
	ecs_log_set_level(0);
	EgB2WorldDef *def = ecs_field(it, EgB2WorldDef, 0); // self
	Position2    *pos = ecs_field(it, Position2, 1);    // self
	for (int i = 0; i < it->count; ++i, ++def, ++pos) {
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
	EgB2BodyDef *def = ecs_field(it, EgB2BodyDef, 1); // self
	Position2   *pos = ecs_field(it, Position2, 2);   // self
	EgB2Box     *size = ecs_field(it, EgB2Box, 3);   // self
	for (int i = 0; i < it->count; ++i, ++def, ++pos, ++size) {
		b2BodyDef bodyDef = b2DefaultBodyDef();
		bodyDef.type      = def->type;
		bodyDef.position  = (b2Pos){pos->x, pos->y};
		bodyDef.userData  = it->entities[i]; // Use the ECS entity as user data
		b2BodyId bodyId   = b2CreateBody(bw->id, &bodyDef);
        b2Polygon box = b2MakeBox(size->half_width, size->half_height);
        b2ShapeDef boxdef = b2DefaultShapeDef();
        b2ShapeId shapeId = b2CreatePolygonShape(bodyId, &boxdef, &box);
		ecs_set(it->world, it->entities[i], EgB2Body, {bodyId, shapeId});
	}
	ecs_log_set_level(-1);
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
	{.id = ecs_id(Position2), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgB2World), .oper = EcsNot}, // Adds this
	}});

	ecs_system(world,
	{.entity  = ecs_entity(world, {.name = "EgB2Body_Create", .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
	.callback = EgB2Body_Create,
	.query.terms =
	{
	{.id = ecs_id(EgB2WorldDef), .trav = EcsChildOf, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(Position2), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgB2BodyDef), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgB2Box), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgB2Body), .oper = EcsNot}, // Adds this
	}});

	ecs_struct_init(world,
	&(ecs_struct_desc_t){
	.entity  = ecs_id(EgB2WorldDef),
	.members = {
	{.name = "gravity_x", .type = ecs_id(ecs_f32_t)},
	{.name = "gravity_y", .type = ecs_id(ecs_f32_t)},
	}});
}
