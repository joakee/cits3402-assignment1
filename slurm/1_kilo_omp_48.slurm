#!/bin/bash
#SBATCH --cpus-per-task=48
#SBATCH --time=00:15:00
#SBATCH --mem=8G
#SBATCH --partition=cits3402

./openmp_implementation test_files/1_kilo_a.pdf test_files/1_kilo_b.pdf 48
