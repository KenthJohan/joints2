#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <cglm/cglm.h>
#include <flecs.h>
#include <box2d/box2d.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <draw.h>
#include <EgSpatials.h>

#include "canvas.h"
#include "fs.h"
#include "b2DebugDraw_init.h"
#include "b2.h"

typedef struct SampleContext {
	ecs_world_t *world;
	GLFWwindow  *window;
	Canvas       canvas;
	b2DebugDraw  debugDraw;
	float timeStep;
	int   subStepCount;
} SampleContext;

static bool BuildDrawCreateInfo(DrawCreateInfo *createInfo)
{
	*createInfo = (DrawCreateInfo){0};

	createInfo->shaders[DRAW_SHADER_BACKGROUND_VERTEX]      = fs_read_allocated("data/background.vs");
	createInfo->shaders[DRAW_SHADER_BACKGROUND_FRAGMENT]    = fs_read_allocated("data/background.fs");
	createInfo->shaders[DRAW_SHADER_POINT_VERTEX]           = fs_read_allocated("data/point.vs");
	createInfo->shaders[DRAW_SHADER_POINT_FRAGMENT]         = fs_read_allocated("data/point.fs");
	createInfo->shaders[DRAW_SHADER_LINE_VERTEX]            = fs_read_allocated("data/line.vs");
	createInfo->shaders[DRAW_SHADER_LINE_FRAGMENT]          = fs_read_allocated("data/line.fs");
	createInfo->shaders[DRAW_SHADER_CIRCLE_VERTEX]          = fs_read_allocated("data/circle.vs");
	createInfo->shaders[DRAW_SHADER_CIRCLE_FRAGMENT]        = fs_read_allocated("data/circle.fs");
	createInfo->shaders[DRAW_SHADER_SOLID_CIRCLE_VERTEX]    = fs_read_allocated("data/solid_circle.vs");
	createInfo->shaders[DRAW_SHADER_SOLID_CIRCLE_FRAGMENT]  = fs_read_allocated("data/solid_circle.fs");
	createInfo->shaders[DRAW_SHADER_SOLID_CAPSULE_VERTEX]   = fs_read_allocated("data/solid_capsule.vs");
	createInfo->shaders[DRAW_SHADER_SOLID_CAPSULE_FRAGMENT] = fs_read_allocated("data/solid_capsule.fs");
	createInfo->shaders[DRAW_SHADER_SOLID_POLYGON_VERTEX]   = fs_read_allocated("data/solid_polygon.vs");
	createInfo->shaders[DRAW_SHADER_SOLID_POLYGON_FRAGMENT] = fs_read_allocated("data/solid_polygon.fs");
	createInfo->shaders[DRAW_SHADER_TEXT_VERTEX]            = fs_read_allocated("data/text.vs");
	createInfo->shaders[DRAW_SHADER_TEXT_FRAGMENT]          = fs_read_allocated("data/text.fs");

	for (int i = 0; i < DRAW_SHADER_COUNT; ++i) {
		if (createInfo->shaders[i] == NULL) {
			fprintf(stderr, "Failed to load one or more shader files from apps/app1/data or data\n");
			return false;
		}
	}

	return true;
}

static void FreeDrawCreateInfo(DrawCreateInfo *createInfo)
{
	for (int i = 0; i < DRAW_SHADER_COUNT; ++i) {
		free((void *)createInfo->shaders[i]);
	}
	*createInfo = (DrawCreateInfo){0};
}

void glfwErrorCallback(int error, const char *description)
{
	fprintf(stderr, "GLFW error occurred. Code: %d. Description: %s\n", error, description);
}





static SampleContext s_context;

static void System_EgB2World_Step(ecs_iter_t *it)
{
	EgB2World     *b2world = ecs_field(it, EgB2World, 0); // self
	SampleContext *ctx     = it->ctx;
	for (int i = 0; i < it->count; ++i) {
		b2World_Step(b2world[i].id, ctx->timeStep, ctx->subStepCount);
	}
}

static void System_EgB2World_Draw(ecs_iter_t *it)
{
	SampleContext *ctx     = it->ctx;
	EgB2World     *b2world = ecs_field(it, EgB2World, 0); // self
	for (int i = 0; i < it->count; ++i) {
		b2World_Draw(b2world[i].id, &ctx->debugDraw);
	}
}

