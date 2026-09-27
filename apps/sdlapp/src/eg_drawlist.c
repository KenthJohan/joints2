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
	ecs_assert(texture_layer < EG_DRAWLIST_MAX_TEXTURE_LAYERS, ECS_INVALID_PARAMETER,
	"drawlist texture layer is out of range");

	eg_widget_data_t *widget = ecs_vec_append_t(NULL, &drawlist->widgets, eg_widget_data_t);
	*widget                  = (eg_widget_data_t){.texture_index = texture_layer};
	memcpy(widget->clip_rect, clip, sizeof(widget->clip_rect));
	memcpy(widget->color, color, sizeof(widget->color));
}

void eg_drawlist_add_rect(eg_drawlist_t *drawlist, float x1, float y1, float x2, float y2)
{
	ecs_assert(ecs_vec_count(&drawlist->widgets) > 0, ECS_INVALID_PARAMETER,
	"eg_drawlist_new_widget must be called before eg_drawlist_add_rect");

	uint32_t widget_index = (uint32_t)ecs_vec_count(&drawlist->widgets) - 1;
	uint32_t base_vertex  = (uint32_t)ecs_vec_count(&drawlist->vertices);

	eg_drawvert_t *v   = ecs_vec_grow_t(NULL, &drawlist->vertices, eg_drawvert_t, 4);
	uint32_t      *idx = ecs_vec_grow_t(NULL, &drawlist->indices, uint32_t, 6);

	v[0] = (eg_drawvert_t){.pos = {x1, y1}, .uv = {0.0f, 0.0f}, .widget_index = widget_index};
	v[1] = (eg_drawvert_t){.pos = {x2, y1}, .uv = {1.0f, 0.0f}, .widget_index = widget_index};
	v[2] = (eg_drawvert_t){.pos = {x2, y2}, .uv = {1.0f, 1.0f}, .widget_index = widget_index};
	v[3] = (eg_drawvert_t){.pos = {x1, y2}, .uv = {0.0f, 1.0f}, .widget_index = widget_index};

	idx[0] = base_vertex + 0;
	idx[1] = base_vertex + 1;
	idx[2] = base_vertex + 2;
	idx[3] = base_vertex + 2;
	idx[4] = base_vertex + 3;
	idx[5] = base_vertex + 0;
}

void eg_drawlist_reset(eg_drawlist_t *drawlist)
{
	ecs_vec_clear(&drawlist->widgets);
	ecs_vec_clear(&drawlist->indices);
	ecs_vec_clear(&drawlist->vertices);
}
