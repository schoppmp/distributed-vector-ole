//    Distributed Vector-OLE
//    Copyright (C) 2019 Phillipp Schoppmann and Adria Gascon
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Affero General Public License as
//    published by the Free Software Foundation, either version 3 of the
//    License, or (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Affero General Public License for more details.
//
//    You should have received a copy of the GNU Affero General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "ggm_tree.h"
#include <omp.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include "absl/memory/memory.h"
#include "mpc_utils/canonical_errors.h"
#include "mpc_utils/status.h"
#include "mpc_utils/status_macros.h"
#include "mpc_utils/statusor.h"
#include "openssl/err.h"
#include "openssl/rand.h"

namespace distributed_vector_ole {

namespace {

// Allocates a tree of the given arity with the given number of leaves.
mpc_utils::StatusOr<std::vector<std::vector<GGMTree::Block>>> AllocateLevels(
    int arity, int64_t num_leaves) {
  int num_levels =
      static_cast<int>(1 + std::ceil(std::log(num_leaves) / std::log(arity)));
  int64_t num_blocks_for_level;
  std::vector<std::vector<GGMTree::Block>> levels(num_levels);
  for (int i = num_levels - 1; i >= 0; i--) {
    if (i == num_levels - 1) {
      num_blocks_for_level = num_leaves;
    } else {
      num_blocks_for_level = (num_blocks_for_level + arity - 1) / arity;
    }
    // Initialize all levels to zero.
    levels[i].resize(num_blocks_for_level, 0);
  }
  if (levels[0].size() != 1) {
    return mpc_utils::InternalError("First level should always have one block");
  }
  // Not sure copy elision works with implicit StatusOr constructor. Let's be
  // safe, the cost of the move is minimal.
  return std::move(levels);
}

// XORs `in` in batches of `out.size()` blocks onto `out`.
void XORBlocks(absl::Span<const GGMTree::Block> in,
               absl::Span<GGMTree::Block> out) {
  // We split each block into 64-bit subblocks, as OpenMP only supports
  // certain types for reductions.
  uint64_t* out_data = reinterpret_cast<uint64_t*>(out.data());
  const uint64_t* in_data = reinterpret_cast<const uint64_t*>(in.data());
  const int num_subblocks = sizeof(GGMTree::Block) / sizeof(uint64_t);
  int64_t batch_size = out.size();

  // Use threads for the outer loop over batches.
  int64_t num_batches = (in.size() + batch_size - 1) / batch_size;
#pragma omp parallel for reduction(^:out_data[:batch_size * num_subblocks]) schedule(guided)
  for (int64_t batch = 0; batch < num_batches; batch++) {
    int64_t num_blocks = std::min(
        batch_size, static_cast<int64_t>(in.size()) - batch * batch_size);

    // The blocks in each batch can be XORed 1:1 onto the result.
    for (int block = 0; block < num_blocks; block++) {
      for (int subblock = 0; subblock < num_subblocks; subblock++) {
        int child = batch * batch_size + block;
        out_data[block * num_subblocks + subblock] ^=
            in_data[child * num_subblocks + subblock];
      }
    }
  }
}

// Evaluates the PRG using the given seed and key.
GGMTree::Block ComputePRG(const GGMTree::Block& seed, const AES_KEY& key) {
  GGMTree::Block result;
  AES_encrypt(reinterpret_cast<const uint8_t*>(&seed),
              reinterpret_cast<uint8_t*>(&result), &key);
  result ^= seed;
  return result;
}

}  // namespace

mpc_utils::StatusOr<std::unique_ptr<GGMTree>> GGMTree::Create(
    int arity, int64_t num_leaves, Block seed) {
  if (arity < 2) {
    return mpc_utils::InvalidArgumentError("arity must be at least 2");
  }
  if (num_leaves <= 0) {
    return mpc_utils::InvalidArgumentError("num_leaves must be positive");
  }

  ASSIGN_OR_RETURN(auto levels, AllocateLevels(arity, num_leaves));
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

mpc_utils::StatusOr<std::unique_ptr<GGMTree>> GGMTree::CreateFromSiblingWiseXOR(
    int arity, int64_t num_leaves, int64_t missing_index,
    absl::Span<const std::vector<Block>> sibling_wise_xors,
    absl::Span<const Block> keys) {
  if (arity < 2) {
    return mpc_utils::InvalidArgumentError("arity must be at least 2");
  }
  if (num_leaves <= 0) {
    return mpc_utils::InvalidArgumentError("`num_leaves` must be positive");
  }
  if (missing_index >= num_leaves) {
    return mpc_utils::InvalidArgumentError(
        "`missing_index` must be smaller than `num_leaves`");
  }
  int num_levels = sibling_wise_xors.size() + 1;
  for (int i = 0; i < num_levels - 1; i++) {
    if (static_cast<int>(sibling_wise_xors[i].size()) != arity) {
      return mpc_utils::InvalidArgumentError(
          "All elements of `sibling_wise_xors` must have length `arity`");
    }
  }
  if (std::pow(arity, num_levels - 1) < num_leaves) {
    return mpc_utils::InvalidArgumentError(
        "Dimensions passed in `sibling_wise_xors` to small for `num_leaves`");
  }
  if (keys.empty()) {
    return mpc_utils::InvalidArgumentError("`keys` must not be empty");
  }

  // Construct path to `missing_index`.
  std::vector<int64_t> missing_path(num_levels);
  missing_path[num_levels - 1] = missing_index;
  for (int i = num_levels - 2; i >= 0; i--) {
    missing_path[i] = missing_path[i + 1] / arity;
  }

  // Expand keys and allocate tree.
  std::vector<Block> keys_copy(keys.begin(), keys.end());
  std::vector<AES_KEY> expanded_keys(arity);
  for (int i = 0; i < arity; i++) {
    if (0 != AES_set_encrypt_key(reinterpret_cast<uint8_t*>(&keys_copy[i]),
                                 8 * kBlockSize, &expanded_keys[i])) {
      return mpc_utils::InternalError(ERR_reason_error_string(ERR_get_error()));
    }
  }
  ASSIGN_OR_RETURN(auto levels, AllocateLevels(arity, num_leaves));
  std::unique_ptr<GGMTree> tree = absl::WrapUnique(new GGMTree(
      std::move(levels), std::move(keys_copy), std::move(expanded_keys)));
  assert(tree->num_levels() == num_levels);

  // Expand tree from the root. At each level, use sibling_wise_xors together
  // with already computed subtrees to compute all missing seeds but one, and
  // expand the subtrees under those.
  for (int level_index = 1; level_index < num_levels; level_index++) {
    // Compute XOR of all nodes at the current level
    std::vector<Block> current_level_xors(arity, 0);
    XORBlocks(tree->levels_[level_index], absl::MakeSpan(current_level_xors));
    int64_t node_base = missing_path[level_index - 1] * arity;
    int num_siblings = std::min(static_cast<int64_t>(arity),
                                tree->level_size(level_index) - node_base);
    for (int sibling_index = 0; sibling_index < num_siblings; sibling_index++) {
      if (node_base + sibling_index == missing_path[level_index]) {
        continue;
      }
      tree->levels_[level_index][node_base + sibling_index] =
          current_level_xors[sibling_index] ^
          sibling_wise_xors[level_index - 1][sibling_index];
      tree->ExpandSubtree(level_index, node_base + sibling_index);
    }
  }
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
  int64_t max_node_index = start_node + 1;
  for (int level_index = start_level; level_index < num_levels_ - 1;
       level_index++) {
    // Account for the fact that the level might not be full.
    max_node_index = std::min(max_node_index, level_size(level_index));

#pragma omp parallel for schedule(guided)
    for (int64_t node_index = start_node; node_index < max_node_index;
         node_index++) {
      int64_t num_siblings =
          std::min(static_cast<int64_t>(arity_),
                   level_size(level_index + 1) - node_index * arity_);
      for (int sibling_index = 0; sibling_index < num_siblings;
           sibling_index++) {
        levels_[level_index + 1][arity_ * node_index + sibling_index] =
            ComputePRG(levels_[level_index][node_index],
                       expanded_keys_[sibling_index]);
      }
    }
    start_node *= arity_;
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

std::vector<std::vector<GGMTree::Block>> GGMTree::GetSiblingWiseXOR() const {
  std::vector<std::vector<Block>> values(num_levels_ - 1,
                                         std::vector<Block>(arity_, 0));

  for (int level_index = 0; level_index < num_levels_ - 1; level_index++) {
    XORBlocks(levels_[level_index + 1], absl::MakeSpan(values[level_index]));
  }
  return values;
}

}  // namespace distributed_vector_ole
