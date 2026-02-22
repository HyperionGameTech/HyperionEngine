#!/usr/bin/env bash
# convert-guards.sh
# Replace traditional include guards with #pragma once.
# Dry-runs by default and writes a patch-style diff to a log file.
# macOS-only core utilities: bash, find, awk, diff, mv, cp, rm, cmp.

set -euo pipefail

###############################################################################
# Parameters
###############################################################################
SRC_DIR="src"                        # root directory to scan
LOG_FILE="include_guard_conversion.log"
APPLY=0                              # 0 = dry-run, 1 = write changes

usage() {
  cat <<EOF
Usage: $0 [--apply|-a] [--src-dir PATH] [--log-file FILE]

  --apply, -a      Actually overwrite headers (dry-run otherwise)
  --src-dir PATH   Root directory to scan (default: ./src)
  --log-file FILE  File where diffs are written (default: include_guard_conversion.log)
EOF
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -a|--apply)   APPLY=1 ;;
    --src-dir)    SRC_DIR=$2;  shift ;;
    --log-file)   LOG_FILE=$2; shift ;;
    -h|--help)    usage ;;
    *)            usage ;;
  esac
  shift
done

# start / truncate log
echo "Include-guard conversion log – $(date)" > "$LOG_FILE"
echo "Mode: $([[ $APPLY -eq 1 ]] && echo 'APPLY' || echo 'DRY-RUN')" >> "$LOG_FILE"
echo >> "$LOG_FILE"

###############################################################################
# Processing loop
###############################################################################
find "$SRC_DIR" -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.hh' \) |
while IFS= read -r file; do
  # Skip files that already contain #pragma once
  if grep -q -E '^[[:space:]]*#pragma[[:space:]]+once' "$file"; then
    continue
  fi

  # Transform with AWK --------------------------------------------------------
  awk '
  BEGIN {
    guard_seen = 0; depth = 0;
    guard_start1 = guard_start2 = guard_end = 0;
  }
  # Identify outer guard ------------------------------------------------------
  function is_if(l) { return (l ~ /^[[:space:]]*#(if|ifdef|ifndef)/) }

  {
    lines[NR] = $0
    if (!guard_seen) {
      if ($0 ~ /^[[:space:]]*#ifndef[[:space:]]+[A-Za-z0-9_]+/) {
        guard_macro = $2; guard_start1 = NR; next_expect_define = NR + 1; next
      }
      if (NR == next_expect_define && guard_macro && \
          $0 ~ /^[[:space:]]*#define[[:space:]]+[A-Za-z0-9_]+/) {
        guard_start2 = NR; guard_seen = 1; depth = 1; next
      }
    } else if (guard_seen == 1) {
      if (is_if($0))              depth++
      else if ($0 ~ /^[[:space:]]*#endif/) {
        depth--
        if (depth == 0) { guard_end = NR; guard_seen = 2 }
      }
    }
  }
  END {
    if (guard_start1 && guard_end) {
      print "#pragma once"
      for (i = 1; i <= NR; i++) {
        if (i == guard_start1 || i == guard_start2 || i == guard_end) continue
        print lines[i]
      }
    } else {
      for (i = 1; i <= NR; i++) print lines[i]
    }
  }' "$file" > "${file}.converted" || true

  # Compare and act -----------------------------------------------------------
  if cmp -s "$file" "${file}.converted"; then
    rm "${file}.converted"               # nothing changed
    continue
  fi

  # Log diff for this file ----------------------------------------------------
  {
    echo "=================================================================="
    echo "File: $file"
    diff -u "$file" "${file}.converted" || true
    echo
  } >> "$LOG_FILE"

  # Apply if requested --------------------------------------------------------
  if [[ $APPLY -eq 1 ]]; then
    mv "${file}.converted" "$file"
  else
    rm "${file}.converted"
  fi
done

echo "Done. Full diff is in $LOG_FILE."
[[ $APPLY -eq 1 ]] && echo "Headers updated in place." || \
                      echo "No files were modified (dry-run)."
