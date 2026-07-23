#pragma once
#include <box2d/box2d.h>
#include <flecs.h>

typedef struct {
	int dummy;
} AppDrawBox2dContextCreate;

typedef struct {
	b2DebugDraw debugDraw;
} AppDrawBox2dContext;

extern ECS_COMPONENT_DECLARE(AppDrawBox2dContextCreate);
extern ECS_COMPONENT_DECLARE(AppDrawBox2dContext);

void AppDrawBox2dImport(ecs_world_t *world);
