#!/bin/bash
# Task 1 benchmark-only SLURM script. Assumes the project is already built
# (cmake + make done on login node) and workloads are already generated.
#
# Runs all 4 workloads (lookup-only, insert+lookup, mixed 90% insert,
# mixed 10% insert) across 3 datasets (fb, books, osmc) and 3 indexes
# (LIPP, BTree, DynamicPGM), with 3 repetitions each (-r 3).
# Then adds CSV headers to the result files.
#
# Prerequisites (run on login node before submitting):
#   1. scripts/download_dataset.sh  — downloads fb/books/osmc to data/
#   2. scripts/create_minimal_cmake.sh — creates CMakeLists.txt
#   3. scripts/generate_workloads.sh — builds project and generates workloads
#      (or manually: mkdir build && cd build && cmake .. && make -j8,
#       then run ./generate for each dataset/workload combo)
#
# Memory: 64GB required (LIPP on 100M keys uses ~28GB; 16GB causes OOM)
#
# Hardware (Della cluster, job 6220151):
#   Node: della-h16n14
#   CPU: AMD EPYC Genoa (4th gen), 2 sockets x 96 cores, 1 thread/core
#   Memory: 1.5 TB total, 64 GB allocated (peak usage ~28 GB)
#   Partition: cpu
#   Wall time: 26 min 30 sec
#
#SBATCH --job-name=task1_bench
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem=64G
#SBATCH --time=01:00:00
#SBATCH --partition=cpu
#SBATCH --output=task1_bench_%j.out
#SBATCH --error=task1_bench_%j.err

set -e

cd /scratch/gpfs/ZHUANGL/sk7524/COS568-LI-SP26

# Set library path for boost
export LD_LIBRARY_PATH=/usr/local/boost/1.85.0/lib64:$LD_LIBRARY_PATH

BENCHMARK=build/benchmark
mkdir -p results

for DATA in fb_100M_public_uint64 books_100M_public_uint64 osmc_100M_public_uint64; do
    for INDEX in LIPP BTree DynamicPGM; do
        echo "=== $DATA / $INDEX ==="
        echo "Lookup-only"
        $BENCHMARK ./data/$DATA ./data/${DATA}_ops_2M_0.000000rq_0.500000nl_0.000000i --through --csv --only $INDEX -r 3
        echo "Insert+Lookup"
        $BENCHMARK ./data/$DATA ./data/${DATA}_ops_2M_0.000000rq_0.500000nl_0.500000i_0m --through --csv --only $INDEX -r 3
        echo "Mixed 90% insert"
        $BENCHMARK ./data/$DATA ./data/${DATA}_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix --through --csv --only $INDEX -r 3
        echo "Mixed 10% insert"
        $BENCHMARK ./data/$DATA ./data/${DATA}_ops_2M_0.000000rq_0.500000nl_0.100000i_0m_mix --through --csv --only $INDEX -r 3
    done
done

echo "=== Adding CSV headers ==="
for FILE in ./results/*.csv; do
    if [[ $FILE == *0.000000i* ]]; then
        if head -n 1 $FILE | grep -q "index_name"; then
            sed -i '1d' $FILE
        fi
        sed -i '1s/^/index_name,build_time_ns1,build_time_ns2,build_time_ns3,index_size_bytes,lookup_throughput_mops1,lookup_throughput_mops2,lookup_throughput_mops3,search_method,value\n/' $FILE
    elif [[ $FILE == *mix* ]]; then
        if head -n 1 $FILE | grep -q "index_name"; then
            sed -i '1d' $FILE
        fi
        sed -i '1s/^/index_name,build_time_ns1,build_time_ns2,build_time_ns3,index_size_bytes,mixed_throughput_mops1,mixed_throughput_mops2,mixed_throughput_mops3,search_method,value\n/' $FILE
    else
        if head -n 1 $FILE | grep -q "index_name"; then
            sed -i '1d' $FILE
        fi
        sed -i '1s/^/index_name,build_time_ns1,build_time_ns2,build_time_ns3,index_size_bytes,insert_throughput_mops1,lookup_throughput_mops1,insert_throughput_mops2,lookup_throughput_mops2,insert_throughput_mops3,lookup_throughput_mops3,search_method,value\n/' $FILE
    fi
    echo "Header set for $FILE"
done

echo "=== Benchmarking complete ==="
