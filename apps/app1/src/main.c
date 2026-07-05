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

typedef struct SampleContext {
	GLFWwindow *window;
	Draw       *draw;
	Camera      camera;
	b2DebugDraw debugDraw;
} SampleContext;

static char* ReadTextFile(const char* path)
{
	FILE* file = fopen(path, "rb");
	if (file == NULL) {
		return NULL;
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return NULL;
	}

	long size = ftell(file);
	if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return NULL;
	}

	char* text = (char*)malloc((size_t)size + 1);
	if (text == NULL) {
		fclose(file);
		return NULL;
	}

	if (fread(text, 1, (size_t)size, file) != (size_t)size) {
		fclose(file);
		free(text);
		return NULL;
	}

	text[size] = '\0';
	fclose(file);
	return text;
}

static char* LoadShaderText(const char* fileName)
{
	char path[256];
	snprintf(path, sizeof(path), "apps/app1/data/%s", fileName);
	char* text = ReadTextFile(path);
	if (text != NULL) {
		return text;
	}

	snprintf(path, sizeof(path), "data/%s", fileName);
	return ReadTextFile(path);
}

static bool BuildDrawCreateInfo(DrawCreateInfo* createInfo)
{
	*createInfo = (DrawCreateInfo){0};

	createInfo->backgroundVertexShader = LoadShaderText("background.vs");
	createInfo->backgroundFragmentShader = LoadShaderText("background.fs");
	createInfo->pointVertexShader = LoadShaderText("point.vs");
	createInfo->pointFragmentShader = LoadShaderText("point.fs");
	createInfo->lineVertexShader = LoadShaderText("line.vs");
	createInfo->lineFragmentShader = LoadShaderText("line.fs");
	createInfo->circleVertexShader = LoadShaderText("circle.vs");
	createInfo->circleFragmentShader = LoadShaderText("circle.fs");
	createInfo->solidCircleVertexShader = LoadShaderText("solid_circle.vs");
	createInfo->solidCircleFragmentShader = LoadShaderText("solid_circle.fs");
	createInfo->solidCapsuleVertexShader = LoadShaderText("solid_capsule.vs");
	createInfo->solidCapsuleFragmentShader = LoadShaderText("solid_capsule.fs");
	createInfo->solidPolygonVertexShader = LoadShaderText("solid_polygon.vs");
	createInfo->solidPolygonFragmentShader = LoadShaderText("solid_polygon.fs");

	if (createInfo->backgroundVertexShader == NULL || createInfo->backgroundFragmentShader == NULL ||
		createInfo->pointVertexShader == NULL || createInfo->pointFragmentShader == NULL ||
		createInfo->lineVertexShader == NULL || createInfo->lineFragmentShader == NULL ||
		createInfo->circleVertexShader == NULL || createInfo->circleFragmentShader == NULL ||
		createInfo->solidCircleVertexShader == NULL || createInfo->solidCircleFragmentShader == NULL ||
		createInfo->solidCapsuleVertexShader == NULL || createInfo->solidCapsuleFragmentShader == NULL ||
		createInfo->solidPolygonVertexShader == NULL || createInfo->solidPolygonFragmentShader == NULL) {
		fprintf(stderr, "Failed to load one or more shader files from apps/app1/data or data\n");
		return false;
	}

	return true;
}

static void FreeDrawCreateInfo(DrawCreateInfo* createInfo)
{
	free((void*)createInfo->backgroundVertexShader);
	free((void*)createInfo->backgroundFragmentShader);
	free((void*)createInfo->pointVertexShader);
	free((void*)createInfo->pointFragmentShader);
	free((void*)createInfo->lineVertexShader);
	free((void*)createInfo->lineFragmentShader);
	free((void*)createInfo->circleVertexShader);
	free((void*)createInfo->circleFragmentShader);
	free((void*)createInfo->solidCircleVertexShader);
	free((void*)createInfo->solidCircleFragmentShader);
	free((void*)createInfo->solidCapsuleVertexShader);
	free((void*)createInfo->solidCapsuleFragmentShader);
	free((void*)createInfo->solidPolygonVertexShader);
	free((void*)createInfo->solidPolygonFragmentShader);
	*createInfo = (DrawCreateInfo){0};
}

void glfwErrorCallback(int error, const char *description)
{
	fprintf(stderr, "GLFW error occurred. Code: %d. Description: %s\n", error, description);
}

void DrawPolygonFcn(b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, b2HexColor color, void *context)
{
	SampleContext *sampleContext = (SampleContext *)(context);
	DrawPolygon(sampleContext->draw, transform, vertices, vertexCount, color);
}

void DrawSolidPolygonFcn(b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, float radius, b2HexColor color, void *context)
{
	SampleContext *sampleContext = (SampleContext *)(context);
	DrawSolidPolygon(sampleContext->draw, transform, vertices, vertexCount, radius, color);
}

