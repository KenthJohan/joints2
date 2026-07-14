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
#include <EgFs.h>
#include <EgCameras.h>

#include "fs.h"
#include "b2DebugDraw_init.h"
#include "b2.h"
#include "AppDraw.h"

int main(int argc, char *argv[])
{
	ecs_os_set_api_defaults();
	ecs_os_api_t os_api = ecs_os_get_api();
	ecs_os_set_api(&os_api);

	ecs_world_t *world = ecs_init();

	ECS_IMPORT(world, EgShapes);
	ECS_IMPORT(world, EgSpatials);
	ECS_IMPORT(world, EgSpatialsSystems);
	ECS_IMPORT(world, EgB2);
	ECS_IMPORT(world, EgWindows);
	ECS_IMPORT(world, EgWindowsSdl);
	ECS_IMPORT(world, EgWindowsSdlGl);
	ECS_IMPORT(world, EgCameras);
	ECS_IMPORT(world, EgFs);
	ECS_IMPORT(world, AppDraw);

	ecs_log_set_level(0);
	ecs_script_run_file(world, "config/windows.flecs");
	ecs_log_set_level(-1);

	ecs_log_set_level(0);
	ecs_script_run_file(world, "config/camera.flecs");
	ecs_log_set_level(-1);

	ecs_log_set_level(0);
	ecs_script_run_file(world, "config/EgKeyboards.flecs");
	ecs_log_set_level(-1);

	ecs_log_set_level(0);
	ecs_script_run_file(world, "config/keybindings_3d.flecs");
	ecs_log_set_level(-1);

	ecs_log_set_level(0);
	ecs_script_run_file(world, "config/physics.flecs");
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

#if 1
	ecs_set(world, EcsWorld, EcsRest, {.port = 0});
	printf("Flecs Explorer: %s\n", "https://www.flecs.dev/explorer/?page=rest&host=localhost");
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
