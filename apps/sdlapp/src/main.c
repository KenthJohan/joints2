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

// Layout must match the `Vertex123` struct declared in config/windows.flecs.
typedef struct
{
	float   pos[2];
	float   uv[2];
	uint8_t color[4];
} DrawDemoVertex;

typedef struct
{
	bool           claimed;
	SDL_GPUBuffer *vertex_buffer;
} DrawDemo;

// Renders a single rectangle using the gpu0/pip/texture0 entities from windows.flecs.
// The required GPU objects are created asynchronously by EgGpusSdl systems, so this
// function is a no-op until the device, pipeline and depth texture are all ready.
static void draw_demo_render(ecs_world_t *world, ecs_entity_t e_window, ecs_entity_t e_device, ecs_entity_t e_pipeline, ecs_entity_t e_depth, DrawDemo *demo)
{
	const EgWindowsWindow         *win   = ecs_get(world, e_window, EgWindowsWindow);
	const EgGpusDevice             *dev   = ecs_get(world, e_device, EgGpusDevice);
	const EgGpusGraphicsPipeline  *pip   = ecs_get(world, e_pipeline, EgGpusGraphicsPipeline);
	const EgGpusTexture           *depth = ecs_get(world, e_depth, EgGpusTexture);
	if (!win || !win->object || !dev || !dev->object || !pip || !pip->object || !depth || !depth->object) {
		return; // GPU resources are not ready yet.
	}

	if (!demo->claimed) {
		if (!SDL_ClaimWindowForGPUDevice(dev->object, win->object)) {
			printf("SDL_ClaimWindowForGPUDevice() failed: %s\n", SDL_GetError());
			return;
		}
		demo->claimed = true;
	}

	if (!demo->vertex_buffer) {
		DrawDemoVertex vertices[6] = {
		{.pos = {-0.5f, -0.5f}, .uv = {0.0f, 1.0f}, .color = {255, 255, 255, 255}},
		{.pos = {0.5f, -0.5f}, .uv = {1.0f, 1.0f}, .color = {255, 255, 255, 255}},
		{.pos = {0.5f, 0.5f}, .uv = {1.0f, 0.0f}, .color = {255, 255, 255, 255}},
		{.pos = {-0.5f, -0.5f}, .uv = {0.0f, 1.0f}, .color = {255, 255, 255, 255}},
		{.pos = {0.5f, 0.5f}, .uv = {1.0f, 0.0f}, .color = {255, 255, 255, 255}},
		{.pos = {-0.5f, 0.5f}, .uv = {0.0f, 0.0f}, .color = {255, 255, 255, 255}},
		};
		Uint32 size = sizeof(vertices);

		SDL_GPUBufferCreateInfo vb_info = {.usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = size};
		demo->vertex_buffer            = SDL_CreateGPUBuffer(dev->object, &vb_info);
		if (!demo->vertex_buffer) {
			printf("SDL_CreateGPUBuffer() failed: %s\n", SDL_GetError());
			return;
		}
		SDL_SetGPUBufferName(dev->object, demo->vertex_buffer, "draw_demo_rectangle");

		SDL_GPUTransferBufferCreateInfo tb_info = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size};
		SDL_GPUTransferBuffer          *transfer = SDL_CreateGPUTransferBuffer(dev->object, &tb_info);
		if (!transfer) {
			printf("SDL_CreateGPUTransferBuffer() failed: %s\n", SDL_GetError());
			return;
		}
		void *map = SDL_MapGPUTransferBuffer(dev->object, transfer, false);
		memcpy(map, vertices, size);
		SDL_UnmapGPUTransferBuffer(dev->object, transfer);

		SDL_GPUCommandBuffer *upload_cmd  = SDL_AcquireGPUCommandBuffer(dev->object);
		SDL_GPUCopyPass       *copy_pass   = SDL_BeginGPUCopyPass(upload_cmd);
		SDL_GPUTransferBufferLocation src = {.transfer_buffer = transfer, .offset = 0};
		SDL_GPUBufferRegion           dst = {.buffer = demo->vertex_buffer, .offset = 0, .size = size};
		SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);
		SDL_EndGPUCopyPass(copy_pass);
		SDL_SubmitGPUCommandBuffer(upload_cmd);
		SDL_ReleaseGPUTransferBuffer(dev->object, transfer);
	}

	SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(dev->object);
	if (!cmd) {
		printf("SDL_AcquireGPUCommandBuffer() failed: %s\n", SDL_GetError());
		return;
	}

	SDL_GPUTexture *swapchain_texture = NULL;
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, win->object, &swapchain_texture, NULL, NULL)) {
		printf("SDL_WaitAndAcquireGPUSwapchainTexture() failed: %s\n", SDL_GetError());
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

	SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, &depth_target);
	SDL_BindGPUGraphicsPipeline(pass, pip->object);
	SDL_GPUBufferBinding vb_binding = {.buffer = demo->vertex_buffer, .offset = 0};
	SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);
	SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
	SDL_EndGPURenderPass(pass);

	SDL_SubmitGPUCommandBuffer(cmd);
}

int main(int argc, char *argv[])
{
	ecs_os_set_api_defaults();
	ecs_os_api_t os_api = ecs_os_get_api();
	ecs_os_set_api(&os_api);

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

	ecs_entity_t e_gpu_device = ecs_lookup(world, "gpu0");
	ecs_entity_t e_pipeline   = ecs_lookup(world, "gpu0.pip");
	ecs_entity_t e_depth      = ecs_lookup(world, "gpu0.texture0");
	if (!e_gpu_device || !e_pipeline || !e_depth) {
		printf("Failed to find gpu0/pip/texture0 entities\n");
		return -1;
	}

#if 1
	ecs_set(world, EcsWorld, EcsRest, {.port = 0});
	printf("Flecs Explorer: %s\n", "https://www.flecs.dev/explorer/?page=rest&host=localhost");
#endif

	DrawDemo draw_demo = {0};

	while (1) {
		if (ecs_has(world, e_window, EgWindowsCloseRequest)) {
			printf("Window should close\n");
			break;
		}
		ecs_progress(world, 1.0f / 60.0f);
		draw_demo_render(world, e_window, e_gpu_device, e_pipeline, e_depth, &draw_demo);
	}

	if (draw_demo.vertex_buffer) {
		const EgGpusDevice *dev = ecs_get(world, e_gpu_device, EgGpusDevice);
		if (dev && dev->object) {
			SDL_ReleaseGPUBuffer(dev->object, draw_demo.vertex_buffer);
		}
	}

	ecs_fini(world);

	return 0;
}
