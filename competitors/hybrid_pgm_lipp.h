#ifndef TLI_HYBRID_PGM_LIPP_H
#define TLI_HYBRID_PGM_LIPP_H

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "../util.h"
#include "base.h"
#include "lipp/src/core/lipp.h"
#include "pgm_index_dynamic.hpp"
#include "searches/branching_binary_search.h"

// Simple Bloom filter for fast negative lookups on the DPGM buffer.
// Not a data store — only a query filter (allowed per teaching staff).
// Block Bloom filter: touches exactly ONE cache line per query.
// Design: array of 64-byte blocks. For each key: one hash selects a block,
// then 4 bits within the 512-bit block are set/tested.
// Not a data store — only a filter (allowed per teaching staff).
template <class KeyType>
class BloomFilter {
 public:
  static constexpr size_t WORDS_PER_BLOCK = 8;  // 64 bytes = 1 cache line

  BloomFilter(size_t num_bits = 64 * 1024 * 1024)
      : num_blocks_((num_bits + 511) / 512),
        data_(num_blocks_ * WORDS_PER_BLOCK, 0) {}

  void insert(const KeyType& key) {
    uint64_t h = mix64(static_cast<uint64_t>(key));
    uint64_t* block = &data_[(h >> 32) % num_blocks_ * WORDS_PER_BLOCK];
    // 4 bit positions from low 32 bits, each 9 bits wide (0-511)
    block[((h >> 0)  & 511) >> 6] |= 1ULL << ((h >> 0)  & 63);
    block[((h >> 9)  & 511) >> 6] |= 1ULL << ((h >> 9)  & 63);
    block[((h >> 18) & 511) >> 6] |= 1ULL << ((h >> 18) & 63);
    block[((h >> 27) & 511) >> 6] |= 1ULL << ((h >> 27) & 63);
  }

  bool maybe_contains(const KeyType& key) const {
    uint64_t h = mix64(static_cast<uint64_t>(key));
    const uint64_t* block = &data_[(h >> 32) % num_blocks_ * WORDS_PER_BLOCK];
    return (block[((h >> 0)  & 511) >> 6] & (1ULL << ((h >> 0)  & 63))) &&
           (block[((h >> 9)  & 511) >> 6] & (1ULL << ((h >> 9)  & 63))) &&
           (block[((h >> 18) & 511) >> 6] & (1ULL << ((h >> 18) & 63))) &&
           (block[((h >> 27) & 511) >> 6] & (1ULL << ((h >> 27) & 63)));
  }

  void clear() {
    std::fill(data_.begin(), data_.end(), 0);
  }

 private:
  static uint64_t mix64(uint64_t k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
  }

  size_t num_blocks_;
  std::vector<uint64_t> data_;
};

template <class KeyType>
class HybridPGMLIPP : public Base<KeyType> {
 public:
  HybridPGMLIPP(const std::vector<int>& params)
      : bloom_(compute_bloom_bits(params.size() > 0 && params[0] > 0
                                      ? static_cast<size_t>(params[0])
                                      : 50000)) {
    if (params.size() > 0 && params[0] > 0) {
      flush_threshold_ = static_cast<size_t>(params[0]);
    }
  }

  // Size bloom filter based on expected keys.
  // Target: ~32 bits per key for low FPR; min 8MB for robustness,
  // but for small thresholds, use an L2-resident size (512KB - 1MB).
  static size_t compute_bloom_bits(size_t threshold) {
    if (threshold <= 500000) {
      // Lookup-heavy case: small bloom fits in L2, minimal overhead per query
      return 4 * 1024 * 1024;  // 512KB (4M bits)
    }
    return 64 * 1024 * 1024;  // 8MB (64M bits) for insert-heavy
  }

  ~HybridPGMLIPP() {
    if (flush_thread_.joinable()) {
      flush_thread_.join();
    }
  }

  uint64_t Build(const std::vector<KeyValue<KeyType>>& data, size_t num_threads) {
    total_keys_ = data.size();
    active_count_ = 0;
    flush_in_progress_.store(false, std::memory_order_relaxed);
    bloom_.clear();
    flush_buffer_.clear();

    std::vector<std::pair<KeyType, uint64_t>> loading_data;
    loading_data.reserve(data.size());
    for (const auto& itm : data) {
      loading_data.push_back(std::make_pair(itm.key, itm.value));
    }

    return util::timing(
        [&] { lipp_.bulk_load(loading_data.data(), loading_data.size()); });
  }

