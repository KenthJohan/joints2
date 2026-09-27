#include "eg_drawlist.h"

void eg_drawlist_init(eg_drawlist_t *drawlist)
{
	ecs_vec_init_t(NULL, &drawlist->cmds, eg_drawcmd_t, 0);
	ecs_vec_init_t(NULL, &drawlist->indices, uint32_t, 0);
	ecs_vec_init_t(NULL, &drawlist->vertices, eg_drawvert_t, 0);
}

void eg_drawlist_fini(eg_drawlist_t *drawlist)
{
	ecs_vec_fini_t(NULL, &drawlist->cmds, eg_drawcmd_t);
	ecs_vec_fini_t(NULL, &drawlist->indices, uint32_t);
	ecs_vec_fini_t(NULL, &drawlist->vertices, eg_drawvert_t);
}

void eg_drawlist_new_cmd(eg_drawlist_t *drawlist, float clip[4], uint64_t texture)
{
	eg_drawcmd_t *cmd = ecs_vec_append_t(NULL, &drawlist->cmds, eg_drawcmd_t);
	cmd->clip[0]       = clip[0];
	cmd->clip[1]       = clip[1];
	cmd->clip[2]       = clip[2];
	cmd->clip[3]       = clip[3];
	cmd->texture       = texture;
	cmd->vertex_offset = (uint32_t)ecs_vec_count(&drawlist->vertices);
	cmd->index_offset  = (uint32_t)ecs_vec_count(&drawlist->indices);
	cmd->element_count = 0;
}

void eg_drawlist_add_rect(eg_drawlist_t *drawlist, float x1, float y1, float x2, float y2)
{
	ecs_assert(ecs_vec_count(&drawlist->cmds) > 0, ECS_INVALID_PARAMETER,
	"eg_drawlist_new_cmd must be called before eg_drawlist_add_rect");
	eg_drawcmd_t *cmd = ecs_vec_get_t(&drawlist->cmds, eg_drawcmd_t, ecs_vec_count(&drawlist->cmds) - 1);

	uint32_t base_vertex = (uint32_t)ecs_vec_count(&drawlist->vertices);

	eg_drawvert_t *v = ecs_vec_append_t(NULL, &drawlist->vertices, eg_drawvert_t);
	*v               = (eg_drawvert_t){.pos = {x1, y1}, .uv = {0.0f, 0.0f}, .col = {255, 255, 255, 255}};
	v                = ecs_vec_append_t(NULL, &drawlist->vertices, eg_drawvert_t);
	*v               = (eg_drawvert_t){.pos = {x2, y1}, .uv = {1.0f, 0.0f}, .col = {255, 255, 255, 255}};
	v                = ecs_vec_append_t(NULL, &drawlist->vertices, eg_drawvert_t);
	*v               = (eg_drawvert_t){.pos = {x2, y2}, .uv = {1.0f, 1.0f}, .col = {255, 255, 255, 255}};
	v                = ecs_vec_append_t(NULL, &drawlist->vertices, eg_drawvert_t);
	*v               = (eg_drawvert_t){.pos = {x1, y2}, .uv = {0.0f, 1.0f}, .col = {255, 255, 255, 255}};

	uint32_t *idx = ecs_vec_append_t(NULL, &drawlist->indices, uint32_t);
	*idx          = base_vertex + 0;
	idx           = ecs_vec_append_t(NULL, &drawlist->indices, uint32_t);
	*idx          = base_vertex + 1;
	idx           = ecs_vec_append_t(NULL, &drawlist->indices, uint32_t);
	*idx          = base_vertex + 2;
	idx           = ecs_vec_append_t(NULL, &drawlist->indices, uint32_t);
	*idx          = base_vertex + 2;
	idx           = ecs_vec_append_t(NULL, &drawlist->indices, uint32_t);
	*idx          = base_vertex + 3;
	idx           = ecs_vec_append_t(NULL, &drawlist->indices, uint32_t);
	*idx          = base_vertex + 0;

	cmd->element_count += 6;
}

void eg_drawlist_reset(eg_drawlist_t *drawlist)
{
	ecs_vec_clear(&drawlist->cmds);
	ecs_vec_clear(&drawlist->indices);
	ecs_vec_clear(&drawlist->vertices);
}
