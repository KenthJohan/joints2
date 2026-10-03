#pragma once
#include <flecs.h>
#include <egg.h>

typedef struct {
	egg_render_t *render;
	egg_draw_t   *draw;
	float         pixelScale;
} AppDrawContext;

enum {
	APP_DRAW_Z_SHAPES = 0,
	APP_DRAW_Z_DEBUG  = 1,
	APP_DRAW_Z_TEXT   = 2,
};

typedef struct {
	ecs_i32_t dummy;
} AppDrawContextCreate;

typedef struct
{
	ecs_id_t term;
	ecs_id_t draw_e;
	uint32_t color;
} AppDrawNameAtPositionRule;

extern ECS_COMPONENT_DECLARE(AppDrawContext);
extern ECS_COMPONENT_DECLARE(AppDrawContextCreate);
extern ECS_COMPONENT_DECLARE(AppDrawNameAtPositionRule);

void AppDrawImport(ecs_world_t *world);
