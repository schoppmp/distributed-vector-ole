// (N-1)-out-of-N Random OT
// The sender and the receiver obtain the same random vector of lengh `N`,
// except for the ith position, for which the receiver obtains nothing.

#ifndef DISTRIBUTED_VECTOR_OLE_ALL_BUT_ONE_RANDOM_OT_H_
#define DISTRIBUTED_VECTOR_OLE_ALL_BUT_ONE_RANDOM_OT_H_

#include <vector>
#include "NTL/ZZ_p.h"
#include "distributed_vector_ole/internal/all_but_one_random_ot_internal.h"
#include "emp-ot/emp-ot.h"
#include "mpc_utils/benchmarker.hpp"
#include "mpc_utils/canonical_errors.h"
#include "mpc_utils/comm_channel.hpp"
#include "mpc_utils/comm_channel_emp_adapter.hpp"
#include "mpc_utils/status_macros.h"
#include "mpc_utils/statusor.h"

namespace distributed_vector_ole {

class AllButOneRandomOT {
 public:
  // Creates an instance of AllButOneRandomOT that communicates over the given
  // comm_channel.
  static mpc_utils::StatusOr<std::unique_ptr<AllButOneRandomOT>> Create(
      mpc_utils::comm_channel* channel);

  // Runs the Server side of the protocol. `output` must point to an array of
  // pre-allocated Ts.
  template <typename T>
  mpc_utils::Status RunSender(absl::Span<T> output) {
    static_assert(sizeof(GGMTree::Block) % sizeof(T) == 0,
                  "sizeof(T) must divide sizeof(GGMTree::Block)");
    if (output.empty()) {
      return mpc_utils::InvalidArgumentError("`output` must not be empty");
    }

    // Pass modulus to SendTree if we're operationg on ZZ_ps.
    const NTL::ZZ* modulus = nullptr;
    if (std::is_same<T, NTL::ZZ_p>::value) {
      modulus = &NTL::ZZ_p::modulus();
    }
    ASSIGN_OR_RETURN(auto tree, SendTree(output.size(), 2, modulus));
    all_but_one_random_ot_internal::UnpackLastLevel(*tree, output);
    return mpc_utils::OkStatus();
  }

  // Runs the Server side of the protocol. `output` must point to an array of
  // pre-allocated Ts, and `ìndex` must be between 0 and output.size() - 1.
  template <typename T>
  mpc_utils::Status RunReceiver(int64_t index, absl::Span<T> output) {
    static_assert(sizeof(GGMTree::Block) % sizeof(T) == 0,
                  "sizeof(T) must divide sizeof(GGMTree::Block)");
    if (output.empty()) {
      return mpc_utils::InvalidArgumentError("`output` must not be empty");
    }
    if (index < 0 || index >= static_cast<int64_t>(output.size())) {
      return mpc_utils::InvalidArgumentError("`index` out of range");
    }
    ASSIGN_OR_RETURN(auto tree, ReceiveTree(output.size(), index, 2));
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
  mpc_utils::StatusOr<std::unique_ptr<GGMTree>> SendTree(
      int64_t num_leaves, int arity, const NTL::ZZ* modulus = nullptr);

  // Obliviously receives a GGMTree that is equal to the server's except at
  // `index`.
  mpc_utils::StatusOr<std::unique_ptr<GGMTree>> ReceiveTree(int64_t num_leaves,
                                                            int64_t index,
                                                            int arity);

  mpc_utils::comm_channel* channel_;
  std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter_;
  emp::SHOTExtension<mpc_utils::CommChannelEMPAdapter> ot_extension_;
};
}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_ALL_BUT_ONE_RANDOM_OT_H_