#include "isa_internal.h"

bool IsaStack_push(
	ecs_world_t *world,
	IsaStack    *stack,
	const char  *json)
{
	const EcsComponent *comp = ecs_get(world, stack->type, EcsComponent);
	if (comp == NULL || comp->size == 0) {
		return false;
	}

	if (stack->vec.size == 0) {
		ecs_vec_init(NULL, &stack->vec, comp->size, 0);
	}

	void       *elem = ecs_vec_append(NULL, &stack->vec, comp->size);
	const char *ptr   = ecs_ptr_from_json(world, stack->type, elem, json, NULL);
	if (ptr == NULL) {
		ecs_vec_remove_last(&stack->vec);
		return false;
	}

	return true;
}

/** `isa_interface_t` write handler for `IsaStack`: appends `value` onto the `entity` stack.
 * `type` 0 means `value` is CONST literal text, otherwise it's a raw component value. */
bool IsaInterface_write_stack(
	ecs_world_t  *world,
	ecs_entity_t  entity,
	ecs_entity_t  type,
	void         *value)
{
	if (!ecs_has(world, entity, IsaStack)) {
		return false;
	}

	IsaStack *stack = ecs_ensure(world, entity, IsaStack);
	bool      ok;

	if (type == 0) {
		ok = IsaStack_push(world, stack, (const char *)value);
	} else if (type == stack->type) {
		const EcsComponent *comp = ecs_get(world, stack->type, EcsComponent);
		if (comp != NULL && comp->size != 0) {
			if (stack->vec.size == 0) {
				ecs_vec_init(NULL, &stack->vec, comp->size, 0);
			}
			void *elem = ecs_vec_append(NULL, &stack->vec, comp->size);
			ecs_os_memcpy(elem, value, comp->size);
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
