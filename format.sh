#!/usr/bin/env bash
set -euo pipefail

IGNORE_FILES=( "bake_config.h" "stb_truetype.h" "stb_image_write.h" )

IGNORE=()
for f in "${IGNORE_FILES[@]}"; do
    IGNORE+=( -not -name "$f" )
done

find apps        \( -name "*.c" -o -name "*.h" \) "${IGNORE[@]}" | xargs clang-format -i
find vendor/draw \( -name "*.c" -o -name "*.h" \) "${IGNORE[@]}" | xargs clang-format -i
