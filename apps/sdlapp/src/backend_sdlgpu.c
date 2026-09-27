#include "backend_sdlgpu.h"
#include <EgGpusSdl.h>
#include <stdint.h>

bool backend_sdlgpu_upload(const eg_drawlist_t *drawlist, const EgGpusDevice *device, const EgGpusBuffer *vertex_buffer, const EgGpusBuffer *index_buffer)
{
	int32_t vertex_count = ecs_vec_count(&drawlist->vertices);
	int32_t index_count   = ecs_vec_count(&drawlist->indices);
	if (vertex_count == 0 || index_count == 0) {
		return true; // Nothing to draw this frame.
	}

	bool ok = EgGpusSdlUploadBuffer(device, vertex_buffer, ecs_vec_first(&drawlist->vertices), (uint32_t)(vertex_count * (int32_t)sizeof(eg_drawvert_t)));
	ok      = ok && EgGpusSdlUploadBuffer(device, index_buffer, ecs_vec_first(&drawlist->indices), (uint32_t)(index_count * (int32_t)sizeof(uint32_t)));
	return ok;
}

static void backend_eg_drawcmd_run(const eg_drawcmd_t *drawcmd, SDL_GPURenderPass *render_pass, SDL_GPUSampler *sampler)
{
	SDL_Rect scissor_rect = {};
	scissor_rect.x        = (int)drawcmd->clip[0];
	scissor_rect.y        = (int)drawcmd->clip[1];
	scissor_rect.w        = (int)(drawcmd->clip[2] - drawcmd->clip[0]);
	scissor_rect.h        = (int)(drawcmd->clip[3] - drawcmd->clip[1]);
	SDL_SetGPUScissor(render_pass, &scissor_rect);

	SDL_GPUTextureSamplerBinding texture_sampler_binding;
	texture_sampler_binding.texture = (SDL_GPUTexture *)(intptr_t)drawcmd->texture;
	texture_sampler_binding.sampler = sampler;
	SDL_BindGPUFragmentSamplers(render_pass, 0, &texture_sampler_binding, 1);

	SDL_DrawGPUIndexedPrimitives(render_pass, drawcmd->element_count, 1, drawcmd->index_offset, (int32_t)drawcmd->vertex_offset, 0);
}

void backend_sdlgpu_draw(const eg_drawlist_t *drawlist, SDL_GPURenderPass *render_pass, const EgGpusGraphicsPipeline *pipeline, const EgGpusBuffer *vertex_buffer, const EgGpusBuffer *index_buffer, const EgGpusSampler *sampler)
{
	int32_t cmd_count = ecs_vec_count(&drawlist->cmds);
	if (cmd_count == 0) {
		return;
	}

	SDL_BindGPUGraphicsPipeline(render_pass, pipeline->object);

	SDL_GPUBufferBinding vertex_binding = {.buffer = vertex_buffer->object, .offset = 0};
	SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_binding, 1);

	SDL_GPUBufferBinding index_binding = {.buffer = index_buffer->object, .offset = 0};
	SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

	const eg_drawcmd_t *cmds = ecs_vec_first(&drawlist->cmds);
	for (int32_t i = 0; i < cmd_count; i++) {
		backend_eg_drawcmd_run(&cmds[i], render_pass, sampler->object);
	}
}

