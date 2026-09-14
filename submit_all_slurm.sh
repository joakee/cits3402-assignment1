#!/usr/bin/env bash
# usage: ./submit_all_slurm.sh

set -uo pipefail

# the repo root has gotta be wherever this script lives
repo="$(cd "$(dirname "$0")" && pwd)"
cd "$repo" || exit 1

# rebuild if the source has changed (no-op otherwise)
make openmp_implementation || exit 1

for f in slurm/*.slurm; do
    sbatch -J "$(basename "$f" .slurm)" -o "slurm/%x_%j.out" "$f"
done
