#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <flecs.h>
#include <box2d/box2d.h>
#include <draw.h>
#include <EgSpatials.h>
#include <EgSpatialsSystems.h>
#include <EgShapes.h>
#include <EgWindows.h>
#include <EgWindowsSdl.h>
#include <EgWindowsSdlGl.h>
#include <EgCameras.h>

#include "fs.h"
#include "b2DebugDraw_init.h"
#include "b2.h"
#include "gcamera.h"

int main(int argc, char *argv[])
{
	ecs_world_t *world = ecs_init();

	ECS_IMPORT(world, EgShapes);
	ECS_IMPORT(world, EgSpatials);
	ECS_IMPORT(world, EgSpatialsSystems);
	ECS_IMPORT(world, EgB2);
	ECS_IMPORT(world, EgWindows);
	ECS_IMPORT(world, EgWindowsSdl);
	ECS_IMPORT(world, EgWindowsSdlGl);
	ECS_IMPORT(world, EgCameras);

	/*
	ecs_entity_t e_debugdraw = ecs_new(world);
	ecs_set(world, e_debugdraw, EgB2DebugDrawDef, {0});

	ecs_entity_t e_b2world = ecs_new(world);
	ecs_set(world, e_b2world, EgB2WorldDef, {0.0f, -10.0f});
	ecs_add_pair(world, e_b2world, EcsDependsOn, e_debugdraw);

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

	ecs_entity_t e_box2 = ecs_new(world);
	ecs_add_pair(world, e_box2, EcsChildOf, e_b2world);
	ecs_set(world, e_box2, Position2, {0.1f, 9.0f});
	ecs_set(world, e_box2, EgB2BodyDef, {b2_dynamicBody});
	ecs_set(world, e_box2, EgB2Box, {1.0f, 1.0f, 1.0f, 0.3f});
	*/

	ecs_log_set_level(0);
	ecs_script_run_file(world, "config/windows.flecs");
	ecs_log_set_level(-1);

	ecs_log_set_level(0);
	ecs_script_run_file(world, "config/camera.flecs");
	ecs_log_set_level(-1);

	// print offset of Velocity3 members:
	printf("Velocity3.x offset: %zu\n", offsetof(Velocity3, x));
	printf("Velocity3.y offset: %zu\n", offsetof(Velocity3, y));
	printf("Velocity3.z offset: %zu\n", offsetof(Velocity3, z));

	ecs_entity_t e_window = ecs_lookup(world, "eg.windows.window1");
	if (!e_window) {
		printf("Failed to find window entity\n");
		return -1;
	}

	/*
	ecs_entity_t e_window = ecs_new(world);
	ecs_add_pair(world, e_window, EcsChildOf, ecs_id(EgWindows));
	ecs_set(world, e_window, EgWindowsWindowCreateInfo, {false});
	ecs_set(world, e_window, EgShapesRectangle, {800, 600});
	*/

	/*
	ecs_entity_t e_camera = ecs_new(world);
	ecs_add_pair(world, e_camera, EcsDependsOn, e_window);
	ecs_add(world, e_camera, EgCamerasState);
	ecs_set(world, e_camera, Position3, {0.0f, 0.0f, 10.0f});
	ecs_set(world, e_camera, Orientation, {0.0f, 0.0f, 0.0f, 1.0f});
	*/


#if 1
	ecs_set(world, EcsWorld, EcsRest, {.port = 0});
	printf("Remote: %s\n", "https://www.flecs.dev/explorer/?page=rest&host=localhost");
#endif

	while (1) {
		if (ecs_has(world, e_window, EgWindowsEventCloseRequest)) {
			printf("Window should close\n");
			break;
		}
		ecs_progress(world, 1.0f / 60.0f);
	}
	ecs_fini(world);

	return 0;
}
