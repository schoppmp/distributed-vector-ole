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
#include "absl/types/span.h"
#include "mpc_utils/statusor.h"
#include "openssl/aes.h"

namespace distributed_vector_ole {

class GGMTree {
 public:
  // The size of each seed.
  static const int kBlockSize = AES_BLOCK_SIZE;

  // Constructs a GGM tree from a single seed.
  static mpc_utils::StatusOr<std::unique_ptr<GGMTree>> Create(
      int arity, int64_t num_leaves, absl::Span<const uint8_t> seed);

  // Returns the value at the `node_index`-th node at the given level.
  mpc_utils::StatusOr<absl::Span<const uint8_t>> GetValueAtNode(
      int level_index, int64_t node_index) const;

  // Returns the value at `leaf_index`-th leaf.
  inline mpc_utils::StatusOr<absl::Span<const uint8_t>> GetValueAtLeaf(
      int64_t leaf_index) {
    return GetValueAtNode(num_levels_, leaf_index);
  }

 private:
  GGMTree(std::vector<std::vector<uint8_t>> levels,
          std::vector<std::vector<uint8_t>> keys,
          std::vector<AES_KEY> expanded_keys);

  // Expands the subtree rooted at the node given by level and node index.
  void ExpandSubtree(int start_level, int64_t start_node);

  int arity_;
  int64_t num_leaves_;
  int num_levels_;

  // Expanded seeds on each level.
  std::vector<std::vector<uint8_t>> levels_;

  // Number of keys is equal to `arity_`.
  std::vector<std::vector<uint8_t>> keys_;
  std::vector<AES_KEY> expanded_keys_;
};

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_GGM_TREE_H
