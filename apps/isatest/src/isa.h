#pragma once
#include <flecs.h>
#include <stdio.h>

typedef enum IsaOp {
	IsaOpCreateStack,
	IsaOpWrite,
} IsaOp;

typedef struct {
	ecs_vec_t    vec;
	ecs_entity_t type;
} IsaStack;

typedef struct {
	int32_t counter;
	FILE   *file;
} IsaTextStream;

extern ECS_COMPONENT_DECLARE(IsaStack);
extern ECS_COMPONENT_DECLARE(IsaTextStream);

/** Parses `json` as a value of `stack->type` and appends it to `stack->vec`. */
bool IsaStack_push(
	ecs_world_t *world,
	IsaStack    *stack,
	const char  *json);

/** Runs an isa script (see p1.isa0) of CREATE_STACK/WRITE/PRINT instructions. */
bool IsaRun(
	ecs_world_t *world,
	const char  *script);

/** Prints every IsaStack entity in the world. */
void IsaStack_print_all(ecs_world_t *world);

void IsaImport(ecs_world_t *world);
