#pragma once
#include "isa.h"

/** Write handler for one `isa_interface_t` implementor, dispatched by component id. */
typedef struct {
	ecs_id_t iface;
	/** Returns the type values written to `entity` must have, or 0 if any type is allowed. */
	ecs_entity_t (*get_type)(ecs_world_t *world, ecs_entity_t entity);
	bool (*write)(ecs_world_t *world, ecs_entity_t entity, ecs_entity_t type, void *value);
	/** Takes one value into a caller-owned buffer and returns its component type. */
	bool (*take)(ecs_world_t *world, ecs_entity_t entity, ecs_entity_t *type, void **value);
} isa_interface_t;

/** `isa_interface_t` get_type handler for `IsaStack`, defined in isa_stack_interface.c. */
ecs_entity_t IsaInterface_get_type_stack(
	ecs_world_t  *world,
	ecs_entity_t  entity);

/** `isa_interface_t` write handler for `IsaStack`, defined in isa_stack_interface.c. */
bool IsaInterface_write_stack(
	ecs_world_t  *world,
	ecs_entity_t  entity,
	ecs_entity_t  type,
	void         *value);

/** `isa_interface_t` take handler for `IsaStack`, defined in isa_stack_interface.c. */
bool IsaInterface_take_stack(
	ecs_world_t  *world,
	ecs_entity_t  entity,
	ecs_entity_t *type,
	void        **value);

/** `isa_interface_t` get_type handler for `IsaTextStream`, defined in isa_stream_interface.c. */
ecs_entity_t IsaInterface_get_type_stream(
	ecs_world_t  *world,
	ecs_entity_t  entity);

/** `isa_interface_t` write handler for `IsaTextStream`, defined in isa_stream_interface.c. */
bool IsaInterface_write_stream(
	ecs_world_t  *world,
	ecs_entity_t  entity,
	ecs_entity_t  type,
	void         *value);
