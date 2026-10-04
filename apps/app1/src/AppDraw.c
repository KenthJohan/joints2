#include "AppDraw.h"
#include <assert.h>
#include <EgWindows.h>
#include <EgCameras.h>
#include <EgSpatials.h>
#include <EgShapes.h>
#include <EgBase.h>
#include <EgShapedraw.h>
#include <ecsx.h>
#include <egg.h>
#include <math.h>

ECS_COMPONENT_DECLARE(AppDrawContext);
ECS_COMPONENT_DECLARE(AppDrawContextCreate);
ECS_COMPONENT_DECLARE(AppDrawNameAtPositionRule);

static void Test_Render(ecs_iter_t *it)
{
	AppDrawContext *draw   = ecs_field_self(it, AppDrawContext, 1);
	EgCamerasState *camera = ecs_field_shared(it, EgCamerasState, 2);
	EgShapedrawList *shapes = ecs_field_self(it, EgShapedrawList, 3);
	for (int i = 0; i < it->count; ++i, ++draw) {
		// Placeholder for rendering logic. This function will be called every frame to handle rendering tasks.
		// printf("Test_Render called with %d entities\n", it->count);

		draw->pixelScale = camera->pixelScale * 1.0f; // Keep the egg-scale near the camera-derived size.
		if (draw->render != NULL && draw->draw != NULL) {
			if (shapes != NULL) {
				egg_draw_append_vertices(draw->draw, APP_DRAW_Z_RECTANGLES, shapes[i].data, shapes[i].count);
			}
			egg_draw_set_pixel_scale(draw->draw, draw->pixelScale);
			egg_flush(draw->render, draw->draw, (float *)&camera->vp);
		}
	}
}

