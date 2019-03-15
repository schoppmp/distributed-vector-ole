#include "ggm_tree.h"
#include <omp.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include "absl/memory/memory.h"
#include "openssl/err.h"
#include "openssl/rand.h"

namespace distributed_vector_ole {

mpc_utils::StatusOr<std::unique_ptr<GGMTree>> GGMTree::Create(
    int arity, int64_t num_leaves, Block seed) {
  if (arity < 2) {
    return mpc_utils::InvalidArgumentError("arity must be at least 2");
  }
  if (num_leaves <= 0) {
    return mpc_utils::InvalidArgumentError("num_leaves must be positive");
  }

  // Allocate memory for tree levels and copy seed to root.
  int num_levels =
      static_cast<int>(1 + std::ceil(std::log(num_leaves) / std::log(arity)));
  int64_t num_blocks_for_level;
  std::vector<std::vector<Block>> levels(num_levels);
  for (int i = num_levels - 1; i >= 0; i--) {
    if (i == num_levels - 1) {
      num_blocks_for_level = num_leaves;
    } else {
      num_blocks_for_level = (num_blocks_for_level + arity - 1) / arity;
    }
    levels[i].resize(num_blocks_for_level);
  }
  if (levels[0].size() != 1) {
    return mpc_utils::InternalError("First level should always have one block");
  }
  levels[0][0] = seed;

  // Generate keys.
  std::vector<Block> keys(arity);
  std::vector<AES_KEY> expanded_keys(arity);
  for (int i = 0; i < arity; i++) {
    RAND_bytes(reinterpret_cast<uint8_t*>(&keys[i]), kBlockSize);
    if (0 != AES_set_encrypt_key(reinterpret_cast<uint8_t*>(&keys[i]),
                                 8 * kBlockSize, &expanded_keys[i])) {
      return mpc_utils::InternalError(ERR_reason_error_string(ERR_get_error()));
    }
  }

  std::unique_ptr<GGMTree> tree = absl::WrapUnique(new GGMTree(
      std::move(levels), std::move(keys), std::move(expanded_keys)));
  tree->ExpandSubtree(0, 0);
  return tree;
}

GGMTree::GGMTree(std::vector<std::vector<Block>> levels,
                 std::vector<Block> keys, std::vector<AES_KEY> expanded_keys)
    : arity_(keys.size()),
      num_leaves_(levels.back().size()),
      num_levels_(levels.size()),
      levels_(std::move(levels)),
      keys_(std::move(keys)),
      expanded_keys_(std::move(expanded_keys)) {}

void GGMTree::ExpandSubtree(int start_level, int64_t start_node) {
  // Iterate over levels, then nodes, then keys.
  int64_t max_node_index = 1;
  for (int level_index = start_level; level_index < num_levels_ - 1;
       level_index++) {
    // Account for the fact that the level might not be full.
    max_node_index = std::min(
        max_node_index, static_cast<int64_t>(levels_[level_index].size()));

#pragma omp parallel num_threads(omp_get_num_procs())
#pragma omp for schedule(guided)
    for (int64_t node_index = 0; node_index < max_node_index; node_index++) {
      for (int key_index = 0;
           key_index < arity() &&
           arity_ * node_index + key_index <
               static_cast<int64_t>(levels_[level_index + 1].size());
           key_index++) {
        AES_encrypt(
            reinterpret_cast<uint8_t*>(&levels_[level_index][node_index]),
            reinterpret_cast<uint8_t*>(
                &levels_[level_index + 1][arity_ * node_index + key_index]),
            &expanded_keys_[key_index]);
      }
    }
    max_node_index *= arity_;
  }
}

mpc_utils::StatusOr<GGMTree::Block> GGMTree::GetValueAtNode(
    int level_index, int64_t node_index) const {
  if (level_index < 0 || level_index >= num_levels_) {
    return mpc_utils::InvalidArgumentError("level_index out of range");
  }
  if (node_index < 0 ||
      node_index >= static_cast<int64_t>(levels_[level_index].size())) {
    return mpc_utils::InvalidArgumentError("node_index out of range");
  }
  return levels_[level_index][node_index];
}

}  // namespace distributed_vector_ole
