#!/usr/bin/env python3
"""
Renames shader files in res/shaders from GLSL extensions to HLSL extensions:
  - .glsl, .inc -> .hlsl
"""

import os
import sys
from pathlib import Path


def rename_shaders(shaders_dir: Path, dry_run: bool = False) -> None:
    """
    Args:
        shaders_dir: Path to the shaders directory
        dry_run: If True, only print what would be renamed without actually renaming
    """
    if not shaders_dir.exists():
        print(f"Error: Directory '{shaders_dir}' does not exist.")
        sys.exit(1)

    extension_map = {
        '.glsl': '.hlsl',
        '.inc': '.hlsl',
        '.frag': '.frag.hlsl',
        '.vert': '.vert.hlsl',
        '.geom': '.geom.hlsl',
        '.comp': '.hlsl',
        '.rgen': '.rgen.hlsl',
        '.rmiss': '.rmiss.hlsl',
        '.rchit': '.rchit.hlsl',
        '.rahit': '.rahit.hlsl',
        '.rint': '.rint.hlsl',
    }

    renamed_count = 0

    for old_ext, new_ext in extension_map.items():
        for file_path in shaders_dir.rglob(f'*{old_ext}'):
            new_path = file_path.with_suffix(new_ext)
            
            if dry_run:
                print(f"[DRY RUN] Would rename: {file_path.relative_to(shaders_dir)} -> {new_path.name}")
            else:
                print(f"Renaming: {file_path.relative_to(shaders_dir)} -> {new_path.name}")

                # if new file already exists, just remove the old one
                if new_path.exists():
                    file_path.unlink()
                else:
                    file_path.rename(new_path)
            
            renamed_count += 1

    action = "Would rename" if dry_run else "Renamed"
    print(f"\n{action} {renamed_count} file(s).")


def main():
    # Determine the repo root (tools/scripts -> go up two levels)
    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent.parent
    shaders_dir = repo_root / 'res' / 'shaders'

    # Check for --dry-run flag
    dry_run = '--dry-run' in sys.argv or '-n' in sys.argv

    if dry_run:
        print("=== DRY RUN MODE (no files will be renamed) ===\n")

    print(f"Shader directory: {shaders_dir}\n")
    rename_shaders(shaders_dir, dry_run=dry_run)


if __name__ == '__main__':
    main()
