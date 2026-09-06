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
	if (!ecs_has(world, entity, IsaStack)) {
		return false;
	}

	IsaStack *stack = ecs_ensure(world, entity, IsaStack);
	bool      ok;

	if (value.type == stack->type) {
		const EcsComponent *comp = ecs_get(world, stack->type, EcsComponent);
		if (comp != NULL && comp->size != 0) {
			if (stack->vec.size == 0) {
				ecs_vec_init(NULL, &stack->vec, comp->size, 0);
			}
			void *elem = ecs_vec_append(NULL, &stack->vec, comp->size);
			ecs_os_memcpy(elem, value.ptr, comp->size);
			ok = true;
		} else {
			ok = false;
		}
	} else {
		ok = false;
	}

	if (ok) {
		ecs_modified(world, entity, IsaStack);
	}
	return ok;
}

/** `isa_channel_t` take handler for `IsaStack`: removes and copies its top value. */
bool ch_stack_take(ecs_world_t *world, ecs_entity_t entity, ecs_value_t *value)
{
	if (!ecs_has(world, entity, IsaStack)) {
		return false;
	}

	IsaStack *stack = ecs_ensure(world, entity, IsaStack);
	if (stack->vec.count == 0) {
		return false;
	}

	const EcsComponent *comp = ecs_get(world, stack->type, EcsComponent);
	if (comp == NULL || comp->size == 0) {
		return false;
	}

	void *elem = ecs_vec_get(&stack->vec, comp->size, stack->vec.count - 1);
	void *copy = ecs_os_malloc(comp->size);
	ecs_os_memcpy(copy, elem, comp->size);
	ecs_vec_remove_last(&stack->vec);
	ecs_modified(world, entity, IsaStack);

	value->type = stack->type;
	value->ptr  = copy;
	return true;
}
