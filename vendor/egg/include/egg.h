#pragma once
#include <flecs.h>

typedef struct egg_render_t egg_render_t;
typedef struct egg_pick_target_t egg_pick_target_t;
typedef struct egg_pick_async_t egg_pick_async_t;
typedef bool (*egg_pick_filter_t)(uint32_t id, void *ctx);
typedef uint64_t (*egg_pick_resolve_t)(uint32_t id, void *ctx);

// GL renderer: owns shaders, buffers and the glyph atlas. Needs a current GL context.
egg_render_t *egg_render_init(void);

void egg_render_destroy(egg_render_t *render);

// Draws every vertex vector in `layers` in ascending z order. Does not modify the vector.
void egg_draw_layers(egg_render_t *render, const ecs_vec_t *layers, const float *projectionMatrix);

/* Asynchronous single-pixel picking, requiring 64-bit buffer atomic support.
 * All calls require the owning GL context to be current. */
egg_pick_async_t *egg_pick_async_create(void);
void egg_pick_async_destroy(egg_pick_async_t *pick);
/* Returns 1 when started, 0 when all slots are busy, or -1 on error. */
int egg_pick_async_begin(egg_pick_async_t *pick, int x, int y, uint64_t frame);
bool egg_pick_async_draw(egg_pick_async_t *pick, egg_render_t *render,
	const ecs_vec_t *layers, const float *projectionMatrix,
	egg_pick_resolve_t resolve, void *ctx);
bool egg_pick_async_end(egg_pick_async_t *pick);
void egg_pick_async_complete(egg_pick_async_t *pick, bool displayed);
/* Returns 1 for a completed result, 0 if none is ready, or -1 on error. */
int egg_pick_async_poll(egg_pick_async_t *pick, uint64_t *entity, uint64_t *frame);
void egg_pick_async_invalidate(egg_pick_async_t *pick);

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
