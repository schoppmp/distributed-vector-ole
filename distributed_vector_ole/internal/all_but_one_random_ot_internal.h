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

#ifndef DISTRIBUTED_VECTOR_OLE_INTERNAL_ALL_BUT_ONE_RANDOM_OT_INTERNAL_H
#define DISTRIBUTED_VECTOR_OLE_INTERNAL_ALL_BUT_ONE_RANDOM_OT_INTERNAL_H

#include <type_traits>
#include "NTL/ZZ_p.h"
#include "NTL/lzz_p.h"
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "distributed_vector_ole/ggm_tree.h"
#include "distributed_vector_ole/internal/ntl_helpers.h"
#include "emp-ot/emp-ot.h"

namespace distributed_vector_ole {

namespace all_but_one_random_ot_internal {

// Currently, this method only truncates the leaves of `tree` and writes them to
// `output`.
//
// TODO: Implement packing, where each leaf of the last level actually
//   represents multiple values of type T if sizeof(T) < sizeof(GGMTree::Block).
//   This will require an (n-1)-out-of-n-OT on the last level.
template <typename T, typename std::enable_if<
                          std::numeric_limits<T>::is_integer, int>::type = 0>
void UnpackLastLevel(const GGMTree& tree, absl::Span<T> output) {
#pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < tree.num_leaves(); ++i) {
    // ValieOrDie() is okay here as long as i stays in [0, num_leaves).
    GGMTree::Block leaf = tree.GetValueAtLeaf(i).ValueOrDie();
    output[i] = T(leaf);
  }
}

// Version for NTL modular integers.
template <typename T,
          typename std::enable_if<is_modular_integer<T>::value, int>::type = 0>
inline void UnpackLastLevel(const GGMTree& tree, absl::Span<T> output) {
  // Save NTL context and restore it in each OMP thread.
  NTLContext<T> context;
  context.save();
#pragma omp parallel
  {
    context.restore();
#pragma omp for schedule(static)
    for (int64_t i = 0; i < tree.num_leaves(); ++i) {
      // ValieOrDie() is okay here as long as i stays in [0, num_leaves).
      GGMTree::Block leaf = tree.GetValueAtLeaf(i).ValueOrDie();
      FromUint128(leaf, &output[i]);
    }
  };
}

// Conversion functions between EMP and GGMTree blocks.
inline GGMTree::Block EMPToGGMTreeBlock(emp::block in) {
  return absl::MakeUint128(static_cast<uint64_t>(in[1]),
                           static_cast<uint64_t>(in[0]));
}

inline emp::block GGMTreeToEMPBlock(GGMTree::Block in) {
  emp::block out;
  out[0] = static_cast<int64_t>(absl::Uint128Low64(in));
  out[1] = static_cast<int64_t>(absl::Uint128High64(in));
  return out;
}

}  // namespace all_but_one_random_ot_internal

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_INTERNAL_ALL_BUT_ONE_RANDOM_OT_INTERNAL_H
