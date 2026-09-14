#!/bin/bash
#SBATCH --job-name=node_info
#SBATCH --cpus-per-task=48
#SBATCH --time=00:02:00
#SBATCH --mem=1G
#SBATCH --partition=cits3402
#SBATCH --output=node_info_%j.out

# prints the compute node's cpu/numa layout, plus how slurm laid out this
# job's 48-cpu allocation (one socket or split across both)
#
# usage (from the repo root): sbatch node_info.slurm
# to check a specific node:   sbatch --nodelist=<node> node_info.slurm
#
# kept out of slurm/ on purpose so submit_all_slurm.sh doesn't pick it up

echo "===== host: $(hostname)  job: ${SLURM_JOB_ID:-none}"

echo
echo "===== lscpu"
lscpu

echo
echo "===== numactl --hardware"
if command -v numactl > /dev/null; then
    numactl --hardware
else
    # numactl isn't always installed on compute nodes, the kernel has the same info
    echo "numactl not found, reading /sys instead"
    for node in /sys/devices/system/node/node[0-9]*; do
        echo "$(basename "$node") cpus: $(cat "$node/cpulist")"
        grep MemTotal "$node/meminfo"
    done
fi

echo
echo "===== this job's allocation"
echo "nproc: $(nproc)"
echo "cpus allowed: $(grep Cpus_allowed_list /proc/self/status | cut -f2)"
if command -v numactl > /dev/null; then
    numactl --show
fi
