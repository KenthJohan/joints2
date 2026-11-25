#pragma once

#include <flecs.h>
#include <EgDraws.h>

typedef struct {
	int32_t order;
} AppFramePass;

typedef struct {
	float r, g, b, a;
} AppFrameClear;

typedef enum {
	AppFrameSetViewport,
	AppFrameClearColor,
	AppFrameDrawLayers
} AppFrameOp;

typedef struct {
	AppFrameOp op;
	union {
		struct { int32_t x, y, width, height; } viewport;
		AppFrameClear clear;
		struct {
			ecs_entity_t pass;
			float matrix[16];
			int32_t layers_index;
		} draw;
	} args;
} AppFrameInstruction;

typedef struct {
	ecs_entity_t window;
	uint64_t frame;
	void *native_window;
	void *gl_context;
	int32_t width, height;
	AppFrameInstruction *instructions;
	int32_t instruction_count;
	EgDraws_List **layers;
	int32_t layer_count;
} AppFramePlan;

typedef enum {
	AppFrameInvalid,
	AppFrameSkipped,
	AppFrameReady
} AppFrameResult;

extern ECS_COMPONENT_DECLARE(AppFramePass);
extern ECS_COMPONENT_DECLARE(AppFrameClear);
extern ECS_TAG_DECLARE(AppFrameCamera);
extern ECS_TAG_DECLARE(AppFrameSource);

void AppFramePlanImport(ecs_world_t *world);
void AppFramePlanFini(AppFramePlan *plan);
AppFrameResult AppFramePlanCompile(ecs_world_t *world, ecs_entity_t window,
	uint64_t frame, AppFramePlan *plan);
