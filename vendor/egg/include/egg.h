#pragma once
#include <flecs.h>

typedef struct egg_render_t egg_render_t;
typedef struct egg_pick_target_t egg_pick_target_t;
typedef bool (*egg_pick_filter_t)(uint32_t id, void *ctx);

// GL renderer: owns shaders, buffers and the glyph atlas. Needs a current GL context.
egg_render_t *egg_render_init(void);

void egg_render_destroy(egg_render_t *render);

// Draws every vertex vector in `layers` in ascending z order. Does not modify the vector.
void egg_draw_layers(egg_render_t *render, const ecs_vec_t *layers, const float *projectionMatrix);

// All picking operations require the target's OpenGL context to be current.
egg_pick_target_t *egg_pick_target_create(void);
void egg_pick_target_destroy(egg_pick_target_t *target);
bool egg_pick_target_begin(egg_pick_target_t *target, int width, int height);
// The framebuffer has an integer color attachment and a depth attachment.
// Returns 0 until begin succeeds; external renderers must restore GL state.
uint32_t egg_pick_target_framebuffer(const egg_pick_target_t *target);
bool egg_pick_draw_layers(egg_pick_target_t *target, egg_render_t *render,
	const ecs_vec_t *layers, const float *projectionMatrix, egg_pick_filter_t filter, void *ctx);
bool egg_pick_target_read(egg_pick_target_t *target, int x, int y, uint32_t *id);
