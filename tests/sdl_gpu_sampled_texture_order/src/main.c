#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

typedef struct
{
	float   pos[2];
	float   uv[2];
	uint8_t color[4];
} Vertex;

typedef struct
{
	float scale[2];
	float translate[2];
} VertexUniforms;

static void fail(const char *operation)
{
	fprintf(stderr, "%s failed: %s\n", operation, SDL_GetError());
	exit(EXIT_FAILURE);
}

static void *load_file(const char *path, size_t *size)
{
	FILE *file = fopen(path, "rb");
	if (!file) {
		fprintf(stderr, "Unable to open %s\n", path);
		exit(EXIT_FAILURE);
	}
	if (fseek(file, 0, SEEK_END) != 0) {
		fail("fseek");
	}
	long length = ftell(file);
	if (length <= 0 || fseek(file, 0, SEEK_SET) != 0) {
		fail("ftell/fseek");
	}
	void *data = malloc((size_t)length);
	if (!data || fread(data, 1, (size_t)length, file) != (size_t)length) {
		fail("fread");
	}
	fclose(file);
	*size = (size_t)length;
	return data;
}

static void upload_buffer(SDL_GPUDevice *device, SDL_GPUBuffer *buffer, const void *data, uint32_t size)
{
	SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &(SDL_GPUTransferBufferCreateInfo){
	.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
	.size = size,
	});
	if (!transfer) {
		fail("SDL_CreateGPUTransferBuffer");
	}
	void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
	if (!mapped) {
		fail("SDL_MapGPUTransferBuffer");
	}
	memcpy(mapped, data, size);
	SDL_UnmapGPUTransferBuffer(device, transfer);

	SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
	if (!command) {
		fail("SDL_AcquireGPUCommandBuffer");
	}
	SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
	SDL_UploadToGPUBuffer(copy,
		&(SDL_GPUTransferBufferLocation){.transfer_buffer = transfer},
		&(SDL_GPUBufferRegion){.buffer = buffer, .size = size}, false);
	SDL_EndGPUCopyPass(copy);
	if (!SDL_SubmitGPUCommandBuffer(command)) {
		fail("SDL_SubmitGPUCommandBuffer");
	}
	SDL_ReleaseGPUTransferBuffer(device, transfer);
}

static void upload_texture(SDL_GPUDevice *device, SDL_GPUTexture *texture)
{
	const uint8_t pixel[] = {255, 255, 255, 255};
	SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &(SDL_GPUTransferBufferCreateInfo){
	.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
	.size = sizeof(pixel),
	});
	if (!transfer) {
		fail("SDL_CreateGPUTransferBuffer");
	}
	void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
	if (!mapped) {
		fail("SDL_MapGPUTransferBuffer");
	}
	memcpy(mapped, pixel, sizeof(pixel));
	SDL_UnmapGPUTransferBuffer(device, transfer);

	SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
	if (!command) {
		fail("SDL_AcquireGPUCommandBuffer");
	}
	SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
	SDL_UploadToGPUTexture(copy,
		&(SDL_GPUTextureTransferInfo){.transfer_buffer = transfer},
		&(SDL_GPUTextureRegion){.texture = texture, .w = 1, .h = 1, .d = 1}, false);
	SDL_EndGPUCopyPass(copy);
	if (!SDL_SubmitGPUCommandBuffer(command)) {
		fail("SDL_SubmitGPUCommandBuffer");
	}
	SDL_ReleaseGPUTransferBuffer(device, transfer);
}

static SDL_GPUTexture *create_white_texture(SDL_GPUDevice *device)
{
	SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &(SDL_GPUTextureCreateInfo){
	.type = SDL_GPU_TEXTURETYPE_2D,
	.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
	.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
	.width = 1,
	.height = 1,
	.layer_count_or_depth = 1,
	.num_levels = 1,
	.sample_count = SDL_GPU_SAMPLECOUNT_1,
	});
	if (!texture) {
		fail("SDL_CreateGPUTexture(sampled)");
	}
	return texture;
}

