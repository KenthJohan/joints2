#include "b2.h"
#include <EgSpatials.h>
#include <EgCameras.h>
#include "AppDraw.h"
#include "b2DebugDraw_init.h"
#include <ecsx.h>

ECS_COMPONENT_DECLARE(EgB2World);
ECS_COMPONENT_DECLARE(EgB2WorldDef);
ECS_COMPONENT_DECLARE(EgB2Body);
ECS_COMPONENT_DECLARE(EgB2BodyDef);
ECS_COMPONENT_DECLARE(EgB2Box);
ECS_COMPONENT_DECLARE(EgB2DebugDrawDef);
ECS_COMPONENT_DECLARE(EgB2DebugDraw);
ECS_COMPONENT_DECLARE(EgB2OverlapChecking);
ECS_TAG_DECLARE(EgB2TargetTransform);

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
		b2BodyDef body_def   = b2DefaultBodyDef();
		body_def.type        = def->type;
		body_def.position    = (b2Pos){pos->x, pos->y};
		body_def.userData    = (void *)(uintptr_t)it->entities[i]; // Use the ECS entity as user data
		b2BodyId   body_id   = b2CreateBody(bw->id, &body_def);
		b2Polygon  poly      = b2MakeBox(box->half_width, box->half_height);
		b2ShapeDef shape_def = b2DefaultShapeDef();
		if (body_def.type == b2_dynamicBody) {
			if (box->density <= 0.0f) {
				ecs_warn("EgB2Box density is %.3f for entity %llu (zero/negative values are allowed but can disable gravity response on dynamic bodies)", box->density, (unsigned long long)it->entities[i]);
			}
			if (box->friction <= 0.0f) {
				ecs_warn("EgB2Box friction is %.3f for entity %llu (zero/negative values are allowed)", box->friction, (unsigned long long)it->entities[i]);
			}
		}
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

static void EgB2DebugDraw_Create(ecs_iter_t *it)
{
	ecs_log_set_level(0);
	AppDrawContext   *draw = ecs_field(it, AppDrawContext, 0);   // shared, up
	EgB2DebugDrawDef *def  = ecs_field(it, EgB2DebugDrawDef, 1); // self
	for (int i = 0; i < it->count; ++i, ++def) {
		b2DebugDraw d = {0};
		b2DebugDraw_init(&d, draw->draw);
		ecs_set(it->world, it->entities[i], EgB2DebugDraw, {d});
	}
	ecs_log_set_level(-1);
}

static void EgB2World_Step(ecs_iter_t *it)
{
	float      timeStep     = 1.0f / 60.0f;
	int        subStepCount = 4;
	EgB2World *w            = ecs_field(it, EgB2World, 0);
	for (int i = 0; i < it->count; ++i, ++w) {
		b2World_Step(w->id, timeStep, subStepCount);
	}
}

static void EgB2World_Draw(ecs_iter_t *it)
{
	EgB2World     *w = ecs_field(it, EgB2World, 0);
	EgB2DebugDraw *d = ecs_field(it, EgB2DebugDraw, 1);
	for (int i = 0; i < it->count; ++i, ++w) {
		b2World_Draw(w->id, &d->debugDraw);
	}
}

static void EgB2Body_TargetTransform(ecs_iter_t *it)
{
	float timeStep = 1.0f / 60.0f;
	ecs_log_set_level(0);
	EgB2Body  *body = ecs_field(it, EgB2Body, 0);  // self
	Position2 *p    = ecs_field(it, Position2, 1); // shared, up
	for (int i = 0; i < it->count; ++i, ++body) {
		b2Vec2 targetPosition = {p->x, p->y};
		// printf("Setting target transform for body %llu to position (%.3f, %.3f)\n", (unsigned long long)it->entities[i], targetPosition.x, targetPosition.y);
		b2Body_SetTargetTransform(body->id, (b2WorldTransform){targetPosition, b2Rot_identity}, timeStep, true);
	}
	ecs_log_set_level(-1);
}

