#!/bin/bash
#SBATCH --job-name=numa_test
#SBATCH --cpus-per-task=96
#SBATCH --time=00:30:00
#SBATCH --mem=8G
#SBATCH --partition=cits3402
#SBATCH --output=numa_test_%j.out

# separates the cost of remote memory from the cost of spreading threads
# across both cpus, on a 2-socket node (node 0 = cpus 0-47, node 1 = 48-95).
# every run uses 48 threads, so schedule(static) hands out the same chunks and
# they should find the same collision, which makes elapsed times comparable
#
#   local   threads on cpu 0, all memory on node 0 (baseline)
#   remote  threads on cpu 0, all memory on node 1 (only the memory moved)
#   spread  24 threads on each cpu, memory lands wherever it's first touched
#
# the whole node is allocated (96 cpus) so the threads can be placed by hand
#
# usage (from the repo root): sbatch numa_test.slurm
# kept out of slurm/ on purpose so submit_all_slurm.sh doesn't pick it up

REPEATS=3
THREADS=48
PROG=(./openmp_implementation test_files/1_kilo_a.pdf test_files/1_kilo_b.pdf "$THREADS")

# one thread per core in every run, so pinning itself isn't a difference
export OMP_PLACES=cores
export OMP_AFFINITY_FORMAT="  thread %n -> cpu %A"

echo "host: $(hostname)  job: ${SLURM_JOB_ID:-none}"
numactl --show

summary=()
for rep in $(seq 1 "$REPEATS"); do
    # list each thread's cpu on the first repeat only, to confirm placement
    if [ "$rep" -eq 1 ]; then
        export OMP_DISPLAY_AFFINITY=true
    else
        export OMP_DISPLAY_AFFINITY=false
    fi

    # configs interleaved, any drift on the node hits all three equally
    for config in local remote spread; do
        case "$config" in
            local)  cmd=(env OMP_PROC_BIND=close numactl --cpunodebind=0 --membind=0 "${PROG[@]}") ;;
            remote) cmd=(env OMP_PROC_BIND=close numactl --cpunodebind=0 --membind=1 "${PROG[@]}") ;;
            spread) cmd=(env OMP_PROC_BIND=spread "${PROG[@]}") ;;
        esac

        echo
        echo "===== $config, repeat $rep: ${cmd[*]}"
        out="$("${cmd[@]}" 2>&1)"
        echo "$out"

        elapsed="$(awk -F': *' '/Elapsed nonce calculation time/ { print $2 }' <<< "$out")"
        nonce1="$(grep -o 'nonce1=[0-9]*' <<< "$out" | cut -d= -f2)"
        summary+=("$(printf '%-7s %3d %12s %12s' "$config" "$rep" "${elapsed:-FAILED}" "${nonce1:--}")")
    done
done

echo
echo "===== summary (nonce1 should match in every row)"
printf '%-7s %3s %12s %12s\n' config rep elapsed_s nonce1
printf '%s\n' "${summary[@]}"
