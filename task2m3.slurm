#!/bin/bash
# Task 2 Milestone 3: Advanced Hybrid DPGM+LIPP benchmark
# Runs mixed workloads (10% and 90% insert) on ALL 3 datasets
# for DynamicPGM, LIPP, and Hybrid (multiple threshold configs).
#
# Prerequisites:
#   1. Datasets downloaded: data/{fb,books,osmc}_100M_public_uint64
#   2. Workloads generated for all datasets
#   3. Project built: cd build && cmake .. && make -j
#
#SBATCH --job-name=task2m3_bench
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=48G
#SBATCH --time=00:50:00
#SBATCH --partition=cpu
#SBATCH --output=task2m3_bench_%j.out
#SBATCH --error=task2m3_bench_%j.err

set -e

cd /scratch/gpfs/ZHUANGL/sk7524/COS568-LI-SP26

# Set library path for boost
export LD_LIBRARY_PATH=/usr/local/boost/1.85.0/lib64:$LD_LIBRARY_PATH

# Run benchmarks
bash scripts/run_task2m3_benchmarks.sh

# Run analysis
module load anaconda3/2023.3
python scripts/task2m3_analysis.py

echo "=== Task 2 Milestone 3 complete ==="
