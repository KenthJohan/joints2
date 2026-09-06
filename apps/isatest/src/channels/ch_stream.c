#include "../isa_internal.h"

/** `isa_channel_t` get_write_type handler for `IsaTextStream`: accepts any type. */
ecs_entity_t ch_stream_get_write_type(ecs_world_t *world, ecs_entity_t entity)
{
	(void)world;
	(void)entity;
	return 0;
}

/** `isa_channel_t` write handler for `IsaTextStream`: prints `value` with an incrementing counter.
 * `value.type` 0 means `value.ptr` is literal text, otherwise it's a raw component value. */
bool ch_stream_write(ecs_world_t *world, ecs_entity_t entity, ecs_value_t value)
{
	if (!ecs_has(world, entity, IsaTextStream)) {
		return false;
	}

	IsaTextStream *stream = ecs_ensure(world, entity, IsaTextStream);
	FILE          *file   = stream->file;

	if (file == NULL) {
		return true;
	}

	if (value.type == 0) {
		fprintf(file, "[%d] %s\n", stream->counter++, (const char *)value.ptr);
	} else {
		char *json = ecs_ptr_to_json(world, value.type, value.ptr);
		fprintf(file, "[%d] %s\n", stream->counter++, json ? json : "?");
		ecs_os_free(json);
	}

	ecs_modified(world, entity, IsaTextStream);
	return true;
}
