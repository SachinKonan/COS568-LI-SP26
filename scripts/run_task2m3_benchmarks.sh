#!/usr/bin/env bash
# Task 2 Milestone 3: Run mixed workload benchmarks on ALL 3 datasets
# for DynamicPGM, LIPP, and Hybrid (multiple threshold configs).

set -e

echo "=== Task 2 Milestone 3: Advanced Hybrid DPGM+LIPP Benchmarks ==="

BENCHMARK=build/benchmark
if [ ! -f $BENCHMARK ]; then
    echo "benchmark binary does not exist. Run: cd build && cmake .. && make -j"
    exit 1
fi

mkdir -p ./results

# Clear old mixed-workload results (stale data from pre-DCE fix)
echo "Clearing old mixed-workload result CSVs..."
rm -f ./results/*mix*results_table.csv

for DATA in fb_100M_public_uint64 books_100M_public_uint64 osmc_100M_public_uint64; do
    for INDEX in DynamicPGM LIPP Hybrid; do
        echo "=== $DATA / $INDEX ==="
        echo "Mixed 90% insert (insertion-heavy)"
        $BENCHMARK ./data/$DATA ./data/${DATA}_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix --through --csv --only $INDEX -r 3
        echo "Mixed 10% insert (lookup-heavy)"
        $BENCHMARK ./data/$DATA ./data/${DATA}_ops_2M_0.000000rq_0.500000nl_0.100000i_0m_mix --through --csv --only $INDEX -r 3
    done
done

echo "=== Adding CSV headers ==="
for FILE in ./results/*mix*results_table.csv; do
    if [ ! -f "$FILE" ]; then continue; fi
    # Remove existing header if present
    if head -n 1 "$FILE" | grep -q "index_name"; then
        sed -i '1d' "$FILE"
    fi
    sed -i '1s/^/index_name,build_time_ns1,build_time_ns2,build_time_ns3,index_size_bytes,mixed_throughput_mops1,mixed_throughput_mops2,mixed_throughput_mops3,search_method,value\n/' "$FILE"
    echo "Header set for $FILE"
done

echo "=== Task 2 Milestone 3 Benchmarking complete! ==="
