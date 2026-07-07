#pragma once
#include <box2d/box2d.h>
#include <flecs.h>

b2BodyId b2_create_body(ecs_world_t* world, b2WorldId worldId, b2BodyType type, b2Pos position, b2Rot rotation);

typedef struct {
	float gravity_x;
    float gravity_y;
} EgB2WorldDef;

typedef struct {
    int type; // b2BodyType
} EgB2BodyDef;

typedef struct {
	b2WorldId id;
} EgB2World;

typedef struct {
	b2BodyId id;
    b2ShapeId shapeId; // Optional: Store the shape ID associated with this body
} EgB2Body;

typedef struct {
    float half_width;
    float half_height;
} EgB2Box;

extern ECS_COMPONENT_DECLARE(EgB2World);
extern ECS_COMPONENT_DECLARE(EgB2WorldDef);
extern ECS_COMPONENT_DECLARE(EgB2Body);
extern ECS_COMPONENT_DECLARE(EgB2BodyDef);
extern ECS_COMPONENT_DECLARE(EgB2Box);

void EgB2Import(ecs_world_t *world);
