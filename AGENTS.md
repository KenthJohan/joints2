# AI Build Notes

## Compile

- Build this project from the repository root with `bake3`.
- Example:

```sh
bake3
```

- Rebuild recursively from the repository root with `bake3 rebuild -r`.
- Example:

```sh
bake3 rebuild -r
```

- For package/library compile checks only, use `bake3 rebuild` to quickly verify the library code builds.
- To rebuild a specific app or package, pass its bake project path: `bake3 rebuild <bake_project_path>`.
- Example:

```sh
bake3 rebuild apps/example
```

- If public headers (for example `vendor/draw/include/draw.h`) change, run `bake3 rebuild` so the generated headers under `bake3/include/` are refreshed; otherwise stale headers can cause editor/type-check mismatches.

## Run

- Run this project from the repository root with `bake3 run`.
- Example:

```sh
bake3 run
```

- To run/test a specific app or package, pass its bake project path: `bake3 run <bake_project_path>`.
- Example:

```sh
bake3 run apps/example
```

## Notes

- The project is configured through `project.json`.
- Do not invoke `gcc` directly unless the user explicitly asks for a manual build command; prefer `bake3` for all normal compile steps.
- In Flecs systems, `ecs_field(it, Type, index)` uses the term order from `.query.terms` (0-based). If you add/reorder terms, update all field indices in the callback to match.
- In Flecs systems, prefer bringing dependency data in through query terms and `ecs_field(...)` instead of calling `ecs_get(...)` inside the callback when the data can be declared in the system signature.

## Flecs Namespace Mapping

- If a module calls `ecs_set_name_prefix(world, "AppDraw")`, Flecs script references should use the namespace form `app.draw`.
- Example: C component `AppDrawNameAtPositionRule` is referenced in script as `app.draw.NameAtPositionRule`.
- When checking script references, do not only search for the C symbol (`AppDraw...`). Also search for namespace-qualified script names (`app.draw...`).

## Refactoring Across C And Flecs Script

- When renaming a component/system/observer in C, update all C declarations/definitions/usages first (`*.h`, `*.c`).
- Then update Flecs script references in `*.flecs` files using the namespace form from the module prefix.
- For `AppDraw` symbols, include both searches during refactor validation:
	- Old/new C symbol names, such as `AppDrawPrintPositionalBinding` and `AppDrawNameAtPositionRule`.
	- Old/new script symbol names, such as `app.draw.PrintPositionalBinding` and `app.draw.NameAtPositionRule`.
- After edits, run `bake3 rebuild -r` from the repository root to validate compile-time changes.
