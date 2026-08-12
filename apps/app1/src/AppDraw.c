#include "AppDraw.h"
#include <assert.h>
#include <EgWindows.h>
#include <EgCameras.h>
#include <EgSpatials.h>
#include <EgShapes.h>
#include <EgBase.h>
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
	for (int i = 0; i < it->count; ++i, ++draw) {
		// Placeholder for rendering logic. This function will be called every frame to handle rendering tasks.
		// printf("Test_Render called with %d entities\n", it->count);

		draw->pixelScale = camera->pixelScale * 1.0f; // Keep the egg-scale near the camera-derived size.
		if (draw->egg != NULL) {
			egg_set_pixel_scale(draw->egg, draw->pixelScale);
			egg_flush(draw->egg, (float *)&camera->vp);
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
		egg_t *egg = egg_init();
		if (egg == NULL) {
			continue;
		}

		ecs_set(it->world, it->entities[i], AppDrawContext, {egg, 1.0f});

		// The window system will call this render system using `ecs_run()` every frame
		// by putting it as a child of the window entity.
		ecs_system(it->world,
		{.entity     = ecs_entity(it->world, {.parent = e_window}),
		.callback    = Test_Render,
		.query.terms = {
		{.id = ecs_childof(e_window)},
		{.id = ecs_id(AppDrawContext), .src.id = EcsSelf, .inout = EcsIn},
		{.id = ecs_id(EgCamerasState), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
		}});
	}
	ecs_log_set_level(-1);
}

void AppDrawNameAtPosition_Draw(ecs_iter_t *it)
{
	AppDrawContext            *d = ecs_field_shared(it, AppDrawContext, 0);
	Position2                 *p = ecs_field_self(it, Position2, 1);
	AppDrawNameAtPositionRule *b = ecs_field_shared(it, AppDrawNameAtPositionRule, 2);
	for (int i = 0; i < it->count; ++i, ++p) {
		char const *name = ecs_get_name(it->world, it->entities[i]);
		assert(d->egg != NULL);
		egg_draw_text(d->egg, p->x, p->y, 1.0f, 0.0f, 0.5f, b->color, name);
	}
}

static void AppDrawText_Draw(ecs_iter_t *it)
{
	AppDrawContext    *d   = ecs_field_shared(it, AppDrawContext, 0);
	EgCamerasState    *cam = ecs_field_shared(it, EgCamerasState, 1);
	Transformation    *p   = ecs_field_self(it, Transformation, 2);
	EgBaseText        *t   = ecs_field_self(it, EgBaseText, 3);
	EgBaseFont        *f   = ecs_field_self(it, EgBaseFont, 4);
	EgShapesRectangle *r   = ecs_field_shared(it, EgShapesRectangle, 5);

	for (int i = 0; i < it->count; ++i, ++p, ++t, ++f) {
		if (t->value == NULL) {
			continue; // Skip empty strings
		}
		if (t->value[0] == '\0') {
			continue; // Skip empty strings
		}
		float font_size = f->font_size > 0.0f ? f->font_size : 24.0f;
		assert(d->egg != NULL);
		float x = p->matrix.c3[0];
		float y = p->matrix.c3[1];
		float c = p->matrix.c0[1]; // Rotation cosine, TODO: Check why this is c0[1] and not c0[0] for cosine
		float s = p->matrix.c0[0]; // Rotation sine
		egg_draw_rectangle(d->egg, x, y, c, s, 10, 10, 0x0066FF00u);
		egg_draw_text(d->egg, x, y, c, s, font_size, 0xFFFFFFFFu, t->value);
	}
}

static void AppDrawShapesRectangle_Draw(ecs_iter_t *it)
{
	AppDrawContext    *d = ecs_field_shared(it, AppDrawContext, 0);
	EgShapesRectangle *r = ecs_field_self(it, EgShapesRectangle, 1);
	Transformation    *p = ecs_field_self(it, Transformation, 2);
	for (int i = 0; i < it->count; ++i, ++r, ++p) {
		float x = p->matrix.c3[0];
		float y = p->matrix.c3[1];
		float c = p->matrix.c0[1];
		float s = p->matrix.c0[0];
		egg_draw_rectangle(d->egg, x, y, c, s, r->w, r->h, 0x00FFFF00u);
		egg_draw_text(d->egg, x, y, c, s, 24.0f, 0xFFFFFFFFu, "Rectangle");
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
			.phase       = EcsOnUpdate,
			.callback    = AppDrawNameAtPosition_Draw,
			.query.terms = {
			{.id = ecs_id(AppDrawContext), .src.id = o->draw_e, .inout = EcsIn},
			{.id = ecs_id(Position2), .src.id = EcsSelf},
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
	{.name = "egg", .type = ecs_id(ecs_uptr_t)},
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
	.phase       = EcsOnUpdate,
	.callback    = AppDrawText_Draw,
	.query.terms = {
	{.id = ecs_id(AppDrawContext), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(EgCamerasState), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(Transformation), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgBaseText), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgBaseFont), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgShapesRectangle), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
	}});

	ecs_system(world,
	{.entity     = ecs_entity(world, {.name = "AppDrawShapesRectangle_Draw"}),
	.phase       = EcsOnUpdate,
	.callback    = AppDrawShapesRectangle_Draw,
	.query.terms = {
	{.id = ecs_id(AppDrawContext), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
	{.id = ecs_id(EgShapesRectangle), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(Transformation), .src.id = EcsSelf, .inout = EcsIn},
	}});

	ecs_observer(world,
	{.query   = {.terms = {{.id = ecs_id(AppDrawNameAtPositionRule)}}},
	.events   = {EcsOnSet},
	.callback = AppDrawNameAtPositionRule_Observer});
}
