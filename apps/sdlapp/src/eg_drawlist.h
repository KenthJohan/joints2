#pragma once
#include <flecs.h>

#define EG_DRAWLIST_MAX_TEXTURE_LAYERS 16

typedef struct {
	float    color[4];
	float    clip_rect[4];
	uint32_t texture_index;
	uint32_t padding[3];
} eg_widget_data_t;

typedef struct {
	float   pos[2]; // (x, y) screen coordinates
	float   uv[2];  // (u, v)
	uint32_t widget_index;
} eg_drawvert_t;

typedef struct {
	ecs_vec_t widgets;  // widgets<eg_widget_data_t>
	ecs_vec_t indices;  // indices<uint32_t>
	ecs_vec_t vertices; // vertices<eg_drawvert_t>
} eg_drawlist_t;

void eg_drawlist_init(eg_drawlist_t *drawlist);

void eg_drawlist_fini(eg_drawlist_t *drawlist);

void eg_drawlist_new_widget(eg_drawlist_t *drawlist, float clip[4], float color[4], uint32_t texture_layer);

void eg_drawlist_add_rect(eg_drawlist_t *drawlist, float x1, float y1, float x2, float y2);

void eg_drawlist_reset(eg_drawlist_t *drawlist);
