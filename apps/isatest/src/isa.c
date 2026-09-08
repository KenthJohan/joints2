#include "isa_internal.h"
#include <string.h>

ECS_COMPONENT_DECLARE(IsaStack);
ECS_COMPONENT_DECLARE(IsaTextStream);
ECS_COMPONENT_DECLARE(IsaTransferConfig);
static ECS_COMPONENT_DECLARE(IsaOpenArgument);
static ECS_COMPONENT_DECLARE(IsaArg);
static ECS_COMPONENT_DECLARE(IsaCmd);
static ECS_COMPONENT_DECLARE(IsaChannel);

/** Write handler for one `IsaChannel` implementor, set on the component entity it dispatches for. */
typedef struct {
	/** Returns the type values written to `entity` must have, or 0 if any type is allowed. */
	ecs_entity_t (*get_write_type)(ecs_world_t *world, ecs_entity_t entity);
	/** Returns the type values taken from `entity` have, or 0 if any type is allowed. */
	ecs_entity_t (*get_take_type)(ecs_world_t *world, ecs_entity_t entity);
	bool (*write)(ecs_world_t *world, ecs_entity_t entity, ecs_value_t value);
	/** Takes one value into a caller-owned buffer, returned via `value->type`/`value->ptr`. */
	bool (*take)(ecs_world_t *world, ecs_entity_t entity, ecs_value_t *value);
	/** Initializes `entity`'s storage for this channel with the given value `type`. */
	bool (*open)(ecs_world_t *world, ecs_entity_t entity, ecs_id_t type);
} IsaChannel;

typedef struct {
	// If null then any next string is accepted, otherwise only this string must be next.
	char const *value;
	bool        required;
	// If zero the value can be anything, otherwise it must parse as JSON of this type.
	ecs_id_t required_type;
} IsaArg;

typedef struct {
	bool (*execute)(ecs_world_t *world, char *args[]);

	IsaArg args[8];
	int    arg_count;
} IsaCmd;

enum {
	ARG_OPEN_ENTITY,
	ARG_OPEN_ARGUMENT,
	ARG_OPEN_COUNT
};

enum {
	ARG_TRANSFER_DESTINATION,
	ARG_TRANSFER_OPERATOR,
	ARG_TRANSFER_SOURCE,
	ARG_TRANSFER_COUNT
};

enum {
	ARG_WRITE_ENTITY,
	ARG_WRITE_OPERATOR,
	ARG_WRITE_VALUE,
	ARG_WRITE_AS,
	ARG_WRITE_TYPE,
	ARG_WRITE_COUNT
};

/** Module entity, used to look up command entities registered as its children by name. */
static ecs_entity_t g_isa_module;

/** IsaChannel prefabs; instances inherit their dispatch via EcsIsA. */
static ecs_entity_t g_isa_stack_channel;
static ecs_entity_t g_isa_stream_channel;

typedef struct {
	uint32_t line_number;
	char    *text;
} isa_line_t;

typedef struct {
	ecs_vec_t lines; // <isa_line_t>
} isa_program_t;

static void IsaProgram_parse(char *script, isa_program_t *program)
{
	ecs_vec_init(NULL, &program->lines, sizeof(isa_line_t), 0);

	uint32_t line_number = 1;
	char    *line_start  = script;
	for (char *cursor = script;; cursor++) {
		if (*cursor != '\r' && *cursor != '\n' && *cursor != '\0') {
			continue;
		}

		bool finished = *cursor == '\0';
		if (*cursor == '\r' && cursor[1] == '\n') {
			cursor++;
		}
		*cursor = '\0';

		isa_line_t *line  = ecs_vec_append(NULL, &program->lines, sizeof(isa_line_t));
		line->line_number = line_number++;
		line->text        = line_start;

		if (finished) {
			break;
		}
		line_start = cursor + 1;
	}
}

static void IsaProgram_fini(isa_program_t *program)
{
	ecs_vec_fini(NULL, &program->lines, sizeof(isa_line_t));
}

/** Finds the `IsaChannel` matching `iface`'s component and returns the type it requires,
 * or 0 if any type is allowed (or no matching interface is found). */
static ecs_entity_t IsaInterface_get_write_type(ecs_world_t *world, ecs_entity_t iface)
{
	const IsaChannel *channel = ecs_get(world, iface, IsaChannel);
	return channel ? channel->get_write_type(world, iface) : 0;
}

