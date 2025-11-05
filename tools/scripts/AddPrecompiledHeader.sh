#!/usr/bin/env bash

# AddPrecompiledHeader.sh
# Adds `#include <HyperionPch.hpp>` to C++ translation units.
# Scope: All .cpp files under src/, excluding anything under src/core/.
# Placement: At the very top of the file, but after an initial comment block
#            (either /* ... */ or a contiguous run of // lines) if present.
# Safety: Skips files that already include HyperionPch.hpp. Supports dry-run.

set -u -o pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"

INCLUDE_LINE="#include <HyperionPch.hpp>"

DRY_RUN=0
VERBOSE=0

usage() {
	cat <<EOF
Usage: $(basename "$0") [options]

Options:
  -n, --dry-run     Don't modify files; just print what would change
  -v, --verbose     Print extra information
  -h, --help        Show this help

This script scans ${ROOT_DIR}/src for .cpp files, excluding any subdirectories
that contain a CMakeLists.txt (separate modules like core, script, etc.), and
inserts: ${INCLUDE_LINE}
after any initial top-of-file comment block, or at the top if none exists.
EOF
}

logv() { if [[ "$VERBOSE" -eq 1 ]]; then echo "$@"; fi; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        -n|--dry-run) DRY_RUN=1; shift ;;
        -v|--verbose) VERBOSE=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

# Find all immediate subdirectories under src/ that contain CMakeLists.txt (indicating separate modules)
EXCLUDE_DIRS=()
while IFS= read -r -d '' cmake_file; do
    dir=$(dirname "$cmake_file")
    # Only consider direct subdirectories of src/ (e.g., src/core, src/script)
    if [[ $(dirname "$dir") == "${ROOT_DIR}/src" ]]; then
        EXCLUDE_DIRS+=("$dir")
    fi
done < <(find "${ROOT_DIR}/src" -mindepth 2 -maxdepth 2 -type f -name "CMakeLists.txt" -print0)

# Build find prune arguments to completely skip module directories
FIND_PRUNES=()
for dir in "${EXCLUDE_DIRS[@]}"; do
    if [[ ${#FIND_PRUNES[@]} -gt 0 ]]; then
        FIND_PRUNES+=("-o")
    fi
    FIND_PRUNES+=("-path" "$dir" "-prune")
done

# Find all .cpp files under src/ excluding module directories entirely
# Use portable bash 3.x compatible approach instead of mapfile
FILES=()
if [[ ${#FIND_PRUNES[@]} -gt 0 ]]; then
    while IFS= read -r -d '' file; do
        FILES+=("$file")
    done < <(find "${ROOT_DIR}/src" \( "${FIND_PRUNES[@]}" \) -o \( -type f -name "*.cpp" -print0 \) | sort -z)
else
    while IFS= read -r -d '' file; do
        FILES+=("$file")
    done < <(find "${ROOT_DIR}/src" -type f -name "*.cpp" -print0 | sort -z)
fi

total=${#FILES[@]}
updated=0
skipped_already_present=0
skipped_error=0

logv "Excluded module directories: ${EXCLUDE_DIRS[*]}"

if [[ $total -eq 0 ]]; then
    echo "No .cpp files found under src/ (excluding module subdirectories). Nothing to do."
    exit 0
fi

has_pch_include() {
	# Returns 0 if file already includes HyperionPch.hpp
	# Match both <...> and "..."
	local f="$1"
	if grep -qE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]HyperionPch\.hpp[>"]' "$f"; then
		return 0
	fi
	return 1
}

insert_after_header_with_awk() {
	# Use awk to insert INCLUDE_LINE after an initial comment block or run of // lines.
	# Prints the modified content to stdout.
	awk -v include_line="$INCLUDE_LINE" '
	BEGIN { inserted=0; in_block=0; in_line_comments=0; n=0 }
	{
		if (!inserted) {
			# Buffer initial region until we decide where to insert.
			if (n == 0) {
				# We are at the top (no buffer yet). Allow leading blanks to be buffered
				if ($0 ~ /^[[:space:]]*$/) {
					buf[++n] = $0; next
				}

				# First non-blank line
				if ($0 ~ /^[[:space:]]*\/\*/) {
					in_block = 1; buf[++n] = $0
					# If block ends on same line
					if ($0 ~ /\*\//) {
						for (i=1;i<=n;i++) print buf[i]; print ""; print include_line; inserted=1
					}
					next
				} else if ($0 ~ /^[[:space:]]*\/\//) {
					in_line_comments = 1; buf[++n] = $0; next
				} else {
					# No comment header; insert at very top (before any leading blanks)
					print include_line; inserted=1
					for (i=1;i<=n;i++) print buf[i]
					print $0
					next
				}
			} else if (in_block) {
				buf[++n] = $0
				if ($0 ~ /\*\//) {
					for (i=1;i<=n;i++) print buf[i]; print ""; print include_line; inserted=1
				}
				next
			} else if (in_line_comments) {
				if ($0 ~ /^[[:space:]]*\/\//) {
					buf[++n] = $0; next
				} else {
					for (i=1;i<=n;i++) print buf[i]; print ""; print include_line; inserted=1
					# fall through to print current line
				}
			}
		}

		print $0
	}
	END {
		if (!inserted) {
			# File was empty or only had comments without closing? Insert at end.
			if (n > 0) {
				for (i=1;i<=n;i++) print buf[i]
			}
			print ""; print include_line
		}
	}'
}

process_file() {
	local f="$1"

	if has_pch_include "$f"; then
		logv "SKIP (already present): $f"
		((skipped_already_present++))
		return 0
	fi

	if [[ $DRY_RUN -eq 1 ]]; then
		echo "WOULD UPDATE: $f"
		((updated++))
		return 0
	fi

	local tmp
	if ! tmp=$(mktemp "${TMPDIR:-/tmp}/pch.XXXXXX"); then
		echo "Failed to create temporary file for $f" >&2
		((skipped_error++))
		return 1
	fi

	if ! insert_after_header_with_awk < "$f" > "$tmp"; then
		echo "Failed to process $f" >&2
		rm -f "$tmp"
		((skipped_error++))
		return 1
	fi

	# Only replace if content actually changed
	if cmp -s "$f" "$tmp"; then
		logv "NO CHANGE: $f"
		rm -f "$tmp"
		return 0
	fi

	mv "$tmp" "$f"
	echo "UPDATED: $f"
	((updated++))
}

for f in "${FILES[@]}"; do
	process_file "$f"
done

echo "---"
echo "Files scanned: $total"
echo "Updated: $updated"
echo "Skipped (already included): $skipped_already_present"
echo "Skipped (errors): $skipped_error"

exit 0

