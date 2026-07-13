#pragma once
#include <flecs.h>
#include <draw.h>

typedef struct {
	Draw *draw;
} AppDrawContext;

typedef struct {
	ecs_i32_t dummy;
} AppDrawContextCreate;

extern ECS_COMPONENT_DECLARE(AppDrawContext);
extern ECS_COMPONENT_DECLARE(AppDrawContextCreate);

void AppDrawImport(ecs_world_t *world);
