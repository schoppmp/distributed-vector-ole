#ifndef DISTRIBUTED_VECTOR_OLE_INTERNAL_ALL_BUT_ONE_RANDOM_OT_INTERNAL_H
#define DISTRIBUTED_VECTOR_OLE_INTERNAL_ALL_BUT_ONE_RANDOM_OT_INTERNAL_H

#include "NTL/ZZ_p.h"
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "distributed_vector_ole/ggm_tree.h"

namespace distributed_vector_ole {

namespace all_but_one_random_ot_internal {

// Currently, this method only truncates the leaves of `tree` and writes them to
// `output`.
//
// TODO: Implement packing, where each leaf of the last level actually
//   represents multiple values of type T if sizeof(T) < sizeof(GGMTree::Block).
//   This will require an (n-1)-out-of-n-OT on the last level.
template <typename T>
void UnpackLastLevel(const GGMTree& tree, absl::Span<T> output) {
#pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < tree.num_leaves(); ++i) {
    // ValieOrDie() is okay here as long as i stays in [0, num_leaves).
    GGMTree::Block leaf = tree.GetValueAtLeaf(i).ValueOrDie();
    output[i] = T(leaf);
  }
}

// Template specialization for NTL::ZZ_p.
template <>
inline void UnpackLastLevel<NTL::ZZ_p>(const GGMTree& tree,
                                       absl::Span<NTL::ZZ_p> output) {
  // Save NTL context and restore it in each OMP thread.
  NTL::ZZ_pContext context;
  context.save();
#pragma omp parallel
  {
    context.restore();
    NTL::ZZ leaf_zz;
#pragma omp for schedule(static)
    for (int64_t i = 0; i < tree.num_leaves(); ++i) {
      // ValieOrDie() is okay here as long as i stays in [0, num_leaves).
      GGMTree::Block leaf = tree.GetValueAtLeaf(i).ValueOrDie();
      leaf_zz = 0;
      leaf_zz |= absl::Uint128High64(leaf);
      leaf_zz <<= 64;
      leaf_zz |= absl::Uint128Low64(leaf);
      output[i] = NTL::conv<NTL::ZZ_p>(leaf_zz);
    }
  };
}
}  // namespace all_but_one_random_ot_internal

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_INTERNAL_ALL_BUT_ONE_RANDOM_OT_INTERNAL_H
