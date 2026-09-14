#!/bin/bash
#SBATCH --cpus-per-task=16
#SBATCH --time=00:15:00
#SBATCH --mem=8G
#SBATCH --partition=cits3402

export OMP_NUM_THREADS=16
./openmp_implementation test_files/2_mega_a.pdf test_files/2_mega_b.pdf 22709404
