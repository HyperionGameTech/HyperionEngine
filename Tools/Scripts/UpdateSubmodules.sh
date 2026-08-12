#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

cd "$SCRIPT_DIR/../.." || exit 1

if [[ ! -f ".gitmodules" ]]; then
    echo "[ERROR] .gitmodules not found in \"$(pwd)\""
    exit 1
fi

echo "Updating submodules..."

process_submodule() {
    local sm_path="$1"
    local sm_url
    local sm_branch
    local sm_commit

    sm_url="$(git config --file .gitmodules --get "submodule.${sm_path}.url" 2>/dev/null)"
    sm_branch="$(git config --file .gitmodules --get "submodule.${sm_path}.branch" 2>/dev/null)"
    sm_commit="$(git config --file .gitmodules --get "submodule.${sm_path}.commit" 2>/dev/null)"

    echo
    if [[ -z "$sm_url" ]]; then
        echo "[WARNING] No URL found for submodule: $sm_path"
        return 1
    fi

    if [[ -e "$sm_path/.git" ]]; then
        echo "[UPDATE] $sm_path"
        git -C "$sm_path" fetch --all --tags --prune
        if [[ -n "$sm_branch" ]]; then
            git -C "$sm_path" checkout "$sm_branch"
            git -C "$sm_path" pull origin "$sm_branch"
        elif [[ -n "$sm_commit" ]]; then
            git -C "$sm_path" checkout "$sm_commit"
        else
            git -C "$sm_path" pull
        fi
    else
        if [[ -d "$sm_path" ]]; then
            if [[ -n "$(ls -A "$sm_path" 2>/dev/null)" ]]; then
                echo "[WARNING] $sm_path exists but is not a git checkout and is not empty; skipping. Remove it manually to allow cloning."
                return 1
            fi
            rmdir "$sm_path"
        fi

        echo "[CLONE] $sm_path  <--  $sm_url"
        if [[ -n "$sm_branch" ]]; then
            git clone --branch "$sm_branch" -- "$sm_url" "$sm_path"
        else
            git clone -- "$sm_url" "$sm_path"
            if [[ -n "$sm_commit" ]]; then
                git -C "$sm_path" checkout "$sm_commit"
            fi
        fi
    fi
}

while IFS= read -r sm_path; do
    process_submodule "$sm_path"
done < <(git config --file .gitmodules --get-regexp 'submodule\..*\.path' | awk '{print $2}')

echo
echo "Done."
