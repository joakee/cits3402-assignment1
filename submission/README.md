# Brute Force Hash Birthday Attack in Parallel

#### Built by: James Oakey (22709404) & Brent Tankink (23842541)

<br>

`serial_implementation.c` and `openmp_implementation.c` perform serial and parallel OpenMP birthday attacks on a pair of PDFs. They find nonce values for both PDFs which create a hash collision.

It takes two PDF files as inputs, and an optional third argument `thread_count` to specify the number of threads the program should use (if this number is not specified, it defaults to the host system's maximum number of available threads.

## `make`-ing

There are numerous options for running the Makefile.

```bash
# Building both `serial_implementation.c` and `openmp_implementation.c`
$ make all

# Building only serial
$ make serial_implementation

# Building only parallel
$ make openmp_implementation

# Removing both implementation executable files
$ make clean
```

## Running on Local Machine

After running any of the above `make` commands, the following is the structure of how to run the `.c` files.

```bash
# Serial (single-threaded only)
$ ./serial_implementation <path/to/pdf1> <path/to/pdf2>

# Parallel (optional thread_count argument)
$ ./openmp_implementation <path/to/pdf1> <path/to/pdf2> <thread_count>
```

Upon successful execution of the above commands, the output PDFs with the modified hashes will be written to the `solved` directory (created during execution) as `solved/<pdf1>` and `solved/<pdf2>`.

## Running on Kaya (UWA HPC Cluster)

To run the parallel implementation on Kaya, a batch script of the following structure should be used.

```bash
#!/bin/bash
#SBATCH --cpus-per-task=<thread_count>
#SBATCH --time=00:15:00
#SBATCH --mem=8G
#SBATCH --partition=cits3402

./openmp_implementation <path/to/pdf1> <path/to/pdf2> <thread_count>
```

All `<sbatch_file_name>.slurm` files which have been run on the cluster will have their output written to `<sbatch_file_name>_<job_id>.out`. Each `.out` file will be written to the same directory which contains the `.slurm` file.

## Collision Verification

Upon successful completion either implementation program, the solved PDF nonces can be verified using the script `verify_collisions.sh`.

```bash
$ ./verify_collisions.sh <path/to/solved_pdf1> <path/to/solved_pdf2>
```

This will return `MATCH` for successful verification, `NO MATCH` otherwise, or `error` if a fatal program error occurred.

## Report & Results Summary

All theory surrounding the building of the program, all results from testing, and the analysis are contained in `CITS3402-Report-22709404-23842541.pdf`.