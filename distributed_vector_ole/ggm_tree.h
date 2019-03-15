// Implements a tree that expands a single PRG seed to arbitrary lengths,
// following the GGM construction [1]. Each inner node has k children, where b
// is the tree's arity. To compute the value of an inner node v, one calls the
// PRG with the parent's seed and key k_i, 0 <= i < b, if v is the i-th child of
// its parent.
//
// [1] Goldreich, Oded, Shafi Goldwasser, and Silvio Micali. "How to construct
// random functions." Journal of the ACM (JACM) 33.4 (1986): 792-807.

#ifndef DISTRIBUTED_VECTOR_OLE_GGM_TREE_H
#define DISTRIBUTED_VECTOR_OLE_GGM_TREE_H

#include <cstdint>
#include <vector>
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "mpc_utils/statusor.h"
#include "openssl/aes.h"

namespace distributed_vector_ole {

class GGMTree {
 public:
  // The size of each seed.
  static const int kBlockSize = AES_BLOCK_SIZE;
  using Block = absl::uint128;
  static_assert(sizeof(Block) == kBlockSize, "AES block size is not 128");

  // Constructs a GGM tree from a single seed.
  static mpc_utils::StatusOr<std::unique_ptr<GGMTree>> Create(
      int arity, int64_t num_leaves, Block seed);

  // Returns the value at the `node_index`-th node at the given level.
  mpc_utils::StatusOr<Block> GetValueAtNode(int level_index,
                                            int64_t node_index) const;

  // Returns the value at `leaf_index`-th leaf.
  inline mpc_utils::StatusOr<Block> GetValueAtLeaf(int64_t leaf_index) const {
    return GetValueAtNode(num_levels_ - 1, leaf_index);
  }

  // Returns the number of children of the tree's inner nodes.
  inline int arity() { return arity_; }

  // Returns the number of leaves.
  inline int64_t num_leaves() { return num_leaves_; }

  // Returns the height.
  inline int num_levels() { return num_levels_; }

  // Returns the keys used to expand levels. keys().size() equals arity().
  inline absl::Span<const Block> keys() { return keys_; }

  // Returns the expanded versions of keys().
  inline absl::Span<const AES_KEY> expanded_keys() { return expanded_keys_; }

 private:
  GGMTree(std::vector<std::vector<Block>> levels, std::vector<Block> keys,
          std::vector<AES_KEY> expanded_keys);

  // Expands the subtree rooted at the node given by level and node index.
  void ExpandSubtree(int start_level, int64_t start_node);

  const int arity_;
  const int64_t num_leaves_;
  const int num_levels_;

  // Expanded seeds on each level.
  std::vector<std::vector<Block>> levels_;

  // Number of keys is equal to `arity_`.
  const std::vector<Block> keys_;
  const std::vector<AES_KEY> expanded_keys_;
};

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_GGM_TREE_H
