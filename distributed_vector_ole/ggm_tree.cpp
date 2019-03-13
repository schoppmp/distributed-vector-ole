#include "ggm_tree.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include "boost/asio.hpp"
#include "boost/thread.hpp"
#include "openssl/rand.h"

namespace distributed_vector_ole {

namespace {
void EncryptBlockWise(absl::Span<const uint8_t> src, const AES_KEY* key,
                      absl::Span<uint8_t> result) {}
}  // namespace

GGMTree::GGMTree(int arity, int64_t num_leaves, absl::Span<const uint8_t> seed)
    : arity_(arity),
      num_leaves_(num_leaves),
      num_levels_(static_cast<int>(
          std::ceil(std::log(num_leaves_) / std::log(arity_)))),
      levels_(num_levels_),
      keys_(arity_, std::vector<uint8_t>(kBlockSize)),
      expanded_keys_(arity_) {
  // Generate keys.
  for (int i = 1; i < arity_; i++) {
    RAND_bytes(keys_[i].data(), keys_[i].size());
    AES_set_encrypt_key(keys_[i].data(), 8 * kBlockSize, &expanded_keys_[i]);
  }

  // Allocate memory for levels.
  int64_t level_size;
  for (int i = num_levels_ - 1; i >= 0; i--) {
    if (i == num_levels_ - 1) {
      level_size = num_leaves_;
    } else {
      level_size = (level_size + arity_ - 1) / arity_;
    }
    levels_[i] = std::vector<uint8_t>(kBlockSize * level_size);
  }

  // Copy seed and expand.
  std::copy_n(seed.begin(), std::min(seed.size(), levels_[0].size()),
              levels_[0].begin());
  ExpandSubtree(0, 0);
}

void GGMTree::ExpandSubtree(int start_level, int64_t start_node) {
  // The byte we start at in each level.
  int start_index = start_node * kBlockSize;
  int level_size = 1;
  int num_threads = boost::thread::hardware_concurrency() * 2;

  // We process levels sequentially, spawning a number of tasks equal to the
  // arity.
  for (int level_index = start_level;
       level_index < num_levels_ - 1 && start_index < static_cast<int64_t>(levels_[level_index].size());
       level_index++) {
    boost::asio::thread_pool pool(num_threads);

    // Compute size of the next level of this subtree, accounting for the fact
    // that the tree might not be full.
    int next_level_size = arity_ * level_size;
    if (arity_ * start_index + next_level_size >
        static_cast<int64_t>(levels_[level_index + 1].size())) {
      next_level_size = levels_[level_index + 1].size() - arity_ * start_index;
    }

    // next_levels stores the result of each task, to be copied to the next
    // level.
    int tasks_per_key = 1;
    if (arity_ < num_threads) {
      tasks_per_key = (num_threads + arity_ - 1) / arity_;
    }
    std::vector<std::vector<uint8_t>> task_results(arity_ * tasks_per_key);
    for (int key_index = 0; key_index < arity_; key_index++) {
      // With each key, we only need to compute every arity_-th seed.
      int next_level_size_for_key = next_level_size / arity_;
      if (key_index < next_level_size % arity_) {
        next_level_size_for_key++;
      }

      // Split work between tasks.
      int task_start_index = start_index;
      for (int task = 0; task < tasks_per_key; task++) {
        int task_size = next_level_size_for_key / tasks_per_key;
        if (task < next_level_size_for_key % tasks_per_key) {
          task_size++;
        }
        assert(task_start_index + task_size <= static_cast<int64_t>(levels_[level_index].size()));
        task_results[key_index * tasks_per_key + task].resize(task_size);

        // Use Spans to pass current seeds and write results.
        absl::Span<uint8_t> encryption_results =
            absl::MakeSpan(task_results[key_index * tasks_per_key + task]);
        absl::Span<const uint8_t> encryption_inputs = absl::MakeConstSpan(
            &levels_[level_index][task_start_index], task_size);

        // Spawn task!
        boost::asio::post(
            pool, std::bind(EncryptBlockWise, encryption_inputs,
                              &expanded_keys_[key_index], encryption_results));
        task_start_index += task_size;
      }
    }
    pool.join();
    level_size = next_level_size;
    start_index *= arity_;
  }
}

}  // namespace distributed_vector_ole
