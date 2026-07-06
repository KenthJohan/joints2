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

#include "canvas.h"
#include "fs.h"
#include "b2DebugDraw_init.h"

typedef struct SampleContext {
	GLFWwindow *window;
	Canvas      canvas;
	b2DebugDraw debugDraw;
	b2JointId   m_mouseJointId;
	b2BodyId    m_mouseBodyId;

	b2WorldId m_worldId;

	b2Pos m_mousePoint;

	float m_mouseForceScale;
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

typedef struct
{
	b2Pos    point;
	b2BodyId bodyId;
} QueryContext;

bool QueryCallback(b2ShapeId shapeId, void *context)
{
	QueryContext *queryContext = (QueryContext *)(context);

	b2BodyId   bodyId   = b2Shape_GetBody(shapeId);
	b2BodyType bodyType = b2Body_GetType(bodyId);
	if (bodyType != b2_dynamicBody) {
		// continue query
		return true;
	}

	bool overlap = b2Shape_TestPoint(shapeId, queryContext->point);
	if (overlap) {
		// found shape
		queryContext->bodyId = bodyId;
		return false;
	}

	return true;
}

static SampleContext s_context;
static bool          s_rightMouseDown = false;
static b2Pos         s_clickPointWS   = b2Pos_zero;

void test1_create_world(b2WorldId worldId)
{

	b2BodyDef groundBodyDef = b2DefaultBodyDef();
	groundBodyDef.position  = (b2Vec2){0.0f, -10.0f};

	b2BodyId groundId = b2CreateBody(worldId, &groundBodyDef);

	b2Polygon groundBox = b2MakeBox(50.0f, 10.0f);

	b2ShapeDef groundShapeDef = b2DefaultShapeDef();
	b2CreatePolygonShape(groundId, &groundShapeDef, &groundBox);

	b2BodyDef bodyDef = b2DefaultBodyDef();
	bodyDef.type      = b2_dynamicBody;
	bodyDef.position  = (b2Vec2){0.0f, 4.0f};
	b2BodyId bodyId   = b2CreateBody(worldId, &bodyDef);

	b2Polygon dynamicBox = b2MakeBox(1.0f, 1.0f);

	b2ShapeDef shapeDef        = b2DefaultShapeDef();
	shapeDef.density           = 1.0f;
	shapeDef.material.friction = 0.3f;

	b2CreatePolygonShape(bodyId, &shapeDef, &dynamicBox);

	/*
	float timeStep = 1.0f / 60.0f;

	int subStepCount = 4;

	for (int i = 0; i < 90; ++i) {
	    b2World_Step(worldId, timeStep, subStepCount);
	    b2Vec2 position = b2Body_GetPosition(bodyId);
	    b2Rot  rotation = b2Body_GetRotation(bodyId);
	    printf("%4.2f %4.2f %4.2f\n", position.x, position.y, b2Rot_GetAngle(rotation));
	}
	*/
}

void MouseDown(SampleContext *ctx, b2Pos p, int button, int mod)
{
	if (B2_IS_NON_NULL(ctx->m_mouseJointId)) {
		return;
	}

	if (button == GLFW_MOUSE_BUTTON_1) {
		// A tiny box around the click point, exact at any distance with the click as the origin
		b2Vec2 d   = {0.001f, 0.001f};
		b2AABB box = {b2Neg(d), d};

		ctx->m_mousePoint = p;

		// Query the world for overlapping shapes.
		QueryContext queryContext = {p, b2_nullBodyId};
		b2World_OverlapAABB(ctx->m_worldId, p, box, b2DefaultQueryFilter(), QueryCallback, &queryContext);

		if (B2_IS_NON_NULL(queryContext.bodyId)) {
			b2BodyDef bodyDef  = b2DefaultBodyDef();
			bodyDef.type       = b2_kinematicBody;
			bodyDef.position   = ctx->m_mousePoint;
			ctx->m_mouseBodyId = b2CreateBody(ctx->m_worldId, &bodyDef);

			b2MotorJointDef jointDef    = b2DefaultMotorJointDef();
			jointDef.base.bodyIdA       = ctx->m_mouseBodyId;
			jointDef.base.bodyIdB       = queryContext.bodyId;
			jointDef.base.localFrameB.p = b2Body_GetLocalPoint(queryContext.bodyId, p);
			jointDef.linearHertz        = 7.5f;
			jointDef.linearDampingRatio = 1.0f;

			b2MassData massData = b2Body_GetMassData(queryContext.bodyId);
			float      g        = b2Length(b2World_GetGravity(ctx->m_worldId));
			float      mg       = massData.mass * g;

			jointDef.maxSpringForce = ctx->m_mouseForceScale * mg;

			if (massData.mass > 0.0f) {
				// This acts like angular friction
				float lever                = sqrtf(massData.rotationalInertia / massData.mass);
				jointDef.maxVelocityTorque = 0.25f * lever * mg;
			}

			ctx->m_mouseJointId = b2CreateMotorJoint(ctx->m_worldId, &jointDef);
		}
	}
}

void MouseUp(SampleContext *ctx, b2Pos p, int button)
{
	if (B2_IS_NON_NULL(ctx->m_mouseJointId) && button == GLFW_MOUSE_BUTTON_1) {
		b2DestroyJoint(ctx->m_mouseJointId, true);
		ctx->m_mouseJointId = b2_nullJointId;

		b2DestroyBody(ctx->m_mouseBodyId);
		ctx->m_mouseBodyId = b2_nullBodyId;
	}
}

void MouseMove(SampleContext *ctx, b2Pos p)
{
	if (b2Joint_IsValid(ctx->m_mouseJointId) == false) {
		// The world or attached body was destroyed.
		ctx->m_mouseJointId = b2_nullJointId;
	}

	ctx->m_mousePoint = p;

	/*
	if (B2_IS_NON_NULL(ctx->m_mouseBodyId) && b2Body_IsValid(ctx->m_mouseBodyId)) {
	    b2Body_SetTransform(ctx->m_mouseBodyId, p, b2Rot_identity);
	}
	*/
}

static void MouseMotionCallback(GLFWwindow *window, double xd, double yd)
{
	b2Vec2 ps = {xd, yd};
	b2Pos  pw = ConvertScreenToWorld(&s_context.canvas.camera, ps);
	MouseMove(&s_context, pw);

	if (s_rightMouseDown) {
		b2Vec2 diff = {pw.x - s_clickPointWS.x, pw.y - s_clickPointWS.y};
		s_context.canvas.camera.center.x -= diff.x;
		s_context.canvas.camera.center.y -= diff.y;
		s_clickPointWS = ConvertScreenToWorld(&s_context.canvas.camera, ps);
	}
}

static void MouseButtonCallback(GLFWwindow *window, int button, int action, int modifiers)
{
	double xd, yd;
	glfwGetCursorPos(window, &xd, &yd);
	b2Vec2 ps = {xd, yd};

	// Use the mouse to move things around.
	if (button == GLFW_MOUSE_BUTTON_1) {
		b2Pos pw = ConvertScreenToWorld(&s_context.canvas.camera, ps);
		if (action == GLFW_PRESS) {
			MouseDown(&s_context, pw, button, modifiers);
		}

		if (action == GLFW_RELEASE) {
			MouseUp(&s_context, pw, button);
		}
	} else if (button == GLFW_MOUSE_BUTTON_2) {
		if (action == GLFW_PRESS) {
			s_clickPointWS   = ConvertScreenToWorld(&s_context.canvas.camera, ps);
			s_rightMouseDown = true;
		}

		if (action == GLFW_RELEASE) {
			s_rightMouseDown = false;
		}
	}
}

int main(int argc, char *argv[])
{
	ecs_world_t *world = ecs_init();
	ecs_fini(world);

	s_context.canvas.camera            = GetDefaultCamera();
	s_context.canvas.camera.center     = (b2Pos){0.0f, 0.0f};
	s_context.canvas.camera.zoom       = 12.0f;
	s_context.m_mouseForceScale = 500.0f;

	b2WorldDef worldDef = b2DefaultWorldDef();
	worldDef.gravity    = (b2Vec2){0.0f, -10.0f};
	s_context.m_worldId = b2CreateWorld(&worldDef);
	test1_create_world(s_context.m_worldId);

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

	glfwSetMouseButtonCallback(s_context.window, MouseButtonCallback);
	glfwSetCursorPosCallback(s_context.window, MouseMotionCallback);

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

	float timeStep     = 1.0f / 60.0f;
	int   subStepCount = 4;

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

		if (B2_IS_NON_NULL(s_context.m_mouseJointId) && b2Joint_IsValid(s_context.m_mouseJointId) == false) {
			// The world or attached body was destroyed.
			s_context.m_mouseJointId = b2_nullJointId;

			if (B2_IS_NON_NULL(s_context.m_mouseBodyId)) {
				b2DestroyBody(s_context.m_mouseBodyId);
				s_context.m_mouseBodyId = b2_nullBodyId;
			}
		}

		if (B2_IS_NON_NULL(s_context.m_mouseBodyId) && timeStep > 0.0f) {
			bool wake = true;
			b2Body_SetTargetTransform(s_context.m_mouseBodyId, (b2WorldTransform){s_context.m_mousePoint, b2Rot_identity}, timeStep, wake);
		}

		b2World_Step(s_context.m_worldId, timeStep, subStepCount);

		SetDrawOrigin(s_context.canvas.draw, s_context.canvas.camera.center);
		b2World_Draw(s_context.m_worldId, &s_context.debugDraw);

		FlushDraw(s_context.canvas.draw, &s_context.canvas.camera);
		glfwSwapBuffers(s_context.window);
		glfwPollEvents();
	}
	glfwTerminate();

	b2DestroyWorld(s_context.m_worldId);

	return 0;
}