int main(int argc, char *argv[])
{
	ecs_world_t *world = ecs_init();

	ECS_IMPORT(world, EgSpatials);
	ECS_IMPORT(world, EgB2);

	s_context.canvas.camera        = GetDefaultCamera();
	s_context.canvas.camera.center = (b2Pos){0.0f, 0.0f};
	s_context.canvas.camera.zoom   = 12.0f;
	s_context.timeStep             = 1.0f / 60.0f;
	s_context.subStepCount         = 4;

	ecs_entity_t e_b2world = ecs_new(world);
	ecs_set(world, e_b2world, EgB2WorldDef, {0.0f, -10.0f});

	ecs_entity_t e_ground = ecs_new(world);
	ecs_add_pair(world, e_ground, EcsChildOf, e_b2world);
	ecs_set(world, e_ground, Position2, {0.0f, -10.0f});
	ecs_set(world, e_ground, EgB2BodyDef, {b2_staticBody});
	ecs_set(world, e_ground, EgB2Box, {50.0f, 10.0f});

	ecs_entity_t e_box = ecs_new(world);
	ecs_add_pair(world, e_box, EcsChildOf, e_b2world);
	ecs_set(world, e_box, Position2, {0.0f, 4.0f});
	ecs_set(world, e_box, EgB2BodyDef, {b2_dynamicBody});
	ecs_set(world, e_box, EgB2Box, {1.0f, 1.0f, 1.0f, 0.3f});

	ecs_system(world,
	{.entity  = ecs_entity(world, {.name = "System_EgB2World_Step", .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
	.callback = System_EgB2World_Step,
	.ctx      = &s_context,
	.query.terms =
	{
	{.id = ecs_id(EgB2World), .src.id = EcsSelf, .inout = EcsIn},
	}});

	ecs_system(world,
	{.entity  = ecs_entity(world, {.name = "System_EgB2World_Draw", .add = ecs_ids(ecs_dependson(EcsOnUpdate))}),
	.callback = System_EgB2World_Draw,
	.ctx      = &s_context,
	.query.terms =
	{
	{.id = ecs_id(EgB2World), .src.id = EcsSelf, .inout = EcsIn},
	}});

	/*
	b2WorldDef worldDef = b2DefaultWorldDef();
	worldDef.gravity    = (b2Vec2){0.0f, -10.0f};
	s_context.m_worldId = b2CreateWorld(&worldDef);
	test1_create_world(world, s_context.m_worldId);
	*/

	glfwSetErrorCallback(glfwErrorCallback);

	if (glfwInit() == 0) {
		fprintf(stderr, "Failed to initialize GLFW\n");
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// MSAA
	glfwWindowHint(GLFW_SAMPLES, 4);

	s_context.window = glfwCreateWindow(s_context.canvas.camera.width, s_context.canvas.camera.height, "App1", NULL, NULL);
	if (s_context.window == NULL) {
		fprintf(stderr, "Failed to open GLFW window.\n");
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(s_context.window);

	// Load OpenGL functions using glad
	if (!gladLoadGL()) {
		fprintf(stderr, "Failed to initialize glad\n");
		glfwTerminate();
		return -1;
	}

	DrawCreateInfo drawCreateInfo;
	if (!BuildDrawCreateInfo(&drawCreateInfo)) {
		glfwTerminate();
		return -1;
	}

	s_context.canvas.draw = CreateDraw(&drawCreateInfo);
	b2DebugDraw_init(&s_context.debugDraw, &s_context.canvas);
	FreeDrawCreateInfo(&drawCreateInfo);

	{
		const char *glVersionString   = (const char *)glGetString(GL_VERSION);
		const char *glslVersionString = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
		printf("OpenGL %s, GLSL %s\n", glVersionString, glslVersionString);
	}

#if 1
	ecs_set(world, EcsWorld, EcsRest, {.port = 0});
	printf("Remote: %s\n", "https://www.flecs.dev/explorer/?page=rest&host=localhost");
#endif

	while (!glfwWindowShouldClose(s_context.window)) {
		int width, height;
		glfwGetWindowSize(s_context.window, &width, &height);
		s_context.canvas.camera.width  = width;
		s_context.canvas.camera.height = height;

		int bufferWidth, bufferHeight;
		glfwGetFramebufferSize(s_context.window, &bufferWidth, &bufferHeight);
		glViewport(0, 0, bufferWidth, bufferHeight);
		glClearColor(0.07f, 0.07f, 0.09f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		SetDrawOrigin(s_context.canvas.draw, s_context.canvas.camera.center);
		ecs_progress(world, 0.0f);

		FlushDraw(s_context.canvas.draw, &s_context.canvas.camera);
		glfwSwapBuffers(s_context.window);
		glfwPollEvents();
	}
	glfwTerminate();

	ecs_fini(world);

	return 0;
}
