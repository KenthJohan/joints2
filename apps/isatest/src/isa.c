#include "isa.h"
#include <string.h>

ECS_COMPONENT_DECLARE(IsaStack);
ECS_COMPONENT_DECLARE(IsaStdout);


typedef struct {
	char const * name;
	// function pointer
	bool (*input)(ecs_world_t *world, ecs_entity_t iface, ecs_entity_t type, void * value);
} isa_interface_t;

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

/** Resolves a "MODE VALUE" operand to a raw value and its type.
 * CONST yields the literal text with type 0. POP yields a heap copy of the
 * popped element (caller must free) along with the source stack's type. */
static bool IsaRun_resolve_operand(
	ecs_world_t  *world,
	const char   *mode,
	const char   *value,
	ecs_entity_t *out_type,
	void        **out_value)
{
	if (mode == NULL || value == NULL) {
		return false;
	}

	if (!strcmp(mode, "CONST")) {
		*out_type  = 0;
		*out_value = ecs_os_strdup(value);
		return true;
	}

	if (!strcmp(mode, "POP")) {
		ecs_entity_t e = ecs_lookup(world, value);
		if (e == 0 || !ecs_has(world, e, IsaStack)) {
			return false;
		}

		IsaStack *stack = ecs_ensure(world, e, IsaStack);
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
		ecs_modified(world, e, IsaStack);

		*out_type  = stack->type;
		*out_value = copy;
		return true;
	}

	return false;
}

/** IsaStdout "PRINT" callback: prints `value` with an incrementing counter.
 * `type` 0 means `value` is CONST literal text, otherwise it's a raw component value. */
static bool IsaInterface_print(
	ecs_world_t  *world,
	ecs_entity_t  iface,
	ecs_entity_t  type,
	void         *value);

/** IsaStack "PUSH" callback: appends `value` onto the `iface` stack.
 * `type` 0 means `value` is CONST literal text, otherwise it's a raw component value.
 * Falls back to printing when `iface` is an `IsaStdout` instead of an `IsaStack`. */
static bool IsaInterface_push(
	ecs_world_t  *world,
	ecs_entity_t  iface,
	ecs_entity_t  type,
	void         *value)
{
	if (!ecs_has(world, iface, IsaStack)) {
		return IsaInterface_print(world, iface, type, value);
	}

	IsaStack *stack = ecs_ensure(world, iface, IsaStack);
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
		ecs_modified(world, iface, IsaStack);
	}
	return ok;
}

/** IsaStdout "PRINT" callback: prints `value` with an incrementing counter.
 * `type` 0 means `value` is CONST literal text, otherwise it's a raw component value. */
static bool IsaInterface_print(
	ecs_world_t  *world,
	ecs_entity_t  iface,
	ecs_entity_t  type,
	void         *value)
{
	if (!ecs_has(world, iface, IsaStdout)) {
		return false;
	}

	IsaStdout *stdout_comp = ecs_ensure(world, iface, IsaStdout);

	if (type == 0) {
		printf("[%d] %s\n", stdout_comp->counter++, (const char *)value);
	} else {
		char *json = ecs_ptr_to_json(world, type, value);
		printf("[%d] %s\n", stdout_comp->counter++, json ? json : "?");
		ecs_os_free(json);
	}

	ecs_modified(world, iface, IsaStdout);
	return true;
}

static const isa_interface_t g_isa_interfaces[] = {
	{.name = "PUSH",  .input = IsaInterface_push},
	{.name = "PRINT", .input = IsaInterface_print},
};

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
			char *mode      = strtok_r(NULL, " \t", &tok_sav);
			char *type_name = strtok_r(NULL, " \t", &tok_sav);
			ecs_entity_t type = (name && mode && type_name && !strcmp(mode, "CONST"))
				? ecs_lookup(world, type_name) : 0;
			if (type == 0) {
				ok = false;
				continue;
			}
			ecs_entity_t e = ecs_entity(world, {.name = name});
			ecs_set(world, e, IsaStack, {.type = type});
			continue;
		}

		const isa_interface_t *iface_def = NULL;
		for (int i = 0; i < 2; i++) {
			if (!strcmp(op, g_isa_interfaces[i].name)) {
				iface_def = &g_isa_interfaces[i];
				break;
			}
		}
		if (iface_def == NULL) {
			ok = false;
			continue;
		}

		char *interface = strtok_r(NULL, " \t", &tok_sav);
		char *mode      = strtok_r(NULL, " \t", &tok_sav);
		char *value_tok = strtok_r(NULL, " \t", &tok_sav);
		ecs_entity_t e  = interface ? ecs_lookup(world, interface) : 0;
		if (e == 0) {
			ok = false;
			continue;
		}

		ecs_entity_t type;
		void        *value;
		if (!IsaRun_resolve_operand(world, mode, value_tok, &type, &value)) {
			ok = false;
			continue;
		}

		if (!iface_def->input(world, e, type, value)) {
			ecs_os_free(value);
			ok = false;
			continue;
		}
		ecs_os_free(value);
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
	ECS_COMPONENT_DEFINE(world, IsaStdout);

	ecs_struct(world,
	{.entity = ecs_id(IsaStack),
	.members = {
	{.name = "type", .type = ecs_id(ecs_entity_t)},
	}});

	ecs_struct(world,
	{.entity = ecs_id(IsaStdout),
	.members = {
	{.name = "counter", .type = ecs_id(ecs_i32_t)},
	}});

	/* Scoped under the module, giving it the full path "isa.Stdout". */
	ecs_entity_t stdout_e = ecs_entity(world, {.name = "Stdout"});
	ecs_set(world, stdout_e, IsaStdout, {.counter = 0});
}
