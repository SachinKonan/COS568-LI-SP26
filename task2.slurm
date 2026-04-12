#!/bin/bash
# Task 2 Milestone 2: Hybrid DPGM+LIPP benchmark
# Runs mixed workloads (10% and 90% insert) on Facebook dataset
# for DynamicPGM, LIPP, and Hybrid indexes.
#
# Prerequisites (run on login node before submitting):
#   1. Datasets downloaded: data/fb_100M_public_uint64
#   2. Workloads generated: data/fb_100M_public_uint64_ops_2M_*mix*
#   3. Project built: cd build && cmake .. && make -j
#
#SBATCH --job-name=task2_bench
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem=64G
#SBATCH --time=01:00:00
#SBATCH --partition=cpu
#SBATCH --output=task2_bench_%j.out
#SBATCH --error=task2_bench_%j.err

set -e

cd /scratch/gpfs/ZHUANGL/sk7524/COS568-LI-SP26

# Set library path for boost
export LD_LIBRARY_PATH=/usr/local/boost/1.85.0/lib64:$LD_LIBRARY_PATH

# Run benchmarks
bash scripts/run_task2_benchmarks.sh

# Run analysis
module load anaconda3/2023.3
python scripts/task2_analysis.py

echo "=== Task 2 Milestone 2 complete ==="
