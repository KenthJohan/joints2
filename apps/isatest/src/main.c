#include <flecs.h>
#include <egmisc.h>
#include <EgSpatials.h>
#include "isa.h"

int main(int argc, char *argv[])
{
	ecs_os_set_api_defaults();
	ecs_os_api_t os_api = ecs_os_get_api();
	ecs_os_set_api(&os_api);

	ecs_world_t *world = ecs_init_w_args(argc, argv);
	ECS_IMPORT(world, Isa);
	ECS_IMPORT(world, EgSpatials);

#if 1
	ecs_set(world, EcsWorld, EcsRest, {.port = 0});
	printf("Flecs Explorer: %s\n", "https://www.flecs.dev/explorer/?page=rest&host=localhost");
#endif

	char *file_content = eg_file_load_alloc("data/p1.isa0", NULL);
	printf("Loaded file content: %s\n", file_content);

	if (!IsaRun(world, file_content)) {
		printf("Failed to run isa script\n");
	}

	IsaStack_print_all(world);

	ecs_os_free(file_content);

	
	/*
	while (1) {
		ecs_os_sleep(0, 1000 * 1000 * 10);
		ecs_progress(world, 1.0f / 60.0f);
	}
	*/
	
	ecs_fini(world);

	return 0;
}
