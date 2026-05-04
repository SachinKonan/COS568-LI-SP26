#include "benchmarks/benchmark_hybrid_pgm_lipp.h"

#include "benchmark.h"
#include "competitors/hybrid_pgm_lipp.h"

void benchmark_64_hybrid(tli::Benchmark<uint64_t>& benchmark) {
  // === Lookup-heavy variants (mix10) ===

  // Hash bloom — sweep size + hash count
  benchmark.template Run<HybridLookup<uint64_t, 30, 1>>();   // 128MB k=1
  benchmark.template Run<HybridLookup<uint64_t, 32, 1>>();   // 512MB k=1
  benchmark.template Run<HybridLookup<uint64_t, 33, 1>>();   // 1GB k=1

  // Prefix-occupancy filter — sweep prefix bits, including very large
  benchmark.template Run<HybridLookupPrefix<uint64_t, 24>>();  // 2MB
  benchmark.template Run<HybridLookupPrefix<uint64_t, 28>>();  // 32MB
  benchmark.template Run<HybridLookupPrefix<uint64_t, 30>>();  // 128MB
  benchmark.template Run<HybridLookupPrefix<uint64_t, 32>>();  // 512MB
  benchmark.template Run<HybridLookupPrefix<uint64_t, 33>>();  // 1GB

  // === Insert-heavy variants (mix90) ===
  benchmark.template Run<HybridInsert<uint64_t, 32>>();
  benchmark.template Run<HybridInsert<uint64_t, 64>>();
  benchmark.template Run<HybridInsert<uint64_t, 128>>();

  // === Async double-buffered fallback ===
  benchmark.template Run<HybridPGMLIPP<uint64_t>>({10000000});
}
