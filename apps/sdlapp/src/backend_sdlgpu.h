#pragma once
#include "eg_drawlist.h"
#include <EgGpus.h>
#include <SDL3/SDL_gpu.h>

bool backend_sdlgpu_upload(const eg_drawlist_t *drawlist, const EgGpusDevice *device, const EgGpusBuffer *vertex_buffer, const EgGpusBuffer *index_buffer, const EgGpusBuffer *widget_buffer);
void backend_sdlgpu_draw(const eg_drawlist_t *drawlist, SDL_GPURenderPass *render_pass, const EgGpusGraphicsPipeline *pipeline, const EgGpusBuffer *vertex_buffer, const EgGpusBuffer *index_buffer, const EgGpusBuffer *widget_buffer, const EgGpusTexture *texture_array, const EgGpusSampler *sampler);

