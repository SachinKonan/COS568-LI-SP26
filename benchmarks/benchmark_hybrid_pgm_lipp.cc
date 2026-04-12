#include "benchmarks/benchmark_hybrid_pgm_lipp.h"

#include "benchmark.h"
#include "competitors/hybrid_pgm_lipp.h"

void benchmark_64_hybrid(tli::Benchmark<uint64_t>& benchmark) {
  benchmark.template Run<HybridPGMLIPP<uint64_t>>();
}