void DrawCircleFcn(b2Pos center, float radius, b2HexColor color, void *context)
{
	SampleContext *sampleContext = (SampleContext *)(context);
	DrawCircle(sampleContext->draw, center, radius, color);
}

void DrawSolidCircleFcn(b2WorldTransform transform, b2Vec2 center, float radius, b2HexColor color, void *context)
{
	SampleContext *sampleContext = (SampleContext *)(context);
	DrawSolidCircle(sampleContext->draw, transform, center, radius, color);
}

void DrawSolidCapsuleFcn(b2Pos p1, b2Pos p2, float radius, b2HexColor color, void *context)
{
	SampleContext *sampleContext = (SampleContext *)(context);
	DrawCapsule(sampleContext->draw, p1, p2, radius, color);
}

void DrawLineFcn(b2Pos p1, b2Pos p2, b2HexColor color, void *context)
{
	SampleContext *sampleContext = (SampleContext *)(context);
	DrawLine(sampleContext->draw, p1, p2, color);
}

void DrawTransformFcn(b2WorldTransform transform, void *context)
{
	SampleContext *sampleContext = (SampleContext *)(context);
	DrawTransform(sampleContext->draw, transform, 1.0f);
}

void DrawPointFcn(b2Pos p, float size, b2HexColor color, void *context)
{
	SampleContext *sampleContext = (SampleContext *)(context);
	DrawPoint(sampleContext->draw, p, size, color);
}

void DrawStringFcn(b2Pos p, const char *s, b2HexColor color, void *context)
{
	SampleContext *sampleContext = (SampleContext *)(context);
	// Implementation of DrawString is not provided in the snippet, but it would typically involve rendering text at position p with the specified color.
	
}

void DrawBoundsFcn(b2AABB aabb, b2HexColor color, void *context)
{
	SampleContext *sampleContext = (SampleContext *)(context);
	DrawBounds(sampleContext->draw, aabb, color);
}

static SampleContext s_context;

b2WorldId test1_create_world()
{
	b2WorldDef worldDef = b2DefaultWorldDef();
	worldDef.gravity    = (b2Vec2){0.0f, -10.0f};
	b2WorldId worldId   = b2CreateWorld(&worldDef);

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

	return worldId;
}

int main(int argc, char *argv[])
{
	ecs_world_t *world = ecs_init();
	ecs_fini(world);

	s_context.camera        = GetDefaultCamera();
	s_context.camera.center = (b2Pos){0.0f, 0.0f};
	s_context.camera.zoom   = 12.0f;

	s_context.debugDraw                     = b2DefaultDebugDraw();
	s_context.debugDraw.DrawPolygonFcn      = DrawPolygonFcn;
	s_context.debugDraw.DrawSolidPolygonFcn = DrawSolidPolygonFcn;
	s_context.debugDraw.DrawCircleFcn       = DrawCircleFcn;
	s_context.debugDraw.DrawSolidCircleFcn  = DrawSolidCircleFcn;
	s_context.debugDraw.DrawSolidCapsuleFcn = DrawSolidCapsuleFcn;
	s_context.debugDraw.DrawLineFcn         = DrawLineFcn;
	s_context.debugDraw.DrawTransformFcn    = DrawTransformFcn;
	s_context.debugDraw.DrawPointFcn        = DrawPointFcn;
	s_context.debugDraw.DrawStringFcn       = DrawStringFcn;
	s_context.debugDraw.DrawBoundsFcn       = DrawBoundsFcn;
	s_context.debugDraw.context             = &s_context;
	s_context.debugDraw.drawMass            = true;

	b2WorldId worldId = test1_create_world();

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

	s_context.window = glfwCreateWindow(s_context.camera.width, s_context.camera.height, "App1", NULL, NULL);
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

	s_context.draw = CreateDraw(drawCreateInfo);
	FreeDrawCreateInfo(&drawCreateInfo);

	{
		const char *glVersionString   = (const char *)glGetString(GL_VERSION);
		const char *glslVersionString = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
		printf("OpenGL %s, GLSL %s\n", glVersionString, glslVersionString);
	}

	while (!glfwWindowShouldClose(s_context.window)) {
		int width, height;
		glfwGetWindowSize(s_context.window, &width, &height);
		s_context.camera.width  = width;
		s_context.camera.height = height;

		int bufferWidth, bufferHeight;
		glfwGetFramebufferSize(s_context.window, &bufferWidth, &bufferHeight);
		glViewport(0, 0, bufferWidth, bufferHeight);
		glClearColor(0.07f, 0.07f, 0.09f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		float timeStep     = 1.0f / 60.0f;
		int   subStepCount = 4;
		b2World_Step(worldId, timeStep, subStepCount);

		SetDrawOrigin(s_context.draw, s_context.camera.center);
		b2World_Draw(worldId, &s_context.debugDraw);

		FlushDraw(s_context.draw, &s_context.camera);
		glfwSwapBuffers(s_context.window);
		glfwPollEvents();
	}
	glfwTerminate();

	b2DestroyWorld(worldId);

	return 0;
}
