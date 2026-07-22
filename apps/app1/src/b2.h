#pragma once
#include <box2d/box2d.h>
#include <flecs.h>

typedef struct {
	float gravity_x;
	float gravity_y;
} EgB2WorldDef;

typedef struct {
	ecs_i32_t type; // b2BodyType
} EgB2BodyDef;

typedef struct {
	b2WorldId id;
} EgB2World;

typedef struct {
	b2BodyId  body;
	b2ShapeId shape; // Optional: Store the shape ID associated with this body
} EgB2Body;

typedef struct {
	float half_width;
	float half_height;
	float density;
	float friction;
} EgB2Box;

typedef struct {
	int dummy;
} EgB2DebugDrawDef;

typedef struct {
	b2DebugDraw debugDraw;
} EgB2DebugDraw;

typedef struct {
	ecs_entity_t tag; ///< Adds this tag to the entity when an overlap is detected and removes it when no overlap is detected
} EgB2OverlapChecking;

extern ECS_COMPONENT_DECLARE(EgB2World);
extern ECS_COMPONENT_DECLARE(EgB2WorldDef);
extern ECS_COMPONENT_DECLARE(EgB2Body);
extern ECS_COMPONENT_DECLARE(EgB2BodyDef);
extern ECS_COMPONENT_DECLARE(EgB2Box);
extern ECS_COMPONENT_DECLARE(EgB2DebugDrawDef);
extern ECS_COMPONENT_DECLARE(EgB2DebugDraw);
extern ECS_COMPONENT_DECLARE(EgB2OverlapChecking);
extern ECS_TAG_DECLARE(EgB2TargetTransform);

void EgB2Import(ecs_world_t *world);
