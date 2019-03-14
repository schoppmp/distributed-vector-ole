#include "ggm_tree.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include "absl/memory/memory.h"
#include "boost/asio.hpp"
#include "boost/thread.hpp"
#include "openssl/err.h"
#include "openssl/rand.h"

namespace distributed_vector_ole {

namespace {

void EncryptBlockWise(absl::Span<const uint8_t> in, int num_blocks,
                      const AES_KEY* key, absl::Span<uint8_t> out) {
  for (int i = 0; i < num_blocks; i++) {
    int64_t block_start = i * GGMTree::kBlockSize;
    AES_encrypt(&in[block_start], &out[block_start], key);
  }
}

}  // namespace

mpc_utils::StatusOr<std::unique_ptr<GGMTree>> GGMTree::Create(
    int arity, int64_t num_leaves, absl::Span<const uint8_t> seed) {
  if (arity < 2) {
    return mpc_utils::InvalidArgumentError("arity must be at least 2");
  }
  if (num_leaves < 0) {
    return mpc_utils::InvalidArgumentError("num_leaves must not be negative");
  }
  if (seed.size() != kBlockSize) {
    return mpc_utils::InvalidArgumentError("seed must have length kBlockSize");
  }

  // Allocate memory for tree levels and copy seed to root.
  int num_levels =
      static_cast<int>(1 + std::ceil(std::log(num_leaves) / std::log(arity)));
  int64_t num_blocks_for_level;
  std::vector<std::vector<uint8_t>> levels(num_levels);
  for (int i = num_levels - 1; i >= 0; i--) {
    if (i == num_levels - 1) {
      num_blocks_for_level = num_leaves;
    } else {
      num_blocks_for_level = (num_blocks_for_level + arity - 1) / arity;
    }
    levels[i].resize(kBlockSize * num_blocks_for_level);
  }
  std::copy_n(seed.begin(), std::min(seed.size(), levels[0].size()),
              levels[0].begin());

  // Generate keys.
  std::vector<std::vector<uint8_t>> keys(arity,
                                         std::vector<uint8_t>(kBlockSize));
  std::vector<AES_KEY> expanded_keys(arity);
  for (int i = 0; i < arity; i++) {
    RAND_bytes(keys[i].data(), keys[i].size());
    if (0 != AES_set_encrypt_key(keys[i].data(), 8 * kBlockSize,
                                 &expanded_keys[i])) {
      return mpc_utils::InternalError(ERR_reason_error_string(ERR_get_error()));
    }
  }

  std::unique_ptr<GGMTree> tree = absl::WrapUnique(new GGMTree(
      std::move(levels), std::move(keys), std::move(expanded_keys)));
  tree->ExpandSubtree(0, 0);
  return tree;
}

GGMTree::GGMTree(std::vector<std::vector<uint8_t>> levels,
                 std::vector<std::vector<uint8_t>> keys,
                 std::vector<AES_KEY> expanded_keys)
    : arity_(keys.size()),
      num_leaves_(levels.back().size()),
      num_levels_(levels.size()),
      levels_(std::move(levels)),
      keys_(std::move(keys)),
      expanded_keys_(std::move(expanded_keys)) {}

void GGMTree::ExpandSubtree(int start_level, int64_t start_node) {
  // The byte we start at in each level.
  int start_index = start_node * kBlockSize;
  int level_size = 1;
  int num_threads = boost::thread::hardware_concurrency() * 2;

  // We process levels sequentially, but parallelize each level.
  for (int level_index = start_level;
       level_index < num_levels_ - 1 &&
       start_index < static_cast<int64_t>(levels_[level_index].size());
       level_index++) {
    boost::asio::thread_pool pool(num_threads);

    // Compute size of the next level of this subtree, accounting for the fact
    // that the tree might not be full.
    int64_t next_level_size = arity_ * level_size;
    if (arity_ * start_index + next_level_size >
        static_cast<int64_t>(levels_[level_index + 1].size())) {
      next_level_size = levels_[level_index + 1].size() - arity_ * start_index;
    }

    // Adjust number of tasks per key if the arity is not enough to fully
    // parallelize.
    int tasks_per_key = 1;
    if (arity_ < num_threads) {
      tasks_per_key = (num_threads + arity_ - 1) / arity_;
    }

    // Iterate over keys, encrypting the seeds at the current level with each
    // key.
    std::vector<std::vector<uint8_t>> task_results(arity_ * tasks_per_key);
    for (int key_index = 0; key_index < arity_; key_index++) {
      // With each key, we only need to compute every arity_-th seed.
      int64_t num_blocks_for_key = next_level_size / kBlockSize / arity_;
      if (key_index < (next_level_size / kBlockSize) % arity_) {
        num_blocks_for_key++;
      }

      // Split work between tasks.
      int64_t task_start_index = start_index;
      for (int task = 0; task < tasks_per_key; task++) {
        int task_index = key_index * tasks_per_key + task;
        int64_t num_blocks_for_task = num_blocks_for_key / tasks_per_key;
        if (task < num_blocks_for_key % tasks_per_key) {
          num_blocks_for_task++;
        }
        int64_t task_size = num_blocks_for_task * kBlockSize;
        assert(task_start_index + task_size <=
               static_cast<int64_t>(levels_[level_index].size()));
        task_results[task_index].resize(task_size);

        // Use Spans to pass current seeds and write results.
        absl::Span<uint8_t> encryption_results =
            absl::MakeSpan(task_results[task_index]);
        absl::Span<const uint8_t> encryption_inputs = absl::MakeConstSpan(
            &levels_[level_index][task_start_index], task_size);

        // Spawn task!
        boost::asio::post(
            pool,
            std::bind(EncryptBlockWise, encryption_inputs, num_blocks_for_task,
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
