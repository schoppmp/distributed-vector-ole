// (N-1)-out-of-N Random OT
// The server and the client obtain the same random vector of lengh `N`,
// except for the ith position, for which the client obtains nothing.

#ifndef DISTRIBUTED_VECTOR_OLE_ALL_BUT_ONE_RANDOM_OT_H_
#define DISTRIBUTED_VECTOR_OLE_ALL_BUT_ONE_RANDOM_OT_H_

#include <vector>
#include "NTL/ZZ_p.h"
#include "absl/types/span.h"
#include "distributed_vector_ole/ggm_tree.h"
#include "emp-ot/emp-ot.h"
#include "mpc_utils/benchmarker.hpp"
#include "mpc_utils/canonical_errors.h"
#include "mpc_utils/comm_channel.hpp"
#include "mpc_utils/comm_channel_emp_adapter.hpp"
#include "mpc_utils/status_macros.h"
#include "mpc_utils/statusor.h"

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
#pragma omp parallel for schedule(guided)
  for (int64_t i = 0; i < tree.num_leaves(); ++i) {
    // ValieOrDie() is okay here as long as i stays in [0, num_leaves).
    GGMTree::Block leaf = tree.GetValueAtLeaf(i).ValueOrDie();
    output[i] = T(leaf);
  }
}

// Template specialization for NTL::ZZ_p.
void UnpackLastLevel(const GGMTree& tree, absl::Span<NTL::ZZ_p> output);

}  // namespace all_but_one_random_ot_internal

class AllButOneRandomOT {
 public:
  // Creates an instance of AllButOneRandomOT that communicates over the given
  // comm_channel.
  static mpc_utils::StatusOr<std::unique_ptr<AllButOneRandomOT>> Create(
      comm_channel* channel);

  // Runs the Server side of the protocol. `output` must point to an array of
  // pre-allocated Ts.
  template <typename T>
  mpc_utils::Status RunServer(absl::Span<T> output) {
    static_assert(sizeof(GGMTree::Block) % sizeof(T) == 0,
                  "sizeof(T) must divide sizeof(GGMTree::Block)");
    if (output.empty()) {
      return mpc_utils::InvalidArgumentError("`output` must not be empty");
    }

    // Pass modulus to ServerSendTree if we're operationg on ZZ_ps.
    const NTL::ZZ* modulus = nullptr;
    if (std::is_same<T, NTL::ZZ_p>::value) {
      modulus = &NTL::ZZ_p::modulus();
    }
    ASSIGN_OR_RETURN(auto tree, ServerSendTree(output.size(), 2, modulus));
    all_but_one_random_ot_internal::UnpackLastLevel(*tree, output);
    return mpc_utils::OkStatus();
  }

  // Runs the Server side of the protocol. `output` must point to an array of
  // pre-allocated Ts, and `ìndex` must be between 0 and output.size() - 1.
  template <typename T>
  mpc_utils::Status RunClient(int64_t index, absl::Span<T> output) {
    static_assert(sizeof(GGMTree::Block) % sizeof(T) == 0,
                  "sizeof(T) must divide sizeof(GGMTree::Block)");
    if (output.empty()) {
      return mpc_utils::InvalidArgumentError("`output` must not be empty");
    }
    if (index < 0 || index >= static_cast<int64_t>(output.size())) {
      return mpc_utils::InvalidArgumentError("`index` out of range");
    }
    ASSIGN_OR_RETURN(auto tree, ClientReceiveTree(output.size(), index, 2));
    all_but_one_random_ot_internal::UnpackLastLevel(*tree, output);
    return mpc_utils::OkStatus();
  }

 private:
  AllButOneRandomOT(
      mpc_utils::comm_channel* channel,
      std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter);

  // Constructs a GGMTree and obliviously sends it to the client, except for the
  // values on the path to an index chosen by the client. If modulus is set,
  // ensures that when the leaves are subsequently reduced modulo `modulus`, all
  // residue classes are sampled with equal probability.
  mpc_utils::StatusOr<std::unique_ptr<GGMTree>> ServerSendTree(
      int64_t num_leaves, int arity, const NTL::ZZ* modulus = nullptr);

  // Obliviously receives a GGMTree that is equal to the server's except at
  // `index`.
  mpc_utils::StatusOr<std::unique_ptr<GGMTree>> ClientReceiveTree(
      int64_t num_leaves, int64_t index, int arity);

  mpc_utils::comm_channel* channel_;
  std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter_;
  emp::SHOTExtension<mpc_utils::CommChannelEMPAdapter> ot_extension_;
};
}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_ALL_BUT_ONE_RANDOM_OT_H_