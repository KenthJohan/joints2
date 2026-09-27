#pragma once
#include <flecs.h>

typedef struct {
	float    clip[4];       // (x1, y1, x2, y2) screen coordinates for the clipping rectangle
	uint64_t texture;       // texture to use for this draw command
	uint32_t vertex_offset; // Offset into the vertex buffer for this draw command
	uint32_t index_offset;  // Offset into the index buffer for this draw command
	uint32_t element_count; // Number of elements (indices) for this draw command
} eg_drawcmd_t;

// Layout must match the `Vertex123` struct declared in config/windows.flecs.
typedef struct {
	float   pos[2]; // (x, y) screen coordinates
	float   uv[2];  // (u, v)
	uint8_t col[4]; // (r, g, b, a)
} eg_drawvert_t;

typedef struct {
	ecs_vec_t cmds;     // cmds<eg_drawcmd_t>
	ecs_vec_t indices;  // vertices<uint32_t>
	ecs_vec_t vertices; // vertices<eg_drawvert_t>
} eg_drawlist_t;

void eg_drawlist_init(eg_drawlist_t *drawlist);

void eg_drawlist_fini(eg_drawlist_t *drawlist);

void eg_drawlist_new_cmd(eg_drawlist_t *drawlist, float clip[4], uint64_t texture);

void eg_drawlist_add_rect(eg_drawlist_t *drawlist, float x1, float y1, float x2, float y2);

void eg_drawlist_reset(eg_drawlist_t *drawlist);
