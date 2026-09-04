#include "isa_internal.h"
#include <string.h>

ECS_COMPONENT_DECLARE(IsaStack);
ECS_COMPONENT_DECLARE(IsaTextStream);

typedef struct {
	// If null then any next string is accepted, otherwise only this string must be next.
	char const *value;
	bool        required;
} isa_arg_t;

typedef struct {
	char const *name;
	bool (*execute)(ecs_world_t *world, char *args[]);

	isa_arg_t args[8];
	int       arg_count;
} isa_ifcmd_t;

/** Dispatch table mapping a target's component to its `isa_interface_t` handlers.
 * Populated in `IsaImport` once the component ids are known. */
static isa_interface_t g_isa_dispatch[2];

/** Finds the `isa_interface_t` matching `iface`'s component and returns the type it requires,
 * or 0 if any type is allowed (or no matching interface is found). */
static ecs_entity_t IsaInterface_get_type(
ecs_world_t *world,
ecs_entity_t iface)
{
	for (int i = 0; i < 2; i++) {
		if (ecs_has_id(world, iface, g_isa_dispatch[i].iface)) {
			return g_isa_dispatch[i].get_type(world, iface);
		}
	}
	return 0;
}

/** Takes one value from the `isa_interface_t` matching `iface`. */
static bool IsaInterface_take(
ecs_world_t  *world,
ecs_entity_t  iface,
ecs_entity_t *type,
void        **value)
{
	for (int i = 0; i < 2; i++) {
		if (ecs_has_id(world, iface, g_isa_dispatch[i].iface) && g_isa_dispatch[i].take != NULL) {
			return g_isa_dispatch[i].take(world, iface, type, value);
		}
	}
	return false;
}

/** Parses `value` as JSON of `type` into a newly allocated buffer (caller must free). */
static bool IsaRun_parse_value(
ecs_world_t *world,
ecs_entity_t type,
const char  *value,
void       **out_value)
{
	const EcsComponent *comp = ecs_get(world, type, EcsComponent);
	if (comp == NULL || comp->size == 0) {
		return false;
	}

	void       *buf = ecs_os_malloc(comp->size);
	const char *ptr = ecs_ptr_from_json(world, type, buf, value, NULL);
	if (ptr == NULL) {
		ecs_os_free(buf);
		return false;
	}

	*out_value = buf;
	return true;
}

/** Resolves a literal `value` targeting `iface` to a raw value and its type.
 * A `:<type> <value>` prefix selects an explicit JSON type; otherwise the
 * interface's required type determines whether the value is parsed as JSON. */
static bool IsaRun_resolve_operand(
ecs_world_t  *world,
ecs_entity_t  iface,
const char   *value,
ecs_entity_t *out_type,
void        **out_value)
{
	if (value == NULL) {
		return false;
	}

	ecs_entity_t type = IsaInterface_get_type(world, iface);
	if (type == 0) {
		*out_type  = 0;
		*out_value = ecs_os_strdup(value);
		return true;
	}

	if (!IsaRun_parse_value(world, type, value, out_value)) {
		return false;
	}

	*out_type = type;
	return true;
}

/** "WRITE" callback: finds the `isa_interface_t` matching `iface`'s component and invokes it. */
static bool IsaInterface_write(
ecs_world_t *world,
ecs_entity_t iface,
ecs_entity_t type,
void        *value)
{
	for (int i = 0; i < 2; i++) {
		if (ecs_has_id(world, iface, g_isa_dispatch[i].iface)) {
			return g_isa_dispatch[i].write(world, iface, type, value);
		}
	}
	return false;
}

static bool IsaRun_create_stack(
ecs_world_t *world,
char        *args[])
{
	ecs_entity_t type = ecs_lookup(world, args[1]);
	if (type == 0) {
		return false;
	}

	ecs_entity_t entity = ecs_entity(world, {.name = args[0]});
	ecs_set(world, entity, IsaStack, {.type = type});
	return true;
}

