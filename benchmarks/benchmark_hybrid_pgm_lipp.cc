#include "benchmarks/benchmark_hybrid_pgm_lipp.h"

#include "benchmark.h"
#include "competitors/hybrid_pgm_lipp.h"

void benchmark_64_hybrid(tli::Benchmark<uint64_t>& benchmark) {
  // Small threshold: aggressive flushing (lookup-optimized via small DPGM)
  benchmark.template Run<HybridPGMLIPP<uint64_t>>({50000});
  // Large threshold: minimize flush overhead (insert-optimized)
  benchmark.template Run<HybridPGMLIPP<uint64_t>>({500000});
  // Very large threshold: effectively never flush during 2M-op benchmark
  // (Hybrid becomes DPGM-for-inserts + bloom-filtered-LIPP-for-lookups)
  benchmark.template Run<HybridPGMLIPP<uint64_t>>({10000000});
}
