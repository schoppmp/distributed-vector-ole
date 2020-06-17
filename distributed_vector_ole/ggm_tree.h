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

#ifndef DISTRIBUTED_VECTOR_OLE_GGM_TREE_H_
#define DISTRIBUTED_VECTOR_OLE_GGM_TREE_H_

// Implements a tree that expands a single PRG seed to arbitrary lengths,
// following the GGM construction [1]. Each inner node has b children, where b
// is the tree's arity. The tree is built starting from the root, and each
// node's children are derived from the parent's value using a single PRG call.
//
// More specifically, we use AES with a fixed set of b keys k_0, ..., k_b. We
// compute the value of the i-th child of parent node `p` as follows:
//
//    value[i-th child of p] = AES(k_i, value[p]) ^ value[p].
//
// This construction is also used in FLORAM [2]. The advantage is that the keys
// are public, and can therefore be expanded in advance.
//
// [1] Goldreich, Oded, Shafi Goldwasser, and Silvio Micali. "How to construct
// random functions." Journal of the ACM (JACM) 33.4 (1986): 792-807.
// [2] Doerner, Jack, and Abhi Shelat. "Scaling ORAM for Secure Computation."
// CCS, ACM, 2017, pp. 523–535.

#include <cstdint>
#include <vector>
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "mpc_utils/statusor.h"
#include "openssl/aes.h"
#include "openssl/rand.h"

namespace distributed_vector_ole {

class GGMTree {
 public:
  // The size of each seed.
  static const int kBlockSize = AES_BLOCK_SIZE;
  using Block = absl::uint128;
  static_assert(sizeof(Block) == kBlockSize, "AES block size is not 128");

  // Constructs a GGMTree from a single seed and the given AES keys.
  static mpc_utils::StatusOr<std::unique_ptr<GGMTree>> Create(
      int64_t num_leaves, Block seed, std::vector<Block> keys);

  // Constructs a GGMTree from a single seed.
  static mpc_utils::StatusOr<std::unique_ptr<GGMTree>> Create(
      int arity, int64_t num_leaves, Block seed) {
    std::vector<Block> keys(arity);
    for (int i = 0; i < arity; i++) {
      RAND_bytes(reinterpret_cast<uint8_t *>(&keys[i]), kBlockSize);
    }
    return Create(num_leaves, seed, std::move(keys));
  }

  // Constructs a GGMTree that is missing a single leaf value.
  // For each level, `sibling_wise_xors` contains a vector of blocks with size
  // equal to the arity of the tree. The values of that vector in all entries
  // except the one on the path to `missing_entries` have to be the sibling-wise
  // XOR of the seeds on that level for that sibling index. On the path, the
  // entries are ignored. `keys` contains the public AES keys used for expanding
  // the tree. `keys.size()` must be equal to `arity`.
  static mpc_utils::StatusOr<std::unique_ptr<GGMTree>> CreateFromSiblingWiseXOR(
      int arity, int64_t num_leaves, int64_t missing_index,
      absl::Span<const std::vector<Block>> sibling_wise_xors,
      absl::Span<const Block> keys);

  // Returns the value at the `node_index`-th node at the given level.
  mpc_utils::StatusOr<Block> GetValueAtNode(int level_index,
                                            int64_t node_index) const;

  // Returns the value at `leaf_index`-th leaf.
  inline mpc_utils::StatusOr<Block> GetValueAtLeaf(int64_t leaf_index) const {
    return GetValueAtNode(num_levels_ - 1, leaf_index);
  }

  // For each level except the last one, returns the sibling-wise XOR of all the
  // children of this level. That is, all the first siblings get XORed together,
  // all the second siblings, and so on. The length of the returned vector is
  // num_levels() - 1, and each inner vector has length arity().
  std::vector<std::vector<Block>> GetSiblingWiseXOR() const;

  // Returns the number of children of the tree's inner nodes.
  inline int arity() const { return arity_; }

  // Returns the number of leaves.
  inline int64_t num_leaves() const { return num_leaves_; }

  // Returns the height.
  inline int num_levels() const { return num_levels_; }

  // Returns the size (in blocks) of the level-th level.
  inline int64_t level_size(int level) const { return levels_[level].size(); }

  // Returns the keys used to expand levels. keys().size() equals arity().
  inline absl::Span<const Block> keys() const { return keys_; }

  // Returns the expanded versions of keys(). This is an AES-specific
  // optimization (see also class documentation).
  inline absl::Span<const AES_KEY> expanded_keys() const {
    return expanded_keys_;
  }

 private:
  GGMTree(std::vector<std::vector<Block>> levels, std::vector<Block> keys,
          std::vector<AES_KEY> expanded_keys);

  // Expands the subtree rooted at the node given by level and node index.
  void ExpandSubtree(int start_level, int64_t start_node);

  const int arity_;
  const int64_t num_leaves_;
  const int num_levels_;

  // Expanded seeds on each level. All nodes at a level are stored as if read
  // left-to-right from the fully expanded tree. For node i at level l,
  // the j-th child is at levels_[l+1][i*arity_ + j].
  std::vector<std::vector<Block>> levels_;

  // Number of keys is equal to `arity_`.
  const std::vector<Block> keys_;

  // Expanded AES round keys computed at construction.
  const std::vector<AES_KEY> expanded_keys_;
};

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_GGM_TREE_H_