static bool IsaRun_transfer(
ecs_world_t *world,
char        *args[])
{
	ecs_entity_t dst = ecs_lookup(world, args[0]);
	ecs_entity_t src = ecs_lookup(world, args[1]);
	if (dst == 0 || src == 0) {
		return false;
	}

	ecs_entity_t type;
	void        *value;
	if (!IsaInterface_take(world, src, &type, &value)) {
		return false;
	}

	bool ok = IsaInterface_write(world, dst, type, value);
	ecs_os_free(value);
	return ok;
}

static bool IsaRun_write(
ecs_world_t *world,
char        *args[])
{
	ecs_entity_t entity = ecs_lookup(world, args[0]);
	if (entity == 0) {
		return false;
	}

	ecs_entity_t type;
	void        *value;
	if (!IsaRun_resolve_operand(world, entity, args[1], &type, &value)) {
		return false;
	}

	bool ok = IsaInterface_write(world, entity, type, value);
	ecs_os_free(value);
	return ok;
}

static const isa_ifcmd_t g_isa_interfaces[] = {
{.name = "CREATE_STACK", .execute = IsaRun_create_stack, .args = {{.required = true}, {.required = true}}, .arg_count = 2},
{.name = "TRANSFER", .execute = IsaRun_transfer, .args = {{.required = true}, {.required = true}}, .arg_count = 2},
{.name = "WRITE", .execute = IsaRun_write, .args = {{.required = true}, {.required = true}}, .arg_count = 2},
};

static bool IsaRun_parse_args(
const isa_ifcmd_t *cmd,
char             **saveptr,
char              *args[])
{
	for (int i = 0; i < cmd->arg_count; i++) {
		args[i] = strtok_r(NULL, " \t", saveptr);
		if (args[i] == NULL) {
			if (cmd->args[i].required) {
				return false;
			}
			continue;
		}
		if (cmd->args[i].value != NULL && strcmp(args[i], cmd->args[i].value)) {
			return false;
		}
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
	line            = strtok_r(NULL, "\r\n", &line_sav)) {
		char *tok_sav = NULL;
		char *op      = strtok_r(line, " \t", &tok_sav);
		if (op == NULL) {
			continue;
		}

		const isa_ifcmd_t *iface_def = NULL;
		for (int i = 0; i < (int)(sizeof(g_isa_interfaces) / sizeof(g_isa_interfaces[0])); i++) {
			if (!strcmp(op, g_isa_interfaces[i].name)) {
				iface_def = &g_isa_interfaces[i];
				break;
			}
		}
		if (iface_def == NULL) {
			ok = false;
			continue;
		}

		char *args[8];
		if (!IsaRun_parse_args(iface_def, &tok_sav, args) || !iface_def->execute(world, args)) {
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
			IsaStack           *stack     = &stacks[i];
			const char         *type_name = ecs_get_name(world, stack->type);
			const EcsComponent *comp      = ecs_get(world, stack->type, EcsComponent);

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
	ECS_COMPONENT_DEFINE(world, IsaTextStream);

	ecs_struct(world,
	{.entity = ecs_id(IsaStack),
	.members = {
	{.name = "type", .type = ecs_id(ecs_entity_t)},
	}});

	ecs_struct(world,
	{.entity = ecs_id(IsaTextStream),
	.members = {
	{.name = "counter", .type = ecs_id(ecs_i32_t)},
	}});

	g_isa_dispatch[0] = (isa_interface_t){.iface = ecs_id(IsaStack), .get_type = IsaInterface_get_type_stack, .write = IsaInterface_write_stack, .take = IsaInterface_take_stack};
	g_isa_dispatch[1] = (isa_interface_t){.iface = ecs_id(IsaTextStream), .get_type = IsaInterface_get_type_stream, .write = IsaInterface_write_stream};

	/* Scoped under the module, giving it the full path "isa.Stdout". */
	ecs_entity_t stdout_e = ecs_entity(world, {.name = "Stdout"});
	ecs_set(world, stdout_e, IsaTextStream, {.counter = 0, .file = stdout});
}
