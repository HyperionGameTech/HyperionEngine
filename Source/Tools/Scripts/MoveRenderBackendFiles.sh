#!/usr/bin/env bash
set -euo pipefail

dry_run=true                # default: show what would happen
[[ ${1:-} == "--apply" ]] && dry_run=false

src_root="src/rendering/backend"
dst_root="src/rendering"

run() {                      # execute or just echo
    if $dry_run; then
        printf '%q ' "$@" ; echo
    else
        "$@"
    fi
}

find "$src_root" -type f -print0 | while IFS= read -r -d '' f; do
    rel="${f#$src_root/}"
    dir="$(dirname "$rel")"
    run mkdir -p "$dst_root/$dir"

    base="$(basename "$rel")"
    [[ $base == Renderer* ]] && base="Render${base#Renderer}"
    if [[ $dir == vulkan* && $base == Render* ]]; then
        base="Vulkan${base#Render}"
    fi

    run mv "$f" "$dst_root/$dir/$base"
done
