# AGENTS.md — First-Principles Engineering & Ground-Up Design

## Core Directive
Do NOT default to minimal, surface-level patches or wrappers around flawed abstractions. When an API, schema, spec, or architecture is broken, inconsistent, or fighting the actual problem, fix the root cause from the ground up.

## 1. First-Principles Analysis (Before Writing Code)
Before proposing any edit, state your reasoning explicitly in this order:
1. **The Fundamental Problem:** What is the system actually trying to achieve at the data, state, and invariant level?
2. **Audit the Existing Spec & API:** Are the current types, endpoints, contracts, or specs broken, misleading, or overly coupled? Do NOT assume existing interfaces are sacred.
3. **Challenge Assumptions:** List any legacy conventions or workarounds in the current code that exist only to patch an earlier design flaw.
4. **Delete Before Adding:** Identify dead abstractions, redundant adapters, or flawed specs that should be removed or replaced rather than wrapped.

## 2. Anti-Workaround Rules (Strictly Enforced)
- **Fix the cause of a bug. Do not add a workaround that hides the symptom.**
  - Do NOT add defensive null checks, silent fallbacks, `try/catch` swallowers, or special-case `if` branches to mask upstream bugs or broken contracts.
  - Trace every bug back to the invalid state, broken spec, or flawed API that produced it, and fix it there.
- **Never paper over a bad API:** Do not write adapter layers, translation shims, or defensive hacks to accommodate a broken internal API or spec. Fix the API and spec directly.
- **No "minimal diff" bias on broken foundations:** Prefer a clean, breaking internal refactor over a small patch that preserves technical debt.
- **Single Source of Truth:** If code and specification (OpenAPI, JSON Schema, types, docs) disagree, determine which represents the true domain requirement, fix the spec first, and align all callers and tests to it.

## 3. Execution Workflow
1. **Prove the Root Cause:** For any bug or design flaw, state in one sentence *why* the invalid state was possible in the first place and how your change makes it impossible.
2. **Propose the Ground-Up Design:** Briefly outline the clean target architecture/spec and list every file/caller that will be updated.
3. **Update the Spec & Types First:** Modify the core schemas, interfaces, and API contracts before touching implementation details.
4. **Refactor Callers & Tests:** Update all consumers cleanly—do not leave deprecated shims or backward-compatibility wrappers unless explicitly requested for an external public API.
5. **Verify:** Run the full test suite, linter, and typechecker to prove the new design holds end-to-end.

## 4. When to Ask Before Breaking
- **Proceed autonomously** on internal modules, private APIs, internal specs, and unshipped features.
- **Ask for confirmation ONLY** before changing public/external versioned APIs, production database schemas with live data, or billing/auth boundaries.


# AI Build Notes

## Compile

- Build this project from the repository root with `bake` (the user's shell aliases `bake` to `bake3`; always use the `bake` command).
- Example:

```sh
bake
```

- Rebuild recursively from the repository root with `bake rebuild -r`.
- Example:

```sh
bake rebuild -r
```

- For package/library compile checks only, use `bake rebuild` to quickly verify the library code builds.
- To rebuild a specific app or package, pass its bake project path: `bake rebuild <bake_project_path>`.
- Example:

```sh
bake rebuild apps/example
```

- If public headers (for example `vendor/draw/include/draw.h`) change, run `bake rebuild` so the generated headers under `bake3/include/` are refreshed; otherwise stale headers can cause editor/type-check mismatches.

## Run

- Run this project from the repository root with `bake run`.
- Example:

```sh
bake run
```

- To run/test a specific app or package, pass its bake project path: `bake run <bake_project_path>`.
- Example:

```sh
bake run apps/example
```

## Notes

- The project is configured through `project.json`.
- Do not invoke `gcc` directly unless the user explicitly asks for a manual build command; prefer `bake` for all normal compile steps.
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
- After edits, run `bake rebuild -r` from the repository root to validate compile-time changes.

## Debugging With The Flecs REST API

- Apps that set `EcsRest` on the world (for example `ecs_set(world, EcsRest, {.port = 0})`) expose a REST server on `http://localhost:27750` while running. Use it to inspect and modify live ECS state instead of guessing from code or scripts.
- Run the app in the background, check it responds with `curl -s localhost:27750/`, and stop it when done.
- Entity paths use `/` as the separator, not `.` (`parent.child` becomes `parent/child`).
- Read an entity and its component values: `curl -s localhost:27750/entity/<path>`
- Query entities by component (fully qualified names, `%24this` is `$this`): `curl -s 'localhost:27750/query?expr=<module>.<Component>(%24this)'`
- Set a component value to test a hypothesis, then restore it:

```sh
curl -s -X PUT -G localhost:27750/component/<path> \
  --data-urlencode 'component=<module>.<Component>' \
  --data-urlencode 'value={"field":1}'
```

- Other endpoints: `GET /component/<path>?component=<name>`, `GET /world`, `GET /components`. `GET /` lists examples.
- Typical use: compare a source value with the values derived from it, to find where a chain of updates stops.
- Script templates only react to component changes when the script is loaded as managed (`ecs_script(world, {.filename = ...})`), and C code that writes a component field directly must call `ecs_modified_id` for them to see the change.


