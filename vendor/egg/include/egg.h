#pragma once
#include <flecs.h>

typedef struct egg_render_t egg_render_t;

// GL renderer: owns shaders, buffers and the glyph atlas. Needs a current GL context.
egg_render_t *egg_render_init(void);

void egg_render_destroy(egg_render_t *render);

// Draws every vertex vector in `layers` in ascending z order. Does not modify the vector.
void egg_flush(egg_render_t *render, const ecs_vec_t *layers, const float *projectionMatrix);
