#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <cglm/cglm.h>
#include <flecs.h>
#include <box2d/box2d.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <draw.h>

void glfwErrorCallback(int error, const char *description)
{
	fprintf(stderr, "GLFW error occurred. Code: %d. Description: %s\n", error, description);
}

int main(int argc, char *argv[])
{
	ecs_world_t *world = ecs_init();
	ecs_fini(world);

	struct GLFWwindow *window = NULL;
	Camera             camera = GetDefaultCamera();
	Draw              *draw;

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

	window = glfwCreateWindow(camera.width, camera.height, "App1", NULL, NULL);
	if (window == NULL) {
		fprintf(stderr, "Failed to open GLFW window.\n");
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Load OpenGL functions using glad
	if (!gladLoadGL()) {
		fprintf(stderr, "Failed to initialize glad\n");
		glfwTerminate();
		return -1;
	}

	{
		const char *glVersionString   = (const char *)glGetString(GL_VERSION);
		const char *glslVersionString = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
		printf("OpenGL %s, GLSL %s\n", glVersionString, glslVersionString);
	}

	while (!glfwWindowShouldClose(window)) {
		int width, height;
		glfwGetWindowSize(window, &width, &height);
		camera.width  = width;
		camera.height = height;

		int bufferWidth, bufferHeight;
		glfwGetFramebufferSize(window, &bufferWidth, &bufferHeight);
		glViewport(0, 0, bufferWidth, bufferHeight);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	glfwTerminate();

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

	float timeStep = 1.0f / 60.0f;

	int subStepCount = 4;

	for (int i = 0; i < 90; ++i) {
		b2World_Step(worldId, timeStep, subStepCount);
		b2Vec2 position = b2Body_GetPosition(bodyId);
		b2Rot  rotation = b2Body_GetRotation(bodyId);
		printf("%4.2f %4.2f %4.2f\n", position.x, position.y, b2Rot_GetAngle(rotation));
	}

	b2DestroyWorld(worldId);

	return 0;
}