typedef struct
{
	b2Pos    point;
	b2BodyId bodyId;
} QueryContext;

bool QueryCallback(b2ShapeId shapeId, void *context)
{
	QueryContext *queryContext = (QueryContext *)context;

	b2BodyId   bodyId   = b2Shape_GetBody(shapeId);
	b2BodyType bodyType = b2Body_GetType(bodyId);
	if (bodyType != b2_dynamicBody) {
		// continue query
		return true;
	}

	bool overlap = b2Shape_TestPoint(shapeId, queryContext->point);
	if (overlap) {
		// found shape
		queryContext->bodyId = bodyId;
		return false;
	}

	return true;
}

static void System_Overlap_Checking_Clear(ecs_iter_t *it)
{
	EgB2Body            *b = ecs_field_self(it, EgB2Body, 0);
	EgB2OverlapChecking *c = ecs_field_shared(it, EgB2OverlapChecking, 1);
	EgB2World           *w = ecs_field_shared(it, EgB2World, 2);

	(void)b;
	(void)w;
	for (int i = 0; i < it->count; ++i) {
		if (!ecs_is_valid(it->world, c->addtag)) {
			ecs_warn("addtag entity %jX is not valid", c->addtag);
			continue; // Skip if the addtag entity is not valid
		}
		ecs_remove_id(it->world, it->entities[i], c->addtag);
	}
}

static void System_Overlap_Checking_Update(ecs_iter_t *it)
{
	EgB2World           *w = ecs_field_self(it, EgB2World, 0);
	EgB2OverlapChecking *c = ecs_field_self(it, EgB2OverlapChecking, 1);
	Position2           *p = ecs_field_shared(it, Position2, 2);
	for (int i = 0; i < it->count; ++i, ++w) {
		b2Vec2       d            = {0.001f, 0.001f};
		b2AABB       box          = {b2Neg(d), d};
		QueryContext queryContext = {{p->x, p->y}, b2_nullBodyId};
		b2World_OverlapAABB(w->id, queryContext.point, box, b2DefaultQueryFilter(), QueryCallback, &queryContext);
		if (!B2_IS_NON_NULL(queryContext.bodyId)) {
			continue; // No overlap found, continue to next entity
		}
		// printf("Overlap found at position (%.3f, %.3f) with body ID %d\n", queryContext.point.x, queryContext.point.y, queryContext.bodyId.index1);
		if (!ecs_is_valid(it->world, c->addtag)) {
			ecs_warn("addtag entity %jX is not valid", c->addtag);
			continue; // Skip if the addtag entity is not valid
		}
		ecs_entity_t body_entity = (ecs_entity_t)(uintptr_t)b2Body_GetUserData(queryContext.bodyId);
		if (!ecs_is_valid(it->world, body_entity)) {
			ecs_warn("Entity %jX is not valid", body_entity);
			continue; // Skip invalid entities
		}
		// printf("name %s\n", ecs_get_name(it->world, body_entity));
		ecs_add_id(it->world, body_entity, c->addtag);
	}
}

