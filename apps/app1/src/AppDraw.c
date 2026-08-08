#include "AppDraw.h"
#include <EgWindows.h>
#include <EgCameras.h>
#include <EgSpatials.h>
#include <EgShapes.h>
#include <EgBase.h>
#include <ecsx.h>
#include <draw.h>
#include <egmisc/eg_file.h>

ECS_COMPONENT_DECLARE(AppDrawContext);
ECS_COMPONENT_DECLARE(AppDrawContextCreate);
ECS_COMPONENT_DECLARE(AppDrawNameAtPositionRule);

static bool BuildDrawCreateInfo(draw_create_info_t *createInfo)
{
	*createInfo = (draw_create_info_t){0};

	createInfo->shaders[DRAW_SHADER_BACKGROUND_VERTEX]      = eg_file_load_alloc("data/background.vs", NULL);
	createInfo->shaders[DRAW_SHADER_BACKGROUND_FRAGMENT]    = eg_file_load_alloc("data/background.fs", NULL);
	createInfo->shaders[DRAW_SHADER_POINT_VERTEX]           = eg_file_load_alloc("data/point.vs", NULL);
	createInfo->shaders[DRAW_SHADER_POINT_FRAGMENT]         = eg_file_load_alloc("data/point.fs", NULL);
	createInfo->shaders[DRAW_SHADER_LINE_VERTEX]            = eg_file_load_alloc("data/line.vs", NULL);
	createInfo->shaders[DRAW_SHADER_LINE_FRAGMENT]          = eg_file_load_alloc("data/line.fs", NULL);
	createInfo->shaders[DRAW_SHADER_CIRCLE_VERTEX]          = eg_file_load_alloc("data/circle.vs", NULL);
	createInfo->shaders[DRAW_SHADER_CIRCLE_FRAGMENT]        = eg_file_load_alloc("data/circle.fs", NULL);
	createInfo->shaders[DRAW_SHADER_SOLID_CIRCLE_VERTEX]    = eg_file_load_alloc("data/solid_circle.vs", NULL);
	createInfo->shaders[DRAW_SHADER_SOLID_CIRCLE_FRAGMENT]  = eg_file_load_alloc("data/solid_circle.fs", NULL);
	createInfo->shaders[DRAW_SHADER_SOLID_CAPSULE_VERTEX]   = eg_file_load_alloc("data/solid_capsule.vs", NULL);
	createInfo->shaders[DRAW_SHADER_SOLID_CAPSULE_FRAGMENT] = eg_file_load_alloc("data/solid_capsule.fs", NULL);
	createInfo->shaders[DRAW_SHADER_SOLID_POLYGON_VERTEX]   = eg_file_load_alloc("data/solid_polygon.vs", NULL);
	createInfo->shaders[DRAW_SHADER_SOLID_POLYGON_FRAGMENT] = eg_file_load_alloc("data/solid_polygon.fs", NULL);
	createInfo->shaders[DRAW_SHADER_TEXT_VERTEX]            = eg_file_load_alloc("data/text.vs", NULL);
	createInfo->shaders[DRAW_SHADER_TEXT_FRAGMENT]          = eg_file_load_alloc("data/text.fs", NULL);

	for (int i = 0; i < DRAW_SHADER_COUNT; ++i) {
		if (createInfo->shaders[i] == NULL) {
			fprintf(stderr, "Failed to load one or more shader files from apps/app1/data or data\n");
			return false;
		}
	}

	return true;
}

static void FreeDrawCreateInfo(draw_create_info_t *createInfo)
{
	for (int i = 0; i < DRAW_SHADER_COUNT; ++i) {
		free((void *)createInfo->shaders[i]);
	}
	*createInfo = (draw_create_info_t){0};
}

static void Test_Render(ecs_iter_t *it)
{
	AppDrawContext *draw   = ecs_field_self(it, AppDrawContext, 1);
	EgCamerasState *camera = ecs_field_shared(it, EgCamerasState, 2);
	for (int i = 0; i < it->count; ++i, ++draw) {
		// Placeholder for rendering logic. This function will be called every frame to handle rendering tasks.
		// printf("Test_Render called with %d entities\n", it->count);

		/*
		b2WorldTransform transform = {{1.0f, 0.0f}, {1.0f, 0.0f}};
		draw_solid_circle(draw->draw, transform, (b2Pos){0.0f, 0.0f}, 10.0f, b2_colorRed);
		*/

		float pixelScale = 100.1f; // Placeholder for pixel scale, can be adjusted based on window size or other factors
		draw_flush(draw->draw, pixelScale, (float *)&camera->vp);
	}
}

static void AppDrawContext_Create(ecs_iter_t *it)
{
	ecs_log_set_level(0);
	AppDrawContextCreate *def      = ecs_field(it, AppDrawContextCreate, 0); // self
	ecs_entity_t          e_window = ecs_field_src(it, 1);
	printf("window_entity: %s\n", ecs_get_name(it->world, e_window));
	for (int i = 0; i < it->count; ++i, ++def) {
		draw_create_info_t drawCreateInfo;
		if (!BuildDrawCreateInfo(&drawCreateInfo)) {
			continue; // Skip this entity if shader loading failed
		}
		draw_t *draw = draw_init(&drawCreateInfo);
		FreeDrawCreateInfo(&drawCreateInfo);

		ecs_set(it->world, it->entities[i], AppDrawContext, {draw});

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
		draw_string(d->draw, p->x, p->y, 0.5f, b->color, "%s", name);
	}
}

static void AppDrawText_Draw(ecs_iter_t *it)
{
	AppDrawContext   *d   = ecs_field_shared(it, AppDrawContext, 0);
	EgCamerasState   *cam = ecs_field_shared(it, EgCamerasState, 1);
	Position2        *p   = ecs_field_self(it, Position2, 2);
	EgBaseText       *t   = ecs_field_self(it, EgBaseText, 3);
	EgBaseFont       *f   = ecs_field_self(it, EgBaseFont, 4);
	EgShapesRectangle *r  = ecs_field_shared(it, EgShapesRectangle, 5);

	for (int i = 0; i < it->count; ++i, ++p, ++t, ++f) {
		if (t->value && t->value[0] != '\0') {
			float font_size = f->font_size > 0.0f ? f->font_size : 24.0f;
			if (cam->pixel_coords) {
				float x = p->x - (0.5f * r->w);
				float y = (0.5f * r->h) - p->y;
				draw_string(d->draw, x, y, font_size, 0xFFFFFFFFu, "%s", t->value);
			} else {
				draw_string(d->draw, p->x, p->y, font_size, 0xFFFFFFFFu, "%s", t->value);
			}
		}
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
	{.name = "draw", .type = ecs_id(ecs_uptr_t)},
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
	{.id = ecs_id(Position2), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgBaseText), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgBaseFont), .src.id = EcsSelf, .inout = EcsIn},
	{.id = ecs_id(EgShapesRectangle), .trav = EcsDependsOn, .src.id = EcsUp, .inout = EcsIn},
	}});

	ecs_observer(world,
	{.query   = {.terms = {{.id = ecs_id(AppDrawNameAtPositionRule)}}},
	.events   = {EcsOnSet},
	.callback = AppDrawNameAtPositionRule_Observer});
}
