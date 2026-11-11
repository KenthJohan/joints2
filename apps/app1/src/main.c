#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <flecs.h>
#include <box2d/box2d.h>
#include <EgSpatials.h>
#include <EgSpatialsSystems.h>
#include <EgShapes.h>
#include <EgWindows.h>
#include <EgWindowsSdl.h>
#include <EgWindowsSdlGl.h>
#include <EgCameras.h>
#include <EgPhysics.h>
#include <EgPhysicsBox2d.h>
#include <EgUi.h>
#include <EgBase.h>
#include <EgButtons.h>

#include "AppDraw.h"

void run_file(ecs_world_t *world, const char *file)
{
	ecs_log_set_level(0);
	int r = ecs_script_run_file(world, file);
	ecs_log_set_level(-1);
	if (r != 0) {
		printf("Failed to run script file: %s\n", file);
		exit(EXIT_FAILURE);
	}
}

void run_managed_file(ecs_world_t *world, const char *file)
{
	// Managed scripts reevaluate template props when their input components change.
	ecs_log_set_level(0);
	ecs_entity_t script = ecs_script(world, {.filename = file});
	ecs_log_set_level(-1);
	if (!script) {
		printf("Failed to run managed script file: %s\n", file);
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char *argv[])
{
	ecs_os_set_api_defaults();
	ecs_os_api_t os_api = ecs_os_get_api();
	ecs_os_set_api(&os_api);

	ecs_world_t *world = ecs_init_w_args(argc, argv);

	ECS_IMPORT(world, EgShapes);
	ECS_IMPORT(world, EgSpatials);
	ECS_IMPORT(world, EgSpatialsSystems);
	ECS_IMPORT(world, EgWindows);
	ECS_IMPORT(world, EgWindowsSdl);
	ECS_IMPORT(world, EgWindowsSdlGl);
	ECS_IMPORT(world, EgCameras);
	ECS_IMPORT(world, EgPhysics);
	ECS_IMPORT(world, EgPhysicsBox2d);
	ECS_IMPORT(world, EgUi);
	ECS_IMPORT(world, AppDraw);

	run_file(world, "config/EgButtons.flecs");
	run_file(world, "config/tags.flecs");
	run_file(world, "config/windows.flecs");
	run_file(world, "config/cameras.flecs");

	run_file(world, "config/xui/base.flecs");
	run_file(world, "config/xui/resizable.flecs");
	run_file(world, "config/xui/button.flecs");
	run_file(world, "config/xui/checkbox.flecs");
	run_file(world, "config/xui/flow1.flecs");

	run_managed_file(world, "config/huds.flecs");
	run_managed_file(world, "config/physics.flecs");
	run_file(world, "config/keybindings_3d.flecs");
	run_file(world, "config/keybindings_common.flecs");

	// print offset of Velocity3 members:
	printf("Velocity3.x offset: %zu\n", offsetof(Velocity3, x));
	printf("Velocity3.y offset: %zu\n", offsetof(Velocity3, y));
	printf("Velocity3.z offset: %zu\n", offsetof(Velocity3, z));

	ecs_entity_t e_window = ecs_lookup(world, "eg.windows.window1");
	if (!e_window) {
		printf("Failed to find window entity\n");
		return -1;
	}

	ecs_entity_t quit_binding = ecs_lookup(world, "keybindings.quit");
	if (!quit_binding || !ecs_has(world, quit_binding, EgButtonsBinding)) {
		fprintf(stderr, "Failed to find quit key binding\n");
		ecs_fini(world);
		return EXIT_FAILURE;
	}

	ecs_entity_t fps_label = ecs_lookup(world, "huds.root2d.fps_counter");
	if (!fps_label) {
		fprintf(stderr, "Failed to find FPS label\n");
		ecs_fini(world);
		return EXIT_FAILURE;
	}

#if 1
	ecs_set(world, EcsWorld, EcsRest, {.port = 0});
	printf("Flecs Explorer: %s\n", "https://www.flecs.dev/explorer/?page=rest&host=localhost");
#endif

	ecs_time_t fps_timer = {0};
	ecs_time_measure(&fps_timer);
	double       fps_elapsed       = 0.0;
	double       fps_reset_elapsed = 0.0;
	double       fps_min           = 0.0;
	double       fps_max           = 0.0;
	unsigned int fps_frames        = 0;

	while (1) {
		bool quit = ecs_progress(world, 1.0f / 60.0f);
		if (!quit) {
			break;
		}
		EgButtonsBinding const *binding = ecs_get(world, quit_binding, EgButtonsBinding);
		EgButtonsState *buttons = ecs_singleton_get_mut(world, EgButtonsState);
		if (!binding || !buttons) {
			fprintf(stderr, "Quit binding or button state is missing\n");
			ecs_fini(world);
			return EXIT_FAILURE;
		}
		if (EgButtonsState_get(buttons, (uint32_t)binding->button0) & binding->mask) {
			break;
		}
		fps_elapsed += ecs_time_measure(&fps_timer);
		fps_frames++;
		if (fps_elapsed >= 0.5) {
			double fps = fps_frames / fps_elapsed;
			fps_reset_elapsed += fps_elapsed;
			if (fps_min == 0.0 || fps_reset_elapsed >= 10.0) {
				fps_min           = fps;
				fps_max           = fps;
				fps_reset_elapsed = 0.0;
			} else {
				if (fps < fps_min) {
					fps_min = fps;
				}
				if (fps > fps_max) {
					fps_max = fps;
				}
			}
			char text[128];
			snprintf(text, sizeof(text), "FPS: %.1f  Min: %.1f  Max: %.1f", fps, fps_min, fps_max);
			ecs_set(world, fps_label, EgBaseText, {.value = text});
			fps_elapsed = 0.0;
			fps_frames  = 0;
		}
		if (ecs_has(world, e_window, EgWindowsCloseRequest)) {
			printf("Window should close\n");
			break;
		}
	}
	ecs_fini(world);

	return 0;
}
