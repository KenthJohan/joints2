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
// Coordinates use a top-left origin. Clear and restrict subsequent draws to a
// clipped 3x3 region around (x, y), without changing the full-size viewport.
bool egg_pick_target_begin_region(egg_pick_target_t *target, int width, int height, int x, int y);
// The framebuffer has an RG32UI color attachment (ID, draw order) and a depth attachment.
// Returns 0 until begin succeeds; external renderers must restore GL state.
uint32_t egg_pick_target_framebuffer(const egg_pick_target_t *target);
// filter may be NULL when the caller has already filtered pick IDs.
bool egg_pick_draw_layers(egg_pick_target_t *target, egg_render_t *render,
	const ecs_vec_t *layers, const float *projectionMatrix, egg_pick_filter_t filter, void *ctx);
// A region target can only be read inside its most recently cleared region.
bool egg_pick_target_read(egg_pick_target_t *target, int x, int y, uint32_t *id);
// Center wins; otherwise the closest pixel wins, with later primitives breaking ties.
bool egg_pick_target_read_region(egg_pick_target_t *target, uint32_t *id);
