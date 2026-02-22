#!/usr/bin/env bash
# reorder-pragma.sh
# Ensure the copyright comment (/* … */ or // …) appears before #pragma once.
# Also collapses multiple blank lines into one in the edited portion.
# macOS-only core utilities: bash, find, awk, diff, mv, rm, cmp.

set -euo pipefail

###############################################################################
# Options
###############################################################################
SRC_DIR="src"                        # root directory to scan
LOG_FILE="pragma_reorder.log"
APPLY=0                              # 0 = dry-run (default), 1 = modify

usage() {
  cat <<EOF
Usage: $0 [--apply|-a] [--src-dir PATH] [--log-file FILE]

  --apply, -a      Overwrite headers; dry-run if omitted
  --src-dir PATH   Root directory to scan (default: ./src)
  --log-file FILE  Where to write per-file diffs (default: pragma_reorder.log)
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

echo "Pragma-ordering log – $(date)" > "$LOG_FILE"
echo "Mode: $([[ $APPLY -eq 1 ]] && echo 'APPLY' || echo 'DRY-RUN')" >> "$LOG_FILE"
echo >> "$LOG_FILE"

###############################################################################
# Main loop
###############################################################################
find "$SRC_DIR" -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.hh' \) |
while IFS= read -r file; do
  # Only bother if the file contains #pragma once
  grep -q -E '^[[:space:]]*#pragma[[:space:]]+once' "$file" || continue

  # Transform with AWK --------------------------------------------------------
  awk '
  # Utilities -----------------------------------------------------------------
  function blank(l) { return (l ~ /^[[:space:]]*$/) }
  function comment_start(l) {
    return (l ~ /^[[:space:]]*\/\*/ || l ~ /^[[:space:]]*\/\//)
  }
  function is_pragma(l) {
    return (l ~ /^[[:space:]]*#pragma[[:space:]]+once/)
  }

  # Pass 1 – read every line ---------------------------------------------------
  { lines[NR] = $0 }

  END {
    n = NR
    # Skip leading blanks -----------------------------------------------------
    i = 1
    while (i <= n && blank(lines[i])) i++

    # If pragma already *after* a comment, nothing to do ----------------------
    if (comment_start(lines[i])) {   # comment block already first
      print_lines(lines, n)          # helper defined below
      exit
    }

    # If the first non-blank is #pragma once, try swapping -------------------
    if (!is_pragma(lines[i])) {      # some other content; leave untouched
      print_lines(lines, n)
      exit
    }

    pragma_idx = i

    # Consume any blank lines after #pragma once ------------------------------
    j = pragma_idx + 1
    while (j <= n && blank(lines[j])) j++

    # If the next token is not a comment, leave untouched --------------------
    if (!comment_start(lines[j])) {
      print_lines(lines, n)
      exit
    }

    # Identify full comment block starting at j ------------------------------
    comment_start_idx = j
    if (lines[j] ~ /^[[:space:]]*\/\*/) {      # /* … */ variant
      while (j <= n && lines[j] !~ /\*\//) j++
    } else {                                   # consecutive // … lines
      while (j <= n && lines[j] ~ /^[[:space:]]*\/\//) j++
      j--
    }
    comment_end_idx = j

    # Build output ------------------------------------------------------------
    # 1. Comment block
    for (k = comment_start_idx; k <= comment_end_idx; k++) print lines[k]

    # 2. One blank line
    print ""

    # 3. #pragma once
    print lines[pragma_idx]

    # 4. One blank line (if not immediately followed by another)
    #    Determine first content line after comment & pragma block
    next_idx = pragma_idx + 1
    while (next_idx <= n && blank(lines[next_idx])) next_idx++
    if (next_idx <= n) print ""

    # 5. Everything else, skipping original comment & pragma positions -------
    prior_blank = 0
    for (k = 1; k <= n; k++) {
      if (k >= comment_start_idx && k <= comment_end_idx) continue
      if (k == pragma_idx) continue
      # Collapse multiple blank lines in remainder
      if (blank(lines[k])) {
        if (!prior_blank) print ""
        prior_blank = 1
      } else {
        print lines[k]
        prior_blank = 0
      }
    }
  }

  # Helper to echo file untouched ---------------------------------------------
  function print_lines(arr, total,   ii) {
    prior = 0
    for (ii = 1; ii <= total; ii++) {
      if (blank(arr[ii])) {
        if (!prior) print ""         # collapse successive blanks
        prior = 1
      } else {
        print arr[ii]
        prior = 0
      }
    }
  }' "$file" > "${file}.converted"

  # Compare & possibly keep ---------------------------------------------------
  if cmp -s "$file" "${file}.converted"; then
    rm "${file}.converted"
    continue
  fi

  # Log diff ------------------------------------------------------------------
  {
    echo "=================================================================="
    echo "File: $file"
    diff -u "$file" "${file}.converted" || true
    echo
  } >> "$LOG_FILE"

  if [[ $APPLY -eq 1 ]]; then
    mv "${file}.converted" "$file"
  else
    rm "${file}.converted"
  fi
done

echo "Done. Review $LOG_FILE for details."
[[ $APPLY -eq 1 ]] && echo "Headers updated in place." || \
                      echo "No files were modified (dry-run)."
