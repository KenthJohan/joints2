#include "backend_sdlgpu.h"
#include <EgGpusSdl.h>
#include <stdint.h>

bool backend_sdlgpu_upload(const eg_drawlist_t *drawlist, const EgGpusDevice *device, const EgGpusBuffer *vertex_buffer, const EgGpusBuffer *index_buffer, const EgGpusBuffer *widget_buffer)
{
	int32_t vertex_count = ecs_vec_count(&drawlist->vertices);
	int32_t index_count  = ecs_vec_count(&drawlist->indices);
	int32_t widget_count = ecs_vec_count(&drawlist->widgets);
	if (vertex_count == 0 || index_count == 0) {
		return true; // Nothing to draw this frame.
	}

	bool ok = EgGpusSdlUploadBuffer(device, vertex_buffer, ecs_vec_first(&drawlist->vertices), (uint32_t)(vertex_count * (int32_t)sizeof(eg_drawvert_t)));
	ok      = ok && EgGpusSdlUploadBuffer(device, index_buffer, ecs_vec_first(&drawlist->indices), (uint32_t)(index_count * (int32_t)sizeof(uint32_t)));
	ok      = ok && EgGpusSdlUploadBuffer(device, widget_buffer, ecs_vec_first(&drawlist->widgets), (uint32_t)(widget_count * (int32_t)sizeof(eg_widget_data_t)));
	return ok;
}

void backend_sdlgpu_draw(const eg_drawlist_t *drawlist, SDL_GPURenderPass *render_pass, const EgGpusGraphicsPipeline *pipeline, const EgGpusBuffer *vertex_buffer, const EgGpusBuffer *index_buffer, const EgGpusBuffer *widget_buffer, const EgGpusTexture *texture_array, const EgGpusSampler *sampler)
{
	int32_t index_count = ecs_vec_count(&drawlist->indices);
	if (index_count == 0) {
		return;
	}

	SDL_BindGPUGraphicsPipeline(render_pass, pipeline->object);

	SDL_GPUBufferBinding vertex_binding = {.buffer = vertex_buffer->object, .offset = 0};
	SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_binding, 1);

	SDL_GPUBufferBinding index_binding = {.buffer = index_buffer->object, .offset = 0};
	SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

	SDL_GPUBuffer *widget_storage_buffer = widget_buffer->object;
	SDL_BindGPUVertexStorageBuffers(render_pass, 0, &widget_storage_buffer, 1);

	SDL_GPUTextureSamplerBinding texture_sampler_binding = {.texture = texture_array->object, .sampler = sampler->object};
	SDL_BindGPUFragmentSamplers(render_pass, 0, &texture_sampler_binding, 1);

	SDL_DrawGPUIndexedPrimitives(render_pass, (Uint32)index_count, 1, 0, 0, 0);
}
