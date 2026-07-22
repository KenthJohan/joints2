#pragma once
#include <flecs.h>
#include <draw.h>

typedef struct {
	draw_t *draw;
} AppDrawContext;

typedef struct {
	ecs_i32_t dummy;
} AppDrawContextCreate;

typedef struct
{
	ecs_id_t term;
	ecs_id_t draw_e;
} AppDrawPrintPositionalBinding;

extern ECS_COMPONENT_DECLARE(AppDrawContext);
extern ECS_COMPONENT_DECLARE(AppDrawContextCreate);
extern ECS_COMPONENT_DECLARE(AppDrawPrintPositionalBinding);

void AppDrawImport(ecs_world_t *world);
