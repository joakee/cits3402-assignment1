#!/usr/bin/env bash

# runs openmp_implementation on every x_name_a.pdf / x_name_b.pdf pair in
# test_files/, one pair at a time. A failed pair gets reported but
# doesn't stop the rest from running.
#
# usage: ./run_all_openmp.sh [student_id]   (default 012345678)
# exits 0 if every pair solved, 1 if any failed, 2 on setup error
#------------------------------
set -uo pipefail

sn="${1:-012345678}"

# the repo root has gotta be wherever this script lives
repo="$(cd "$(dirname "$0")" && pwd)"
cd "$repo" || exit 2

# rebuild if the source has changed (no-op otherwise)
make openmp_implementation || exit 2

shopt -s nullglob
pairs=(test_files/*_a.pdf)
if [ "${#pairs[@]}" -eq 0 ]; then
    echo "error: no *_a.pdf files in test_files/" >&2
    exit 2
fi

failed=()
for a in "${pairs[@]}"; do
    b="${a%_a.pdf}_b.pdf"
    if [ ! -f "$b" ]; then
        echo "skipping $a: no matching $b" >&2
        failed+=("$a")
        continue
    fi

    echo "=============================="
    echo "./openmp_implementation $a $b $sn"
    if ! ./openmp_implementation "$a" "$b" "$sn"; then
        echo "FAILED: $a $b" >&2
        failed+=("$a")
    fi
done

echo "=============================="
if [ "${#failed[@]}" -eq 0 ]; then
    echo "all ${#pairs[@]} pairs solved"
    exit 0
else
    echo "${#failed[@]}/${#pairs[@]} pairs failed: ${failed[*]}" >&2
    exit 1
fi
