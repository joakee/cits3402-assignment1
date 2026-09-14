#!/bin/bash
#SBATCH --cpus-per-task=64
#SBATCH --time=00:15:00
#SBATCH --mem=8G
#SBATCH --partition=cits3402

./openmp_implementation test_files/3_giga_a.pdf test_files/3_giga_b.pdf 64
