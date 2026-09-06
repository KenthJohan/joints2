#pragma once
#include "isa.h"

/** `IsaChannel` get_write_type handler for `IsaStack`, defined in channels/ch_stack_interface.c. */
ecs_entity_t ch_stack_get_write_type(ecs_world_t *world, ecs_entity_t entity);

/** `IsaChannel` get_take_type handler for `IsaStack`, defined in channels/ch_stack_interface.c. */
ecs_entity_t ch_stack_get_take_type(ecs_world_t *world, ecs_entity_t entity);

/** `IsaChannel` write handler for `IsaStack`, defined in channels/ch_stack_interface.c. */
bool ch_stack_write(ecs_world_t *world, ecs_entity_t entity, ecs_value_t value);

/** `IsaChannel` take handler for `IsaStack`, defined in channels/ch_stack_interface.c. */
bool ch_stack_take(ecs_world_t *world, ecs_entity_t entity, ecs_value_t *value);

/** `IsaChannel` open handler for `IsaStack`, defined in channels/ch_stack.c. */
bool ch_stack_open(ecs_world_t *world, ecs_entity_t entity, ecs_id_t type);

/** `IsaChannel` get_write_type handler for `IsaTextStream`, defined in channels/ch_stream_interface.c. */
ecs_entity_t ch_stream_get_write_type(ecs_world_t *world, ecs_entity_t entity);

/** `IsaChannel` write handler for `IsaTextStream`, defined in channels/ch_stream_interface.c. */
bool ch_stream_write(ecs_world_t *world, ecs_entity_t entity, ecs_value_t value);
