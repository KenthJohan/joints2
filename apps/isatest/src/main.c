#include <flecs.h>

int main(int argc, char *argv[])
{
	ecs_os_set_api_defaults();
	ecs_os_api_t os_api = ecs_os_get_api();
	ecs_os_set_api(&os_api);

	ecs_world_t *world = ecs_init_w_args(argc, argv);

#if 1
	ecs_set(world, EcsWorld, EcsRest, {.port = 0});
	printf("Flecs Explorer: %s\n", "https://www.flecs.dev/explorer/?page=rest&host=localhost");
#endif

	while (1) {
		ecs_os_sleep(0, 1000*1000*10);
		ecs_progress(world, 1.0f / 60.0f);
	}
	ecs_fini(world);

	return 0;
}
