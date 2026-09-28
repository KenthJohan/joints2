#include "eg_drawlist.h"
#include <string.h>

void eg_drawlist_init(eg_drawlist_t *drawlist)
{
	ecs_vec_init_t(NULL, &drawlist->widgets, eg_widget_data_t, 0);
	ecs_vec_init_t(NULL, &drawlist->indices, uint32_t, 0);
	ecs_vec_init_t(NULL, &drawlist->vertices, eg_drawvert_t, 0);
}

void eg_drawlist_fini(eg_drawlist_t *drawlist)
{
	ecs_vec_fini_t(NULL, &drawlist->widgets, eg_widget_data_t);
	ecs_vec_fini_t(NULL, &drawlist->indices, uint32_t);
	ecs_vec_fini_t(NULL, &drawlist->vertices, eg_drawvert_t);
}

void eg_drawlist_new_widget(eg_drawlist_t *drawlist, float clip[4], float color[4], uint32_t texture_layer)
{
	ecs_assert(texture_layer < EG_DRAWLIST_MAX_TEXTURE_LAYERS, ECS_INVALID_PARAMETER, "drawlist texture layer is out of range");

	eg_widget_data_t *widget = ecs_vec_grow_t(NULL, &drawlist->widgets, eg_widget_data_t, 1);

	widget[0] = (eg_widget_data_t){.texture_layer = texture_layer};
	memcpy(widget->clip, clip, sizeof(widget->clip));
	memcpy(widget->color, color, sizeof(widget->color));
}

void eg_drawlist_add_rect(eg_drawlist_t *drawlist, float x1, float y1, float x2, float y2)
{
	ecs_assert(ecs_vec_count(&drawlist->widgets) > 0, ECS_INVALID_PARAMETER, "eg_drawlist_new_widget must be called before eg_drawlist_add_rect");

	// Get the index of the current widget.
	uint32_t index = (uint32_t)ecs_vec_count(&drawlist->widgets) - 1;

	// Get the base index for the new rectangle's vertices.
	uint32_t base = (uint32_t)ecs_vec_count(&drawlist->vertices);

	// Grow the vertex and index buffers to accommodate the new rectangle.
	eg_drawvert_t *v = ecs_vec_grow_t(NULL, &drawlist->vertices, eg_drawvert_t, 4);
	uint32_t      *x = ecs_vec_grow_t(NULL, &drawlist->indices, uint32_t, 6);

	// Define the four vertices of the rectangle.
	v[0] = (eg_drawvert_t){.pos = {x1, y1}, .uv = {0.0f, 0.0f}, .index = index};
	v[1] = (eg_drawvert_t){.pos = {x2, y1}, .uv = {1.0f, 0.0f}, .index = index};
	v[2] = (eg_drawvert_t){.pos = {x2, y2}, .uv = {1.0f, 1.0f}, .index = index};
	v[3] = (eg_drawvert_t){.pos = {x1, y2}, .uv = {0.0f, 1.0f}, .index = index};

	// Define the two triangles that make up the rectangle.
	x[0] = base + 0;
	x[1] = base + 1;
	x[2] = base + 2;
	x[3] = base + 2;
	x[4] = base + 3;
	x[5] = base + 0;
}

void eg_drawlist_reset(eg_drawlist_t *drawlist)
{
	ecs_vec_clear(&drawlist->widgets);
	ecs_vec_clear(&drawlist->indices);
	ecs_vec_clear(&drawlist->vertices);
}