static SDL_GPUTexture *create_depth_texture(SDL_GPUDevice *device)
{
	SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &(SDL_GPUTextureCreateInfo){
	.type = SDL_GPU_TEXTURETYPE_2D,
	.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
	.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
	.width = 1024,
	.height = 1024,
	.layer_count_or_depth = 1,
	.num_levels = 1,
	.sample_count = SDL_GPU_SAMPLECOUNT_1,
	});
	if (!texture) {
		fail("SDL_CreateGPUTexture(depth)");
	}
	return texture;
}

int main(int argc, char *argv[])
{
	bool create_after_claim = argc == 2 && strcmp(argv[1], "--create-after-claim") == 0;
	if (argc > 2 || (argc == 2 && !create_after_claim)) {
		fprintf(stderr, "Usage: %s [--create-after-claim]\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		fail("SDL_Init");
	}
	SDL_Window *window = SDL_CreateWindow("SDL GPU sampled texture ordering", 1024, 1024, 0);
	if (!window) {
		fail("SDL_CreateWindow");
	}
	SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
	if (!device) {
		fail("SDL_CreateGPUDevice");
	}

	SDL_GPUTexture *sampled_texture = NULL;
	SDL_GPUTexture *depth_texture = create_depth_texture(device);
	if (!create_after_claim) {
		puts("Creating sampled texture before SDL_ClaimWindowForGPUDevice");
		sampled_texture = create_white_texture(device);
	}

	size_t vertex_shader_size;
	size_t fragment_shader_size;
	void *vertex_shader_code = load_file("../../apps/sdlapp/data/vertex.spv", &vertex_shader_size);
	void *fragment_shader_code = load_file("../../apps/sdlapp/data/fragment.spv", &fragment_shader_size);
	SDL_GPUShader *vertex_shader = SDL_CreateGPUShader(device, &(SDL_GPUShaderCreateInfo){
	.code = vertex_shader_code,
	.code_size = vertex_shader_size,
	.entrypoint = "main",
	.format = SDL_GPU_SHADERFORMAT_SPIRV,
	.stage = SDL_GPU_SHADERSTAGE_VERTEX,
	.num_uniform_buffers = 1,
	});
	SDL_GPUShader *fragment_shader = SDL_CreateGPUShader(device, &(SDL_GPUShaderCreateInfo){
	.code = fragment_shader_code,
	.code_size = fragment_shader_size,
	.entrypoint = "main",
	.format = SDL_GPU_SHADERFORMAT_SPIRV,
	.stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
	.num_samplers = 1,
	});
	free(vertex_shader_code);
	free(fragment_shader_code);
	if (!vertex_shader || !fragment_shader) {
		fail("SDL_CreateGPUShader");
	}

	SDL_GPUVertexAttribute attributes[] = {
	{.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(Vertex, pos)},
	{.location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(Vertex, uv)},
	{.location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, .offset = offsetof(Vertex, color)},
	};
	SDL_GPUVertexBufferDescription vertex_buffer_description = {.slot = 0, .pitch = sizeof(Vertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX};
	SDL_GPUColorTargetDescription color_target_description = {.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM};
	SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(device, &(SDL_GPUGraphicsPipelineCreateInfo){
	.vertex_shader = vertex_shader,
	.fragment_shader = fragment_shader,
	.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
	.vertex_input_state = {
		.vertex_buffer_descriptions = &vertex_buffer_description,
		.num_vertex_buffers = 1,
		.vertex_attributes = attributes,
		.num_vertex_attributes = SDL_arraysize(attributes),
	},
		.target_info = {
			.color_target_descriptions = &color_target_description,
			.num_color_targets = 1,
			.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
			.has_depth_stencil_target = true,
		},
		.depth_stencil_state = {
			.enable_depth_test = true,
			.enable_depth_write = true,
			.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
		},
		.multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1},
	});
	if (!pipeline) {
		fail("SDL_CreateGPUGraphicsPipeline");
	}
	if (!SDL_ClaimWindowForGPUDevice(device, window)) {
		fail("SDL_ClaimWindowForGPUDevice");
	}
	if (create_after_claim) {
		puts("Creating sampled texture after SDL_ClaimWindowForGPUDevice");
		sampled_texture = create_white_texture(device);
	}

	const Vertex vertices[] = {
	{{-0.5f, -0.5f}, {0.0f, 1.0f}, {255, 255, 255, 255}},
	{{0.5f, -0.5f}, {1.0f, 1.0f}, {255, 255, 255, 255}},
	{{0.5f, 0.5f}, {1.0f, 0.0f}, {255, 255, 255, 255}},
	{{-0.5f, -0.5f}, {0.0f, 1.0f}, {255, 255, 255, 255}},
	{{0.5f, 0.5f}, {1.0f, 0.0f}, {255, 255, 255, 255}},
	{{-0.5f, 0.5f}, {0.0f, 0.0f}, {255, 255, 255, 255}},
	};
	SDL_GPUBuffer *vertex_buffer = SDL_CreateGPUBuffer(device, &(SDL_GPUBufferCreateInfo){.usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = sizeof(vertices)});
	SDL_GPUSampler *sampler = SDL_CreateGPUSampler(device, &(SDL_GPUSamplerCreateInfo){
	.min_filter = SDL_GPU_FILTER_NEAREST,
	.mag_filter = SDL_GPU_FILTER_NEAREST,
	.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
	.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	});
	if (!vertex_buffer || !sampler) {
		fail("SDL_CreateGPUBuffer/SDL_CreateGPUSampler");
	}
	upload_buffer(device, vertex_buffer, vertices, sizeof(vertices));
	upload_texture(device, sampled_texture);

	SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
	SDL_GPUTexture *swapchain_texture = NULL;
	if (!command || !SDL_WaitAndAcquireGPUSwapchainTexture(command, window, &swapchain_texture, NULL, NULL)) {
		fail("SDL_WaitAndAcquireGPUSwapchainTexture");
	}
	if (!swapchain_texture) {
		puts("No swapchain texture acquired");
		SDL_SubmitGPUCommandBuffer(command);
		return EXIT_SUCCESS;
	}
	VertexUniforms uniforms = {.scale = {1.0f, 1.0f}};
	SDL_PushGPUVertexUniformData(command, 0, &uniforms, sizeof(uniforms));
	SDL_GPUColorTargetInfo color_target = {
	.texture = swapchain_texture,
	.clear_color = {0.05f, 0.05f, 0.08f, 1.0f},
	.load_op = SDL_GPU_LOADOP_CLEAR,
	.store_op = SDL_GPU_STOREOP_STORE,
	};
	SDL_GPUDepthStencilTargetInfo depth_target = {
	.texture = depth_texture,
	.clear_depth = 1.0f,
	.load_op = SDL_GPU_LOADOP_CLEAR,
	.store_op = SDL_GPU_STOREOP_DONT_CARE,
	.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
	.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
	};
	SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(command, &color_target, 1, &depth_target);
	SDL_BindGPUGraphicsPipeline(pass, pipeline);
	SDL_BindGPUVertexBuffers(pass, 0, &(SDL_GPUBufferBinding){.buffer = vertex_buffer}, 1);
	SDL_BindGPUFragmentSamplers(pass, 0, &(SDL_GPUTextureSamplerBinding){.texture = sampled_texture, .sampler = sampler}, 1);
	puts("Drawing one primitive batch");
	SDL_DrawGPUPrimitives(pass, SDL_arraysize(vertices), 1, 0, 0);
	SDL_EndGPURenderPass(pass);
	if (!SDL_SubmitGPUCommandBuffer(command)) {
		fail("SDL_SubmitGPUCommandBuffer");
	}

	SDL_ReleaseGPUSampler(device, sampler);
	SDL_ReleaseGPUBuffer(device, vertex_buffer);
	SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
	SDL_ReleaseGPUShader(device, vertex_shader);
	SDL_ReleaseGPUShader(device, fragment_shader);
	SDL_ReleaseGPUTexture(device, sampled_texture);
	SDL_ReleaseGPUTexture(device, depth_texture);
	SDL_ReleaseWindowFromGPUDevice(device, window);
	SDL_DestroyGPUDevice(device);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return EXIT_SUCCESS;
}
