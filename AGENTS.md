# AI Build Notes

## Compile

- Build this project from the repository root with `bake`.
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
- If public headers (for example `vendor/draw/include/draw.h`) change, run `bake rebuild` so the generated headers under `bake/include/` are refreshed; otherwise stale headers can cause editor/type-check mismatches.

## Run

- Run this project from the repository root with `bake run`.
- Example:

```sh
bake run
```

## Output

- The debug binary is written to `bin/x64-Linux-debug/joints2`.

## Notes

- The project is configured through `project.json`.
- Do not invoke `gcc` directly unless the user explicitly asks for a manual build command; prefer `bake` for all normal compile steps.