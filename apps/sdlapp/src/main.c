#include <flecs.h>
#include <EgWindows.h>
#include <EgWindowsSdl.h>
#include <EgButtons.h>
#include <EgSpatials.h>
#include <EgDisplays.h>
#include <EgDisplaysSdl.h>
#include <EgGpus.h>
#include <EgGpusSdl.h>


int main(int argc, char *argv[])
{
	ecs_os_set_api_defaults();
	ecs_os_api_t os_api = ecs_os_get_api();
	ecs_os_set_api(&os_api);

	ecs_world_t *world = ecs_init();

	ECS_IMPORT(world, EgWindows);
	ECS_IMPORT(world, EgWindowsSdl);
	ECS_IMPORT(world, EgButtons);
	ECS_IMPORT(world, EgSpatials);
	ECS_IMPORT(world, EgDisplays);
	ECS_IMPORT(world, EgDisplaysSdl);
	ECS_IMPORT(world, EgGpus);
	ECS_IMPORT(world, EgGpusSdl);


	ecs_log_set_level(0);
	ecs_script_run_file(world, "config/windows.flecs");
	ecs_log_set_level(-1);

	ecs_entity_t e_window = ecs_lookup(world, "window1");
	if (!e_window) {
		printf("Failed to find window entity\n");
		return -1;
	}

#if 1
	ecs_set(world, EcsWorld, EcsRest, {.port = 0});
	printf("Flecs Explorer: %s\n", "https://www.flecs.dev/explorer/?page=rest&host=localhost");
#endif

	while (1) {
		if (ecs_has(world, e_window, EgWindowsCloseRequest)) {
			printf("Window should close\n");
			break;
		}
		ecs_progress(world, 1.0f / 60.0f);
	}
	ecs_fini(world);

	return 0;
}
