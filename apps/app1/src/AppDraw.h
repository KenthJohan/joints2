#pragma once
#include <box2d/box2d.h>
#include <flecs.h>
#include <draw.h>

typedef struct {
	Draw *draw;
} AppDrawContext;

typedef struct {
	int dummy;
} AppDrawContextCreate;

extern ECS_COMPONENT_DECLARE(AppDrawContext);
extern ECS_COMPONENT_DECLARE(AppDrawContextCreate);

void EgB2Import(ecs_world_t *world);
