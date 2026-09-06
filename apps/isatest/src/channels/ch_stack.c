#include "../isa_internal.h"

/** `isa_channel_t` get_write_type handler for `IsaStack`: values must match the stack's own type. */
ecs_entity_t ch_stack_get_write_type(ecs_world_t *world, ecs_entity_t entity)
{
	if (!ecs_has(world, entity, IsaStack)) {
		return 0;
	}
	const IsaStack *stack = ecs_get(world, entity, IsaStack);
	return stack->type;
}

/** `isa_channel_t` get_take_type handler for `IsaStack`: values have the stack's own type. */
ecs_entity_t ch_stack_get_take_type(ecs_world_t *world, ecs_entity_t entity)
{
	return ch_stack_get_write_type(world, entity);
}

/** `isa_channel_t` write handler for `IsaStack`: appends `value` onto the `entity` stack.
 * `value.ptr` must be a raw component value of `stack->type`. */
bool ch_stack_write(ecs_world_t *world, ecs_entity_t entity, ecs_value_t value)
{
	IsaStack *stack = ecs_get_mut(world, entity, IsaStack);
	ecs_assert(stack != NULL, ECS_INVALID_PARAMETER, NULL);
	ecs_assert(value.ptr != NULL, ECS_INVALID_PARAMETER, NULL);
	ecs_assert(value.type != 0, ECS_INVALID_PARAMETER, NULL);
	ecs_assert(stack->type != 0, ECS_INVALID_PARAMETER, NULL);
	if (value.type != stack->type) {
		return false;
	}
	
	const EcsComponent *comp = ecs_get(world, stack->type, EcsComponent);
	ecs_assert(comp != NULL, ECS_INVALID_PARAMETER, NULL);
	ecs_assert(comp->size != 0, ECS_INVALID_PARAMETER, NULL);

	void *elem = ecs_vec_append(NULL, &stack->vec, comp->size);
	ecs_os_memcpy(elem, value.ptr, comp->size);

	ecs_modified(world, entity, IsaStack);
	return true;
}

/** `isa_channel_t` open handler for `IsaStack`: initializes the entity's stack to hold `type`. */
bool ch_stack_open(ecs_world_t *world, ecs_entity_t entity, ecs_id_t type)
{
	ecs_set(world, entity, IsaStack, {.type = type});
	return true;
}

/** `isa_channel_t` take handler for `IsaStack`: removes and copies its top value. */
bool ch_stack_take(ecs_world_t *world, ecs_entity_t entity, ecs_value_t *value)
{
	IsaStack *stack = ecs_get_mut(world, entity, IsaStack);
	ecs_assert(stack != NULL, ECS_INVALID_PARAMETER, NULL);
	ecs_assert(stack->type != 0, ECS_INVALID_PARAMETER, NULL);

	// Return false if the stack is empty.
	if (stack->vec.count == 0) {
		return false;
	}

	const EcsComponent *comp = ecs_get(world, stack->type, EcsComponent);
	ecs_assert(comp != NULL, ECS_INVALID_PARAMETER, NULL);
	ecs_assert(comp->size != 0, ECS_INVALID_PARAMETER, NULL);
	
	void *elem = ecs_vec_get(&stack->vec, comp->size, stack->vec.count - 1);
	void *copy = ecs_os_malloc(comp->size);
	ecs_os_memcpy(copy, elem, comp->size);
	ecs_vec_remove_last(&stack->vec);
	ecs_modified(world, entity, IsaStack);

	value->type = stack->type;
	value->ptr  = copy;
	return true;
}
