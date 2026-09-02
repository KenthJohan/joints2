#include "isa_internal.h"

/** `isa_interface_t` get_type handler for `IsaTextStream`: accepts any type. */
ecs_entity_t IsaInterface_get_type_stream(
	ecs_world_t  *world,
	ecs_entity_t  entity)
{
	(void)world;
	(void)entity;
	return 0;
}

/** `isa_interface_t` write handler for `IsaTextStream`: prints `value` with an incrementing counter.
 * `type` 0 means `value` is CONST literal text, otherwise it's a raw component value. */
bool IsaInterface_write_stream(
	ecs_world_t  *world,
	ecs_entity_t  entity,
	ecs_entity_t  type,
	void         *value)
{
	if (!ecs_has(world, entity, IsaTextStream)) {
		return false;
	}

	IsaTextStream *stream = ecs_ensure(world, entity, IsaTextStream);
	FILE          *file   = stream->file;

	if (file == NULL) {
		return true;
	}

	if (type == 0) {
		fprintf(file, "[%d] %s\n", stream->counter++, (const char *)value);
	} else {
		char *json = ecs_ptr_to_json(world, type, value);
		fprintf(file, "[%d] %s\n", stream->counter++, json ? json : "?");
		ecs_os_free(json);
	}

	ecs_modified(world, entity, IsaTextStream);
	return true;
}
