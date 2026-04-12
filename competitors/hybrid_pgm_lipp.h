#ifndef TLI_HYBRID_PGM_LIPP_H
#define TLI_HYBRID_PGM_LIPP_H

#include <algorithm>
#include <vector>

#include "../util.h"
#include "base.h"
#include "lipp/src/core/lipp.h"
#include "pgm_index_dynamic.hpp"
#include "searches/branching_binary_search.h"

template <class KeyType>
class HybridPGMLIPP : public Base<KeyType> {
 public:
  HybridPGMLIPP(const std::vector<int>& params) {
    if (!params.empty()) {
      flush_threshold_ = params[0] / 100.0;
    }
  }

  uint64_t Build(const std::vector<KeyValue<KeyType>>& data, size_t num_threads) {
    total_keys_ = data.size();

    std::vector<std::pair<KeyType, uint64_t>> loading_data;
    loading_data.reserve(data.size());
    for (const auto& itm : data) {
      loading_data.push_back(std::make_pair(itm.key, itm.value));
    }

    return util::timing(
        [&] { lipp_.bulk_load(loading_data.data(), loading_data.size()); });
  }

  size_t EqualityLookup(const KeyType& lookup_key, uint32_t thread_id) const {
    // Check DPGM buffer first
    if (!buffer_.empty()) {
      auto it = dpgm_.find(lookup_key);
      if (it != dpgm_.end()) {
        return it->value();
      }
    }

    // Fall back to LIPP
    uint64_t value;
    if (!lipp_.find(lookup_key, value)) {
      return util::NOT_FOUND;
    }
    return value;
  }

  uint64_t RangeQuery(const KeyType& lower_key, const KeyType& upper_key,
                       uint32_t thread_id) const {
    return 0;
  }

  void Insert(const KeyValue<KeyType>& data, uint32_t thread_id) {
    dpgm_.insert(data.key, data.value);
    buffer_.push_back(data);
    total_keys_++;

    // Check if flush threshold is reached
    if (buffer_.size() >= static_cast<size_t>(flush_threshold_ * total_keys_)) {
      flush();
    }
  }

  std::string name() const { return "Hybrid"; }

  std::size_t size() const {
    return lipp_.index_size() + dpgm_.size_in_bytes();
  }

  bool applicable(bool unique, bool range_query, bool insert, bool multithread,
                  const std::string& ops_filename) const {
    return unique && !multithread;
  }

  std::vector<std::string> variants() const {
    return std::vector<std::string>();
  }

 private:
  void flush() const {
    // Naive flush: insert each buffered entry into LIPP
    for (const auto& kv : buffer_) {
      lipp_.insert(kv.key, kv.value);
    }
    // Reset DPGM and buffer
    dpgm_ = DPGMType();
    buffer_.clear();
  }

  using DPGMType = DynamicPGMIndex<KeyType, uint64_t, BranchingBinarySearch<0>,
                                    PGMIndex<KeyType, BranchingBinarySearch<0>, 64, 16>>;

  mutable LIPP<KeyType, uint64_t> lipp_;
  mutable DPGMType dpgm_;
  mutable std::vector<KeyValue<KeyType>> buffer_;
  mutable size_t total_keys_ = 0;
  double flush_threshold_ = 0.05;  // 5% of total keys
};

#endif  // TLI_HYBRID_PGM_LIPP_H
