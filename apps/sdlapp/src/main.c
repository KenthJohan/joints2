#include <flecs.h>
#include <EgWindows.h>
#include <EgWindowsSdl.h>
#include <EgButtons.h>
#include <EgSpatials.h>
#include <EgDisplays.h>
#include <EgDisplaysSdl.h>
#include <EgGpus.h>
#include <EgGpusSdl.h>
#include <EgSpirv.h>
#include <EgFs.h>
#include <SDL3/SDL_gpu.h>
#include <string.h>
#include "eg_drawlist.h"
#include "backend_sdlgpu.h"

typedef struct
{
	bool          claimed;
	bool          texture_uploaded;
	eg_drawlist_t drawlist;
} DrawDemo;

// Vertex uniform buffer layout must match `UBO` in data/shader.vert (uScale, uTranslate).
typedef struct
{
	float scale[2];
	float translate[2];
} DrawDemoVertexUBO;

// Renders a single rectangle using the gpu0/pip/depth_texture entities from windows.flecs.
// The required GPU objects are created asynchronously by EgGpusSdl systems, so this
// function is a no-op until the device, pipeline and depth texture are all ready.
static void draw_demo_render(ecs_world_t *world, ecs_entity_t e_window, ecs_entity_t e_device, ecs_entity_t e_pipeline, ecs_entity_t e_depth, ecs_entity_t e_vertex_buffer, ecs_entity_t e_index_buffer, ecs_entity_t e_widget_buffer, ecs_entity_t e_white_texture, ecs_entity_t e_sampler, DrawDemo *demo)
{
	const EgWindowsWindow        *win           = ecs_get(world, e_window, EgWindowsWindow);
	const EgGpusDevice           *dev           = ecs_get(world, e_device, EgGpusDevice);
	const EgGpusGraphicsPipeline *pip           = ecs_get(world, e_pipeline, EgGpusGraphicsPipeline);
	const EgGpusTexture          *depth         = ecs_get(world, e_depth, EgGpusTexture);
	const EgGpusBuffer           *vertex_buffer = ecs_get(world, e_vertex_buffer, EgGpusBuffer);
	const EgGpusBuffer           *index_buffer  = ecs_get(world, e_index_buffer, EgGpusBuffer);
	const EgGpusBuffer           *widget_buffer = ecs_get(world, e_widget_buffer, EgGpusBuffer);
	const EgGpusTexture          *white_texture = ecs_get(world, e_white_texture, EgGpusTexture);
	const EgGpusSampler          *sampler       = ecs_get(world, e_sampler, EgGpusSampler);
	if (!win || !win->object || !dev || !dev->object || !pip || !pip->object || !depth || !depth->object || !vertex_buffer || !vertex_buffer->object || !index_buffer || !index_buffer->object || !widget_buffer || !widget_buffer->object || !white_texture || !white_texture->object || !sampler || !sampler->object) {
		return; // GPU resources are not ready yet.
	}

	if (!demo->claimed) {
		if (!SDL_ClaimWindowForGPUDevice(dev->object, win->object)) {
			printf("SDL_ClaimWindowForGPUDevice() failed: %s\n", SDL_GetError());
			return;
		}
		demo->claimed = true;
		printf("Swapchain format: %d (pipeline expects %d)\n", SDL_GetGPUSwapchainTextureFormat(dev->object, win->object), SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM);
		return;
	}

	if (!demo->texture_uploaded) {
		uint8_t white_pixel[4] = {255, 255, 255, 255};
		if (!EgGpusSdlUploadTextureArrayLayer(dev, white_texture, white_pixel, sizeof(white_pixel), 0, 1, 1)) {
			printf("Failed to upload draw resources: %s\n", SDL_GetError());
			return;
		}
		demo->texture_uploaded = true;
	}

	float clip[4] = {0.0f, 0.0f, 1024.0f, 1024.0f}; // Matches window1's Rectangle in config/windows.flecs.
	eg_drawlist_reset(&demo->drawlist);
	float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	eg_drawlist_new_cmd(&demo->drawlist, clip, color, 0);
	eg_drawlist_add_rect(&demo->drawlist, -0.5f, -0.5f, 0.5f, 0.5f);
	if (!backend_sdlgpu_upload(&demo->drawlist, dev, vertex_buffer, index_buffer, widget_buffer)) {
		printf("Failed to upload drawlist: %s\n", SDL_GetError());
		return;
	}

	SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(dev->object);
	if (!cmd) {
		printf("SDL_AcquireGPUCommandBuffer() failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
		return;
	}

	SDL_GPUTexture *swapchain_texture = NULL;
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, win->object, &swapchain_texture, NULL, NULL)) {
		printf("SDL_WaitAndAcquireGPUSwapchainTexture() failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
		SDL_SubmitGPUCommandBuffer(cmd);
		return;
	}
	if (!swapchain_texture) {
		SDL_SubmitGPUCommandBuffer(cmd); // Window is minimized or not ready this frame.
		return;
	}

	SDL_GPUColorTargetInfo color_target = {0};
	color_target.texture                = swapchain_texture;
	color_target.clear_color            = (SDL_FColor){0.05f, 0.05f, 0.08f, 1.0f};
	color_target.load_op                = SDL_GPU_LOADOP_CLEAR;
	color_target.store_op               = SDL_GPU_STOREOP_STORE;

	SDL_GPUDepthStencilTargetInfo depth_target = {0};
	depth_target.texture                       = depth->object;
	depth_target.clear_depth                   = 1.0f;
	depth_target.load_op                       = SDL_GPU_LOADOP_CLEAR;
	depth_target.store_op                      = SDL_GPU_STOREOP_DONT_CARE;
	depth_target.stencil_load_op               = SDL_GPU_LOADOP_DONT_CARE;
	depth_target.stencil_store_op              = SDL_GPU_STOREOP_DONT_CARE;

	// Identity transform: draw the quad directly in clip space, matching data/shader.vert.
	DrawDemoVertexUBO ubo = {.scale = {1.0f, 1.0f}, .translate = {0.0f, 0.0f}};
	SDL_PushGPUVertexUniformData(cmd, 0, &ubo, sizeof(ubo));

	SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, &depth_target);
	backend_sdlgpu_draw(&demo->drawlist, pass, pip, vertex_buffer, index_buffer, widget_buffer, white_texture, sampler);
	SDL_EndGPURenderPass(pass);

	SDL_SubmitGPUCommandBuffer(cmd);
}

