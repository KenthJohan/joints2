#pragma once
#include "isa.h"

/** Write handler for one `isa_interface_t` implementor, dispatched by component id. */
typedef struct {
	ecs_id_t iface;
	bool (*write)(ecs_world_t *world, ecs_entity_t entity, ecs_entity_t type, void *value);
} isa_interface_t;

/** `isa_interface_t` write handler for `IsaStack`, defined in isa_stack_interface.c. */
bool IsaInterface_write_stack(
	ecs_world_t  *world,
	ecs_entity_t  entity,
	ecs_entity_t  type,
	void         *value);

/** `isa_interface_t` write handler for `IsaTextStream`, defined in isa_stream_interface.c. */
bool IsaInterface_write_stream(
	ecs_world_t  *world,
	ecs_entity_t  entity,
	ecs_entity_t  type,
	void         *value);
