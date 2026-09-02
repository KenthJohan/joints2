#include "isa.h"
#include <string.h>

ECS_COMPONENT_DECLARE(IsaStack);

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

bool IsaRun(
	ecs_world_t *world,
	const char  *script)
{
	bool  ok       = true;
	char *buf      = ecs_os_strdup(script);
	char *line_sav = NULL;

	for (char *line = strtok_r(buf, "\r\n", &line_sav); line != NULL;
		line = strtok_r(NULL, "\r\n", &line_sav)) {
		char *tok_sav = NULL;
		char *op      = strtok_r(line, " \t", &tok_sav);
		if (op == NULL) {
			continue;
		}

		if (!strcmp(op, "CREATE_STACK")) {
			char *name      = strtok_r(NULL, " \t", &tok_sav);
			char *type_name = strtok_r(NULL, " \t", &tok_sav);
			ecs_entity_t type = (name && type_name) ? ecs_lookup(world, type_name) : 0;
			if (type == 0) {
				ok = false;
				continue;
			}
			ecs_entity_t e = ecs_entity(world, {.name = name});
			ecs_set(world, e, IsaStack, {.type = type});
		} else if (!strcmp(op, "PUSH")) {
			char *name  = strtok_r(NULL, " \t", &tok_sav);
			char *value = strtok_r(NULL, " \t", &tok_sav);
			ecs_entity_t e = name ? ecs_lookup(world, name) : 0;
			IsaStack *stack = e ? ecs_ensure(world, e, IsaStack) : NULL;
			if (stack == NULL || value == NULL || !IsaStack_push(world, stack, value)) {
				ok = false;
				continue;
			}
			ecs_modified(world, e, IsaStack);
		} else {
			ok = false;
		}
	}

	ecs_os_free(buf);
	return ok;
}

void IsaStack_print_all(ecs_world_t *world)
{
	ecs_query_t *q = ecs_query(world, {.terms = {{.id = ecs_id(IsaStack)}}});

	ecs_iter_t it = ecs_query_iter(world, q);
	while (ecs_query_next(&it)) {
		IsaStack *stacks = ecs_field(&it, IsaStack, 0);
		for (int i = 0; i < it.count; i++) {
			IsaStack   *stack     = &stacks[i];
			const char *type_name = ecs_get_name(world, stack->type);
			const EcsComponent *comp = ecs_get(world, stack->type, EcsComponent);

			printf("%s: IsaStack { type = %s, count = %d }\n",
				ecs_get_name(world, it.entities[i]),
				type_name ? type_name : "?",
				stack->vec.count);

			if (comp == NULL || comp->size == 0) {
				continue;
			}

			for (int e = 0; e < stack->vec.count; e++) {
				void *elem = ecs_vec_get(&stack->vec, comp->size, e);
				char *json = ecs_ptr_to_json(world, stack->type, elem);
				printf("  [%d] = %s\n", e, json ? json : "?");
				ecs_os_free(json);
			}
		}
	}

	ecs_query_fini(q);
}

void IsaImport(ecs_world_t *world)
{
	ECS_MODULE(world, Isa);
	ecs_set_name_prefix(world, "Isa");

	ECS_COMPONENT_DEFINE(world, IsaStack);

	ecs_struct(world,
	{.entity = ecs_id(IsaStack),
	.members = {
	{.name = "type", .type = ecs_id(ecs_entity_t)},
	}});
}
