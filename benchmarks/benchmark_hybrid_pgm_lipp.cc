#include "benchmarks/benchmark_hybrid_pgm_lipp.h"

#include "benchmark.h"
#include "competitors/hybrid_pgm_lipp.h"

void benchmark_64_hybrid(tli::Benchmark<uint64_t>& benchmark) {
  // === Lookup-heavy variants (mix10, applicable() filters on filename) ===

  // Hash bloom (k=1): single hash, sweep size
  benchmark.template Run<HybridLookup<uint64_t, 28, 1>>();   // 32MB
  benchmark.template Run<HybridLookup<uint64_t, 30, 1>>();   // 128MB

  // Hash bloom (k=3): three hashes, lower FPR. With 256MB filter and 100M
  // keys, FPR drops to ~5% vs ~37% for k=1 → much better filter rejection
  benchmark.template Run<HybridLookup<uint64_t, 28, 3>>();   // 32MB k=3
  benchmark.template Run<HybridLookup<uint64_t, 30, 3>>();   // 128MB k=3

  // Prefix-occupancy filter — best for OSMC's clustered keys
  benchmark.template Run<HybridLookupPrefix<uint64_t, 24>>();  // 2MB
  benchmark.template Run<HybridLookupPrefix<uint64_t, 28>>();  // 32MB
  benchmark.template Run<HybridLookupPrefix<uint64_t, 30>>();  // 128MB

  // === Insert-heavy variants (mix90) ===
  benchmark.template Run<HybridInsert<uint64_t, 32>>();
  benchmark.template Run<HybridInsert<uint64_t, 64>>();
  benchmark.template Run<HybridInsert<uint64_t, 128>>();

  // === Existing async double-buffered variants (work for both) ===
  benchmark.template Run<HybridPGMLIPP<uint64_t>>({500000});
  benchmark.template Run<HybridPGMLIPP<uint64_t>>({10000000});
}