static void AppDrawContext_Create(ecs_iter_t *it)
{
	ecs_log_set_level(0);
	AppDrawContextCreate *def      = ecs_field(it, AppDrawContextCreate, 0); // self
	ecs_entity_t          e_window = ecs_field_src(it, 1);
	printf("window_entity: %s\n", ecs_get_name(it->world, e_window));
	for (int i = 0; i < it->count; ++i, ++def) {
		egg_render_t *render = egg_render_init();
		egg_draw_t   *draw   = egg_draw_create();
		if (render == NULL || draw == NULL) {
			egg_render_destroy(render);
			egg_draw_destroy(draw);
			continue;
		}

		ecs_set(it->world, it->entities[i], AppDrawContext, {render, draw, 1.0f});
		ecs_add(it->world, it->entities[i], EgShapedrawList);

		// The window system will call this render system using `ecs_run()` every frame
		// by putting it as a child of the window entity.
		ecs_system(it->world,
		{.entity     = ecs_entity(it->world, {.parent = e_window}),
		.callback    = Test_Render,
		.query.terms = {
		{.id = ecs_childof(e_window)},
		{.id = ecs_id(AppDrawContext), .src.id = EcsSelf, .inout = EcsIn},
		{.id = ecs_id(EgCamerasState), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
		{.id = ecs_id(EgShapedrawList), .src.id = EcsSelf, .inout = EcsIn, .oper = EcsOptional},
		}});
	}
	ecs_log_set_level(-1);
}

void AppDrawNameAtPosition_Draw(ecs_iter_t *it)
{
	AppDrawContext            *d  = ecs_field_shared(it, AppDrawContext, 0);
	WorldTransform3           *m3 = ecs_field_self(it, WorldTransform3, 1);
	WorldTransform4           *m4 = ecs_field_self(it, WorldTransform4, 2);
	AppDrawNameAtPositionRule *b  = ecs_field_shared(it, AppDrawNameAtPositionRule, 3);
	assert(d->draw != NULL);
	if (m3) {
		for (int i = 0; i < it->count; ++i, ++m3, ++m4) {
			char const *name = ecs_get_name(it->world, it->entities[i]);

			float x = m3->matrix.c2[0];
			float y = m3->matrix.c2[1];
			float c = m3->matrix.c0[0];
			float s = m3->matrix.c0[1];
			//printf("Drawing name '%s' at position (%f, %f) with rotation (c=%f, s=%f)\n", name, x, y, c, s);
			egg_draw_text(d->draw, APP_DRAW_Z_TEXT, x, y, c, s, 0.5f, b->color, name);
			egg_draw_rectangle_outline(d->draw, APP_DRAW_Z_SHAPES, x, y, c, s, 20, 20, 2.0f, 0x0066FF00u);
		}
	} else if (m4) {
		for (int i = 0; i < it->count; ++i, ++m4) {
			char const *name = ecs_get_name(it->world, it->entities[i]);

			float x = m4->matrix.c3[0];
			float y = m4->matrix.c3[1];
			float c = 1.0f; // Rotation cosine
			float s = 0.0f; // Rotation sine
			//printf("Drawing name '%s' at position (%f, %f) with rotation (c=%f, s=%f)\n", name, x, y, c, s);
			egg_draw_text(d->draw, APP_DRAW_Z_TEXT, x, y, c, s, 0.5f, b->color, name);
			egg_draw_rectangle_outline(d->draw, APP_DRAW_Z_SHAPES, x, y, c, s, 20, 20, 2.0f, 0x0066FF00u);
		}
	}
}

static void AppDrawText_Draw(ecs_iter_t *it)
{
	AppDrawContext    *d   = ecs_field_shared(it, AppDrawContext, 0);
	EgCamerasState    *cam = ecs_field_shared(it, EgCamerasState, 1);
	WorldTransform4   *p   = ecs_field_self(it, WorldTransform4, 2);
	EgBaseText        *t   = ecs_field_self(it, EgBaseText, 3);
	EgBaseFont        *f   = ecs_field_self(it, EgBaseFont, 4);
	EgBaseColor       *col = ecs_field_self(it, EgBaseColor, 5);
	EgShapesRectangle *r   = ecs_field_shared(it, EgShapesRectangle, 6);

	(void)r;
	(void)cam;

	for (int i = 0; i < it->count; ++i, ++p, ++t, ++f) {
		if (t->value == NULL) {
			continue; // Skip empty strings
		}
		if (t->value[0] == '\0') {
			continue; // Skip empty strings
		}
		float    font_size = f->font_size > 0.0f ? f->font_size : 24.0f;
		uint32_t color     = col != NULL ? col[i].color : 0xFFFFFFFFu;
		assert(d->draw != NULL);
		float x = p->matrix.c3[0];
		float y = p->matrix.c3[1];
		float c = p->matrix.c0[0]; // Rotation cosine
		float s = p->matrix.c0[1]; // Rotation sine
		egg_draw_rectangle(d->draw, APP_DRAW_Z_SHAPES, x, y, c, s, 10, 10, 0x0066FF00u);
		egg_draw_text(d->draw, APP_DRAW_Z_TEXT, x, y, c, s, font_size, color, t->value);
	}
}

static void AppDrawShapesRectangle_Draw2D(ecs_iter_t *it)
{
	AppDrawContext    *d   = ecs_field_shared(it, AppDrawContext, 0);
	EgShapesRectangle *r   = ecs_field_self(it, EgShapesRectangle, 1);
	WorldTransform3  *p   = ecs_field_self(it, WorldTransform3, 2);
	EgBaseColor       *col = ecs_field_self(it, EgBaseColor, 3);
	(void)r;
	(void)col;
	for (int i = 0; i < it->count; ++i, ++p) {
		float    x     = p->matrix.c2[0];
		float    y     = p->matrix.c2[1];
		float    c     = p->matrix.c0[0];
		float    s     = p->matrix.c0[1];
		egg_draw_rectangle(d->draw, APP_DRAW_Z_SHAPES, x, y, c, s, 10, 10, 0x0000000FF);
		egg_draw_text(d->draw, APP_DRAW_Z_TEXT, x, y, c, s, 12.0f, 0xFFFFFFFFu, "Debug");
	}
}

void AppDrawNameAtPositionRule_Observer(ecs_iter_t *it)
{
	AppDrawNameAtPositionRule *o = ecs_field_self(it, AppDrawNameAtPositionRule, 0);

	for (int i = 0; i < it->count; i++) {
		ecs_entity_t e    = it->entities[i];
		char const  *name = ecs_get_name(it->world, e);
		char         buffer[256];
		snprintf(buffer, sizeof(buffer), "sys_%s_%s", name, ecs_get_name(it->world, o->term));
		if (it->event == EcsOnSet) {
			ecs_system(it->world,
			{.entity     = ecs_entity(it->world, {.name = buffer}),
			.phase       = EcsPostUpdate,
			.callback    = AppDrawNameAtPosition_Draw,
			.query.terms = {
			{.id = ecs_id(AppDrawContext), .src.id = o->draw_e, .inout = EcsIn},
			{.id = ecs_id(WorldTransform3), .src.id = EcsSelf, .inout = EcsIn, .oper = EcsOptional},
			{.id = ecs_id(WorldTransform4), .src.id = EcsSelf, .inout = EcsIn, .oper = EcsOptional},
			{.id = ecs_id(AppDrawNameAtPositionRule), .src.id = e},
			{.id = o->term, .src.id = EcsSelf},
			}});
		}
	}
}

void AppDrawImport(ecs_world_t *world)
{
	ECS_MODULE(world, AppDraw);
	ecs_set_name_prefix(world, "AppDraw");
	ECS_IMPORT(world, EgWindows);
	ECS_IMPORT(world, EgSpatials);
	ECS_IMPORT(world, EgBase);
	ECS_IMPORT(world, EgShapedraw);

	ECS_COMPONENT_DEFINE(world, AppDrawContext);
	ECS_COMPONENT_DEFINE(world, AppDrawContextCreate);
	ECS_COMPONENT_DEFINE(world, AppDrawNameAtPositionRule);

	ecs_struct(world,
	{.entity = ecs_id(AppDrawContextCreate),
	.members = {
	{.name = "dummy", .type = ecs_id(ecs_i32_t)},
	}});

	ecs_struct(world,
	{.entity = ecs_id(AppDrawContext),
	.members = {
	{.name = "render", .type = ecs_id(ecs_uptr_t)},
	{.name = "draw", .type = ecs_id(ecs_uptr_t)},
	{.name = "pixelScale", .type = ecs_id(ecs_f32_t)},
	}});

	ecs_struct(world,
	{.entity = ecs_id(AppDrawNameAtPositionRule),
	.members = {
	{.name = "term", .type = ecs_id(ecs_id_t)},
	{.name = "draw_e", .type = ecs_id(ecs_id_t)},
	{.name = "color", .type = ecs_id(ecs_u32_t)},
	}});

	ecs_system(world,
	{.entity     = ecs_entity(world, {.name = "AppDrawContext_Create"}),
	.phase       = EcsOnUpdate,
	.callback    = AppDrawContext_Create,
	.immediate   = true,
	.query.terms = {
	{.id = ecs_id(AppDrawContextCreate), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgWindowsOpenGLContext), .trav = EcsChildOf, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(AppDrawContext), .oper = EcsNot}, // Adds this
	}});

	ecs_system(world,
	{.entity     = ecs_entity(world, {.name = "AppDrawText_Draw"}),
	.phase       = EcsPostUpdate,
	.callback    = AppDrawText_Draw,
	.query.terms = {
	{.id = ecs_id(AppDrawContext), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(EgCamerasState), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(WorldTransform4), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgBaseText), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgBaseFont), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgBaseColor), .src.id = EcsSelf, .inout = EcsIn, .oper = EcsOptional},
	{.id = ecs_id(EgShapesRectangle), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
	}});

	ecs_system(world,
	{.entity     = ecs_entity(world, {.name = "AppDrawShapesRectangle_Draw2D"}),
	.phase       = EcsPostUpdate,
	.callback    = AppDrawShapesRectangle_Draw2D,
	.query.terms = {
	{.id = ecs_id(AppDrawContext), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(EgShapesRectangle), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(WorldTransform3), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgBaseColor), .src.id = EcsSelf, .inout = EcsIn},
	}});

	ecs_observer(world,
	{.query   = {.terms = {{.id = ecs_id(AppDrawNameAtPositionRule)}}},
	.events   = {EcsOnSet},
	.callback = AppDrawNameAtPositionRule_Observer});
}
