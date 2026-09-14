#!/bin/bash
#SBATCH --cpus-per-task=96
#SBATCH --time=00:15:00
#SBATCH --mem=8G
#SBATCH --partition=cits3402

./openmp_implementation test_files/4_tera_a.pdf test_files/4_tera_b.pdf 96