int main(int argc, char *argv[])
{
	ecs_os_set_api_defaults();
	ecs_os_api_t os_api = ecs_os_get_api();
	ecs_os_set_api(&os_api);

	setvbuf(stdout, NULL, _IONBF, 0); // Diagnostic: keep log ordering accurate against SDL's unbuffered stderr output.

	ecs_world_t *world = ecs_init_w_args(argc, argv);

	ECS_IMPORT(world, FlecsUnits);
	ECS_IMPORT(world, EgWindows);
	ECS_IMPORT(world, EgWindowsSdl);
	ECS_IMPORT(world, EgButtons);
	ECS_IMPORT(world, EgSpatials);
	ECS_IMPORT(world, EgDisplays);
	ECS_IMPORT(world, EgDisplaysSdl);
	ECS_IMPORT(world, EgGpus);
	ECS_IMPORT(world, EgGpusSdl);
	ECS_IMPORT(world, EgSpirv);
	ECS_IMPORT(world, EgFs);

	ecs_log_set_level(0);
	ecs_script_run_file(world, "config/windows.flecs");
	ecs_log_set_level(-1);

	ecs_entity_t e_window = ecs_lookup(world, "window1");
	if (!e_window) {
		printf("Failed to find window entity\n");
		return -1;
	}

	ecs_entity_t e_gpu_device    = ecs_lookup(world, "gpu0");
	ecs_entity_t e_pipeline      = ecs_lookup(world, "gpu0.pip");
	ecs_entity_t e_depth         = ecs_lookup(world, "gpu0.depth_texture");
	ecs_entity_t e_vertex_buffer = ecs_lookup(world, "gpu0.vertex_buffer");
	ecs_entity_t e_index_buffer  = ecs_lookup(world, "gpu0.index_buffer");
	ecs_entity_t e_widget_buffer = ecs_lookup(world, "gpu0.widget_buffer");
	ecs_entity_t e_white_texture = ecs_lookup(world, "gpu0.white_texture");
	ecs_entity_t e_sampler       = ecs_lookup(world, "gpu0.nearest_sampler");
	if (!e_gpu_device || !e_pipeline || !e_depth || !e_vertex_buffer || !e_index_buffer || !e_widget_buffer || !e_white_texture || !e_sampler) {
		printf("Failed to find draw resource entities\n");
		return -1;
	}

#if 1
	ecs_set(world, EcsWorld, EcsRest, {.port = 0});
	printf("Flecs Explorer: %s\n", "https://www.flecs.dev/explorer/?page=rest&host=localhost");
#endif

	DrawDemo draw_demo = {0};
	eg_drawlist_init(&draw_demo.drawlist);

	while (1) {
		if (ecs_should_quit(world)) {
			break;
		}
		if (ecs_has(world, e_window, EgWindowsCloseRequest)) {
			printf("Window should close\n");
		}
		if (ecs_has(world, ecs_id(EgWindows), EgWindowsQuit)) {
			printf("Quit requested\n");
			break;
		}
		ecs_progress(world, 1.0f / 60.0f);
		draw_demo_render(world, e_window, e_gpu_device, e_pipeline, e_depth, e_vertex_buffer, e_index_buffer, e_widget_buffer, e_white_texture, e_sampler, &draw_demo);
	}

	eg_drawlist_fini(&draw_demo.drawlist);
	ecs_fini(world);

	return 0;
}
