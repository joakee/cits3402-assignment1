#!/usr/bin/env bash

# runs openmp_implementation on every x_name_a.pdf / x_name_b.pdf pair in
# test_files/, one pair at a time. A failed pair gets reported but
# doesn't stop the rest from running. Times for each run go to
# logs/openmp_<timestamp>.log.
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

mkdir -p logs || exit 2
log="logs/openmp_$(date +%Y%m%d_%H%M%S).log"

# each run's output is kept here too, so the program's own timing line
# can be pulled out after it finishes
out="$(mktemp)" || exit 2
trap 'rm -f "$out"' EXIT

# wall = whole process as timed by this script (includes file loading,
# the table calloc and writing solved/), nonce_calc = the program's own
# "Elapsed nonce calculation time" figure
{
    echo "# openmp_implementation, started $(date '+%Y-%m-%d %H:%M:%S')"
    echo "# host=$(hostname) student_id=$sn OMP_NUM_THREADS=${OMP_NUM_THREADS:-unset} nproc=$(nproc)"
    printf '%-12s %-7s %12s %14s\n' pair status wall_s nonce_calc_s
} > "$log"

failed=()
for a in "${pairs[@]}"; do
    b="${a%_a.pdf}_b.pdf"
    name="$(basename "${a%_a.pdf}")"
    if [ ! -f "$b" ]; then
        echo "skipping $a: no matching $b" >&2
        printf '%-12s %-7s %12s %14s\n' "$name" missing - - >> "$log"
        failed+=("$a")
        continue
    fi

    echo "=============================="
    echo "./openmp_implementation $a $b $sn"

    start="$(date +%s.%N)"
    # tee's status is ignored thanks to pipefail, so this is the program's
    ./openmp_implementation "$a" "$b" "$sn" 2>&1 | tee "$out"
    rc=$?
    end="$(date +%s.%N)"

    wall="$(awk -v s="$start" -v e="$end" 'BEGIN { printf "%.3f", e - s }')"
    calc="$(awk -F': *' 'tolower($0) ~ /elapsed nonce calculation time/ { printf "%.3f", $2 }' "$out")"

    if [ "$rc" -eq 0 ]; then
        status=ok
    else
        status="fail($rc)"
        echo "FAILED: $a $b" >&2
        failed+=("$a")
    fi
    printf '%-12s %-7s %12s %14s\n' "$name" "$status" "$wall" "${calc:--}" >> "$log"
done

echo "=============================="
if [ "${#failed[@]}" -eq 0 ]; then
    echo "all ${#pairs[@]} pairs solved"
    rc=0
else
    echo "${#failed[@]}/${#pairs[@]} pairs failed: ${failed[*]}" >&2
    rc=1
fi
echo "# finished $(date '+%Y-%m-%d %H:%M:%S')" >> "$log"
echo "times logged to $log"
exit "$rc"