  __attribute__((always_inline))
  size_t EqualityLookup(const KeyType& lookup_key, uint32_t thread_id) const {
    // Fast path: Bloom filter usually returns false → skip DPGM entirely
    if (__builtin_expect(bloom_.maybe_contains(lookup_key), 0)) {
      if (active_count_ > 0) {
        auto it = dpgm_active_.find(lookup_key);
        if (it != dpgm_active_.end()) return it->value();
      }
      // Only check flushing buffer if a flush might be active
      if (__builtin_expect(flush_in_progress_.load(std::memory_order_acquire), 0)) {
        auto it = dpgm_flushing_.find(lookup_key);
        if (it != dpgm_flushing_.end()) return it->value();
      }
    }

    // Check LIPP — lock only if flush thread is writing to it (rare)
    uint64_t value;
    if (__builtin_expect(flush_in_progress_.load(std::memory_order_acquire), 0)) {
      std::lock_guard<std::mutex> lock(lipp_mutex_);
      if (!lipp_.find(lookup_key, value)) return util::NOT_FOUND;
      return value;
    }
    if (!lipp_.find(lookup_key, value)) return util::NOT_FOUND;
    return value;
  }

  uint64_t RangeQuery(const KeyType& lower_key, const KeyType& upper_key,
                       uint32_t thread_id) const {
    return 0;
  }

  void Insert(const KeyValue<KeyType>& data, uint32_t thread_id) {
    dpgm_active_.insert(data.key, data.value);
    bloom_.insert(data.key);
    active_log_.push_back({data.key, data.value});
    active_count_++;
    total_keys_++;

    // Check if flush threshold is reached
    if (active_count_ >= flush_threshold_) {
      // If a previous flush is still running, wait for it
      if (flush_thread_.joinable()) {
        flush_thread_.join();
      }

      // Clear the old flushing DPGM (already flushed into LIPP last round)
      dpgm_flushing_ = DPGMType();

      // Swap DPGMs: dpgm_active_ moves to dpgm_flushing_ for background flush
      std::swap(dpgm_active_, dpgm_flushing_);
      // Move active_log_ to flush_buffer_ for background thread
      flush_buffer_.swap(active_log_);
      active_log_.clear();
      active_log_.reserve(flush_threshold_);
      active_count_ = 0;

      // Launch background flush thread
      flush_in_progress_.store(true, std::memory_order_release);
      flush_thread_ = std::thread(&HybridPGMLIPP::flush_background, this);
    }
  }

  std::string name() const { return "Hybrid"; }

  std::size_t size() const {
    return lipp_.index_size() + dpgm_active_.size_in_bytes() +
           dpgm_flushing_.size_in_bytes();
  }

  bool applicable(bool unique, bool range_query, bool insert, bool multithread,
                  const std::string& ops_filename) const {
    return unique && !multithread;
  }

  std::vector<std::string> variants() const {
    std::vector<std::string> vec;
    vec.push_back(std::to_string(flush_threshold_));
    return vec;
  }

 private:
  void flush_background() const {
    // Insert flush_buffer_ entries into LIPP in batches
    constexpr size_t BATCH_SIZE = 128;
    size_t idx = 0;
    while (idx < flush_buffer_.size()) {
      std::lock_guard<std::mutex> lock(lipp_mutex_);
      size_t end = std::min(idx + BATCH_SIZE, flush_buffer_.size());
      for (; idx < end; ++idx) {
        lipp_.insert(flush_buffer_[idx].first, flush_buffer_[idx].second);
      }
    }

    // Signal flush complete BEFORE touching dpgm_flushing_.
    // This ensures the main thread's lookup path won't race with us.
    flush_in_progress_.store(false, std::memory_order_release);
    // Note: dpgm_flushing_ remains populated until next swap (which is fine —
    // the main thread uses active_count_/flush_in_progress_ flags as gates).
  }

  using DPGMType = DynamicPGMIndex<KeyType, uint64_t, BranchingBinarySearch<0>,
                                    PGMIndex<KeyType, BranchingBinarySearch<0>, 64, 16>>;

  mutable LIPP<KeyType, uint64_t> lipp_;
  mutable DPGMType dpgm_active_;
  mutable DPGMType dpgm_flushing_;
  mutable BloomFilter<KeyType> bloom_;
  mutable std::atomic<bool> flush_in_progress_{false};
  mutable std::thread flush_thread_;
  mutable std::mutex lipp_mutex_;
  mutable std::vector<std::pair<KeyType, uint64_t>> flush_buffer_;
  mutable std::vector<std::pair<KeyType, uint64_t>> active_log_;
  mutable size_t active_count_ = 0;
  mutable size_t total_keys_ = 0;
  size_t flush_threshold_ = 50000;
};

#endif  // TLI_HYBRID_PGM_LIPP_H