/** Parses `value` as an expression of `type` into a newly allocated buffer (caller must free with ecs_ptr_free). */
static bool IsaRun_parse_value(ecs_world_t *world, ecs_entity_t type, const char *value, void **out_value)
{
	const EcsComponent *comp = ecs_get(world, type, EcsComponent);
	if (comp == NULL || comp->size == 0) {
		return false;
	}

	ecs_value_t          result = {.type = type};
	ecs_expr_eval_desc_t desc   = {.expr = value};
	if (ecs_expr_run(world, value, &result, &desc) == NULL) {
		return false;
	}

	*out_value = result.ptr;
	return true;
}

/** Resolves a literal `value` targeting `iface` to a raw value and its type.
 * An explicit type takes precedence; otherwise the interface's required type
 * determines whether the value is parsed as JSON. */
static bool IsaRun_resolve_operand(ecs_world_t *world, ecs_entity_t iface, const char *value, const char *type_name, ecs_entity_t *out_type, void **out_value)
{
	if (value == NULL) {
		return false;
	}

	ecs_entity_t type = type_name ? ecs_lookup(world, type_name) : IsaInterface_get_write_type(world, iface);
	if (type_name != NULL && type == 0) {
		return false;
	}
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

static bool IsaRun_open(ecs_world_t *world, char *args[])
{
	void *value;
	if (!IsaRun_parse_value(world, ecs_id(IsaOpenArgument), args[ARG_OPEN_ARGUMENT], &value)) {
		return false;
	}
	IsaOpenArgument *open_arg = value;

	const IsaChannel *channel = ecs_get(world, open_arg->driver, IsaChannel);
	bool              ok      = false;
	if (channel != NULL && channel->open != NULL) {
		ecs_entity_t entity = ecs_entity(world, {.name = args[ARG_OPEN_ENTITY]});
		ecs_add_pair(world, entity, EcsIsA, open_arg->driver);
		ok = channel->open(world, entity, open_arg->type);
	}

	ecs_ptr_free(world, ecs_id(IsaOpenArgument), value);
	return ok;
}

static bool IsaRun_transfer(ecs_world_t *world, char *args[])
{
	ecs_entity_t dst = ecs_lookup(world, args[ARG_TRANSFER_DESTINATION]);
	ecs_entity_t src = ecs_lookup(world, args[ARG_TRANSFER_SOURCE]);
	if (dst == 0 || src == 0) {
		return false;
	}
	const IsaChannel *src_channel = ecs_get(world, src, IsaChannel);
	const IsaChannel *dst_channel = ecs_get(world, dst, IsaChannel);
	ecs_assert(src_channel != NULL, ECS_INVALID_PARAMETER, NULL);
	ecs_assert(src_channel->take != NULL, ECS_INVALID_PARAMETER, NULL);
	ecs_assert(dst_channel != NULL, ECS_INVALID_PARAMETER, NULL);
	ecs_assert(dst_channel->write != NULL, ECS_INVALID_PARAMETER, NULL);

	ecs_value_t value = {0};
	bool        result;

	result = src_channel->take(world, src, &value);
	if (!result) {
		return false;
	}

	result = dst_channel->write(world, dst, value);

	// After writing, free the value regardless of the result
	if (value.type != 0) {
		ecs_ptr_free(world, value.type, value.ptr);
	} else {
		ecs_os_free(value.ptr);
	}
	return result;
}

static bool IsaRun_write(ecs_world_t *world, char *args[])
{
	ecs_entity_t entity = ecs_lookup(world, args[ARG_WRITE_ENTITY]);
	if (entity == 0) {
		return false;
	}

	ecs_entity_t type;
	void        *value;
	if (args[ARG_WRITE_AS] != NULL && args[ARG_WRITE_TYPE] == NULL) {
		return false;
	}
	if (!IsaRun_resolve_operand(world, entity, args[ARG_WRITE_VALUE], args[ARG_WRITE_TYPE], &type, &value)) {
		return false;
	}

	const IsaChannel *channel = ecs_get(world, entity, IsaChannel);
	bool              ok      = channel ? channel->write(world, entity, (ecs_value_t){.type = type, .ptr = value}) : false;
	if (type != 0) {
		ecs_ptr_free(world, type, value);
	} else {
		ecs_os_free(value);
	}
	return ok;
}

/** Reads the next whitespace-separated token from `*cursor`, treating a brace-delimited
 * group (which may itself contain spaces and nested braces) as a single token. */
static char *isa_tokenize(char **cursor)
{
	char *s = *cursor;
	while (*s == ' ' || *s == '\t') {
		s++;
	}
	if (*s == '\0') {
		*cursor = s;
		return NULL;
	}

	char *start = s;
	if (*s == '{') {
		int depth = 0;
		for (; *s != '\0'; s++) {
			if (*s == '{') {
				depth++;
			} else if (*s == '}') {
				depth--;
				if (depth == 0) {
					s++;
					break;
				}
			}
		}
	} else {
		while (*s != '\0' && *s != ' ' && *s != '\t') {
			s++;
		}
	}

	if (*s != '\0') {
		*s = '\0';
		s++;
	}
	*cursor = s;
	return start;
}

static bool IsaRun_parse_args(ecs_world_t *world, const IsaCmd *cmd, char **cursor, char *args[])
{
	for (int i = 0; i < cmd->arg_count; i++) {
		args[i] = isa_tokenize(cursor);
		if (args[i] == NULL) {
			if (cmd->args[i].required) {
				return false;
			}
			continue;
		}
		if (cmd->args[i].value != NULL && strcmp(args[i], cmd->args[i].value)) {
			return false;
		}
		if (cmd->args[i].required_type != 0) {
			void *parsed = NULL;
			if (!IsaRun_parse_value(world, cmd->args[i].required_type, args[i], &parsed)) {
				return false;
			}
			ecs_ptr_free(world, cmd->args[i].required_type, parsed);
		}
	}
	return true;
}

bool IsaRun(ecs_world_t *world, const char *script)
{
	bool          ok      = true;
	char         *buf     = ecs_os_strdup(script);
	isa_program_t program = {0};
	IsaProgram_parse(buf, &program);

	isa_line_t *lines = ecs_vec_first(&program.lines);
	for (int i = 0; i < program.lines.count; i++) {
		char *cursor = lines[i].text;
		char *op     = isa_tokenize(&cursor);
		if (op == NULL) {
			continue;
		}

		ecs_entity_t  cmd_entity = ecs_lookup_child(world, g_isa_module, op);
		const IsaCmd *cmd        = cmd_entity ? ecs_get(world, cmd_entity, IsaCmd) : NULL;
		if (cmd == NULL) {
			ok = false;
			continue;
		}

		char *args[8];
		if (!IsaRun_parse_args(world, cmd, &cursor, args) || !cmd->execute(world, args)) {
			ok = false;
		}
	}

	IsaProgram_fini(&program);
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

static void IsaStack_on_set(ecs_iter_t *it)
{
	IsaStack *stacks = ecs_field(it, IsaStack, 0);
	for (int i = 0; i < it->count; i++) {
		IsaStack           *stack = &stacks[i];
		const EcsComponent *comp  = ecs_get(it->world, stack->type, EcsComponent);
		if (comp != NULL && comp->size != 0 && stack->vec.array == NULL) {
			ecs_vec_init(NULL, &stack->vec, comp->size, 0);
		}
	}
}

void IsaImport(ecs_world_t *world)
{
	ECS_MODULE(world, Isa);
	ecs_set_name_prefix(world, "Isa");
	g_isa_module = ecs_id(Isa);

	ECS_COMPONENT_DEFINE(world, IsaStack);
	ECS_COMPONENT_DEFINE(world, IsaTextStream);
	ECS_COMPONENT_DEFINE(world, IsaTransferConfig);
	ECS_COMPONENT_DEFINE(world, IsaOpenArgument);
	ECS_COMPONENT_DEFINE(world, IsaArg);
	ECS_COMPONENT_DEFINE(world, IsaCmd);
	ECS_COMPONENT_DEFINE(world, IsaChannel);
	ecs_add_pair(world, ecs_id(IsaChannel), EcsOnInstantiate, EcsInherit);
	ecs_set_hooks(world, IsaStack, {.on_set = IsaStack_on_set});

	ecs_struct(world,
	{.entity = ecs_id(IsaTextStream),
	.members = {
	{.name = "counter", .type = ecs_id(ecs_i32_t)},
	}});

	ecs_struct(world,
	{.entity = ecs_id(IsaTransferConfig),
	.members = {
	{.name = "timeout", .type = ecs_id(ecs_f32_t)},
	}});

	ecs_struct(world,
	{.entity = ecs_id(IsaOpenArgument),
	.members = {
	{.name = "driver", .type = ecs_id(ecs_entity_t)},
	{.name = "type", .type = ecs_id(ecs_id_t)},
	}});

	ecs_struct(world,
	{.entity = ecs_id(IsaArg),
	.members = {
	{.name = "value", .type = ecs_id(ecs_string_t)},
	{.name = "required", .type = ecs_id(ecs_bool_t)},
	{.name = "required_type", .type = ecs_id(ecs_id_t)},
	}});

	ecs_struct(world,
	{.entity = ecs_id(IsaCmd),
	.members = {
	{.name = "execute", .type = ecs_id(ecs_uptr_t)},
	{.name = "args", .type = ecs_id(IsaArg), .count = 8},
	{.name = "arg_count", .type = ecs_id(ecs_i32_t)},
	}});

	ecs_struct(world,
	{.entity = ecs_id(IsaChannel),
	.members = {
	{.name = "get_write_type", .type = ecs_id(ecs_uptr_t)},
	{.name = "get_take_type", .type = ecs_id(ecs_uptr_t)},
	{.name = "write", .type = ecs_id(ecs_uptr_t)},
	{.name = "take", .type = ecs_id(ecs_uptr_t)},
	{.name = "open", .type = ecs_id(ecs_uptr_t)},
	}});

	/* Scoped under the module, giving it the full path "isa.Stack". */
	g_isa_stack_channel = ecs_entity(world, {.name = "Stack"});
	ecs_add_id(world, g_isa_stack_channel, EcsPrefab);
	ecs_set(world, g_isa_stack_channel, IsaChannel, {.get_write_type = ch_stack_get_write_type, .get_take_type = ch_stack_get_take_type, .write = ch_stack_write, .take = ch_stack_take, .open = ch_stack_open});

	/* Scoped under the module, giving it the full path "isa.Stream". */
	g_isa_stream_channel = ecs_entity(world, {.name = "Stream"});
	ecs_add_id(world, g_isa_stream_channel, EcsPrefab);
	ecs_set(world, g_isa_stream_channel, IsaChannel, {.get_write_type = ch_stream_get_write_type, .write = ch_stream_write});

	ecs_entity_t open_cmd = ecs_entity(world, {.name = "OPEN"});
	ecs_set_id(world, open_cmd, ecs_id(IsaCmd), sizeof(IsaCmd),
	&(IsaCmd){
	.execute = IsaRun_open,
	.args    = {
	[ARG_OPEN_ENTITY]   = {.required = true},
	[ARG_OPEN_ARGUMENT] = {.required = true}},
	.arg_count = ARG_OPEN_COUNT});

	ecs_entity_t transfer_cmd = ecs_entity(world, {.name = "TRANSFER"});
	ecs_set_id(world, transfer_cmd, ecs_id(IsaCmd), sizeof(IsaCmd),
	&(IsaCmd){
	.execute = IsaRun_transfer,
	.args    = {
	[ARG_TRANSFER_DESTINATION] = {.required = true},
	[ARG_TRANSFER_OPERATOR]    = {.value = "<--"},
	[ARG_TRANSFER_SOURCE]      = {.required = true}},
	.arg_count = ARG_TRANSFER_COUNT,
	});

	ecs_entity_t write_cmd = ecs_entity(world, {.name = "WRITE"});
	ecs_set_id(world, write_cmd, ecs_id(IsaCmd), sizeof(IsaCmd),
	&(IsaCmd){
	.execute = IsaRun_write,
	.args    = {
	[ARG_WRITE_ENTITY]   = {.required = true},
	[ARG_WRITE_OPERATOR] = {.value = "<--"},
	[ARG_WRITE_VALUE]    = {.required = true},
	[ARG_WRITE_AS]       = {.value = "AS"},
	[ARG_WRITE_TYPE]     = {}},
	.arg_count = ARG_WRITE_COUNT,
	});

	/* Scoped under the module, giving it the full path "isa.Stdout". */
	ecs_entity_t stdout_e = ecs_entity(world, {.name = "Stdout"});
	ecs_add_pair(world, stdout_e, EcsIsA, g_isa_stream_channel);
	ecs_set(world, stdout_e, IsaTextStream, {.counter = 0, .file = stdout});
}
