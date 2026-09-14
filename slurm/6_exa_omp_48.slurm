#!/bin/bash
#SBATCH --cpus-per-task=48
#SBATCH --time=00:15:00
#SBATCH --mem=8G
#SBATCH --partition=cits3402

export OMP_NUM_THREADS=48
./openmp_implementation test_files/6_exa_a.pdf test_files/6_exa_b.pdf 22709404
