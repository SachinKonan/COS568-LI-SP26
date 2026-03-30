#!/bin/bash
# Task 1 all-in-one SLURM script: builds the project, generates workloads,
# runs benchmarks for B+Tree / DynamicPGM / LIPP on fb/books/osmc datasets,
# adds CSV headers, and runs the analysis script.
#
# NOTE: This script was our first attempt that tried to build inside the job.
# It failed because Boost 1.85.0 (installed at /usr/local/boost/1.85.0) was
# not discoverable by cmake on compute nodes even with explicit BOOST_ROOT,
# BOOST_INCLUDEDIR, and BOOST_LIBRARYDIR flags. The FindBoost cmake module
# kept reporting "Could NOT find Boost (missing: chrono)".
#
# Additionally, the initial 16GB memory allocation caused OOM kills — LIPP
# indexing 100M keys requires ~28GB RAM.
#
# Resolution: We switched to building on the login node (where module load
# boost/1.85.0 works correctly), then using task1_bench.slurm for benchmarks
# only with 64GB memory.
#
# Hardware used (Della cluster):
#   Node: della-h16n14
#   CPU: AMD EPYC Genoa (4th gen), 2 sockets x 96 cores = 192 cores total
#   Memory: 1.5 TB total, 64 GB allocated
#   Partition: cpu
#   Wall time: ~27 minutes
#
#SBATCH --job-name=task1_benchmark
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem=16G
#SBATCH --time=01:00:00
#SBATCH --partition=cpu
#SBATCH --output=task1_%j.out
#SBATCH --error=task1_%j.err

set -e

cd /scratch/gpfs/ZHUANGL/sk7524/COS568-LI-SP26

# Load required modules
module purge
module load anaconda3/2023.3
module load boost/1.85.0

# Explicitly set boost paths in case module doesn't propagate
export BOOST_ROOT=/usr/local/boost/1.85.0
export CMAKE_PREFIX_PATH=/usr/local/boost/1.85.0:$CMAKE_PREFIX_PATH
export LD_LIBRARY_PATH=/usr/local/boost/1.85.0/lib64:$LD_LIBRARY_PATH
export CPATH=/usr/local/boost/1.85.0/include:$CPATH

echo "=== Step 1: Building benchmark ==="
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release \
    -DBOOST_ROOT=/usr/local/boost/1.85.0 \
    -DBOOST_INCLUDEDIR=/usr/local/boost/1.85.0/include \
    -DBOOST_LIBRARYDIR=/usr/local/boost/1.85.0/lib64 \
    -DBoost_NO_SYSTEM_PATHS=ON ..
make -j 8
cd ..

echo "=== Step 2: Generating workloads ==="
cd build
for dataset in fb_100M_public_uint64 books_100M_public_uint64 osmc_100M_public_uint64; do
    echo "Generating operations for $dataset"
    ./generate ../data/$dataset 2000000 --negative-lookup-ratio 0.5
    ./generate ../data/$dataset 2000000 --insert-ratio 0.5 --negative-lookup-ratio 0.5
    ./generate ../data/$dataset 2000000 --insert-ratio 0.9 --negative-lookup-ratio 0.5 --mix
    ./generate ../data/$dataset 2000000 --insert-ratio 0.1 --negative-lookup-ratio 0.5 --mix
done
cd ..

echo "=== Step 3: Running benchmarks ==="
BENCHMARK=build/benchmark
mkdir -p results

for DATA in fb_100M_public_uint64 books_100M_public_uint64 osmc_100M_public_uint64; do
    for INDEX in LIPP BTree DynamicPGM; do
        echo "Executing operations for $DATA and index $INDEX"
        echo "Executing lookup-only workload"
        $BENCHMARK ./data/$DATA ./data/${DATA}_ops_2M_0.000000rq_0.500000nl_0.000000i --through --csv --only $INDEX -r 3
        echo "Executing insert+lookup workload"
        $BENCHMARK ./data/$DATA ./data/${DATA}_ops_2M_0.000000rq_0.500000nl_0.500000i_0m --through --csv --only $INDEX -r 3
        echo "Executing insert+lookup mixed workload with insert-ratio 0.9"
        $BENCHMARK ./data/$DATA ./data/${DATA}_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix --through --csv --only $INDEX -r 3
        echo "Executing insert+lookup mixed workload with insert-ratio 0.1"
        $BENCHMARK ./data/$DATA ./data/${DATA}_ops_2M_0.000000rq_0.500000nl_0.100000i_0m_mix --through --csv --only $INDEX -r 3
    done
done

echo "=== Step 4: Adding CSV headers ==="
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

echo "=== Step 5: Running analysis ==="
python scripts/analysis.py

echo "=== Task 1 Complete ==="
echo "Results are in results/ and analysis_results/"
echo "Plot saved as benchmark_results.png"