void EgB2Import(ecs_world_t *world)
{
	ECS_MODULE(world, EgB2);
	ecs_set_name_prefix(world, "EgB2");

	ECS_IMPORT(world, EgSpatials);
	ECS_IMPORT(world, EgCameras);
	ECS_IMPORT(world, AppDraw);

	ECS_COMPONENT_DEFINE(world, EgB2World);
	ECS_COMPONENT_DEFINE(world, EgB2WorldDef);
	ECS_COMPONENT_DEFINE(world, EgB2Body);
	ECS_COMPONENT_DEFINE(world, EgB2BodyDef);
	ECS_COMPONENT_DEFINE(world, EgB2Box);
	ECS_COMPONENT_DEFINE(world, EgB2DebugDrawDef);
	ECS_COMPONENT_DEFINE(world, EgB2DebugDraw);
	ECS_COMPONENT_DEFINE(world, EgB2OverlapChecking);
	ECS_TAG_DEFINE(world, EgB2TargetTransform);
	ecs_add_id(world, EgB2TargetTransform, EcsTraversable);
	ecs_add_id(world, ecs_id(EgB2OverlapChecking), EcsTraversable);

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

	ecs_system(world,
	{.entity  = ecs_entity(world, {.name = "EgB2DebugDraw_Create", .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
	.callback = EgB2DebugDraw_Create,
	.query.terms =
	{
	{.id = ecs_id(AppDrawContext), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(EgB2DebugDrawDef), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgB2DebugDraw), .oper = EcsNot}, // Adds this
	}});

	ecs_system(world,
	{.entity  = ecs_entity(world, {.name = "EgB2World_Step", .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
	.callback = EgB2World_Step,
	.query.terms =
	{
	{.id = ecs_id(EgB2World), .src.id = EcsSelf, .inout = EcsIn},
	}});

	ecs_system(world,
	{.entity  = ecs_entity(world, {.name = "EgB2World_Draw", .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
	.callback = EgB2World_Draw,
	.query.terms =
	{
	{.id = ecs_id(EgB2World), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgB2DebugDraw), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
	}});

	ecs_system(world,
	{.entity  = ecs_entity(world, {.name = "EgB2Body_TargetTransform", .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
	.callback = EgB2Body_TargetTransform,
	.query.terms =
	{
	{.id = ecs_id(EgB2Body), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(Position2), .trav = EgB2TargetTransform, .src.id = EcsUp, .inout = EcsIn},
	}});

	ecs_system(world,
	{.entity  = ecs_entity(world, {.name = "System_Overlap_Checking_Clear", .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
	.callback = System_Overlap_Checking_Clear,
	.query.terms =
	{
	{.id = ecs_id(EgB2Body), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_pair(ecs_id(EgB2OverlapChecking), EcsWildcard), .trav = EcsChildOf, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(EgB2World), .trav = EcsChildOf, .src.id = EcsUp, .inout = EcsIn},
	}});

	ecs_system(world,
	{.entity  = ecs_entity(world, {.name = "System_Overlap_Checking_Update", .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
	.callback = System_Overlap_Checking_Update,
	.query.terms =
	{
	{.id = ecs_id(EgB2World), .inout = EcsIn},
	{.id = ecs_pair(ecs_id(EgB2OverlapChecking), EcsWildcard), .inout = EcsIn},
	{.id = ecs_id(Position2), .trav = ecs_id(EgB2OverlapChecking), .src.id = EcsUp, .inout = EcsIn},
	}});

	ecs_observer(world,
	{.query   = {.terms = {{.id = ecs_id(EgB2World)}}},
	.events   = {EcsOnRemove},
	.callback = EgB2World_Destroy});

	ecs_struct_init(world,
	&(ecs_struct_desc_t){
	.entity  = ecs_id(EgB2OverlapChecking),
	.members = {
	{.name = "addtag", .type = ecs_id(ecs_entity_t)},
	}});

	ecs_struct_init(world,
	&(ecs_struct_desc_t){
	.entity  = ecs_id(EgB2WorldDef),
	.members = {
	{.name = "gravity_x", .type = ecs_id(ecs_f32_t)},
	{.name = "gravity_y", .type = ecs_id(ecs_f32_t)},
	}});

	ecs_struct_init(world,
	&(ecs_struct_desc_t){
	.entity  = ecs_id(EgB2BodyDef),
	.members = {
	{.name = "type", .type = ecs_id(ecs_i32_t)},
	}});

	ecs_struct_init(world,
	&(ecs_struct_desc_t){
	.entity  = ecs_id(EgB2Box),
	.members = {
	{.name = "half_width", .type = ecs_id(ecs_f32_t)},
	{.name = "half_height", .type = ecs_id(ecs_f32_t)},
	{.name = "density", .type = ecs_id(ecs_f32_t)},
	{.name = "friction", .type = ecs_id(ecs_f32_t)},
	}});
}
