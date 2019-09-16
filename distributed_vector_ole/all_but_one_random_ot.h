// (N-1)-out-of-N Random OT
// The sender and the receiver obtain the same random vector of lengh `N`,
// except for the ith position, for which the receiver obtains nothing.

#ifndef DISTRIBUTED_VECTOR_OLE_ALL_BUT_ONE_RANDOM_OT_H_
#define DISTRIBUTED_VECTOR_OLE_ALL_BUT_ONE_RANDOM_OT_H_

#include <vector>
#include "NTL/ZZ_p.h"
#include "distributed_vector_ole/ggm_tree.h"
#include "distributed_vector_ole/internal/all_but_one_random_ot_internal.h"
#include "emp-ot/emp-ot.h"
#include "mpc_utils/benchmarker.hpp"
#include "mpc_utils/canonical_errors.h"
#include "mpc_utils/comm_channel.hpp"
#include "mpc_utils/comm_channel_emp_adapter.hpp"
#include "mpc_utils/status_macros.h"
#include "mpc_utils/statusor.h"
#include "openssl/rand.h"

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

    ASSIGN_OR_RETURN(auto tree, SendTree<T>(output.size(), 2));
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
  // values on the path to an index chosen by the client. If T is a NTL modular
  // integer, ensures that when the leaves are subsequently reduced modulo
  // `T::modulus()`, all residue classes are sampled with equal probability.
  template <typename T>
  mpc_utils::StatusOr<std::unique_ptr<GGMTree>> SendTree(int64_t num_leaves,
                                                         int arity);

  // Checks if the tree should be accepted to ensure equal probability of each
  // leaf value. Called by `SendTree`.
  template <typename T, typename std::enable_if<
                            std::numeric_limits<T>::is_integer, int>::type = 0>
  bool AcceptTree(const GGMTree& tree) {
    return true;
  }
  template <typename T, typename std::enable_if<is_modular_integer<T>::value,
                                                int>::type = 0>
  bool AcceptTree(const GGMTree& tree);

  // Obliviously receives a GGMTree that is equal to the server's except at
  // `index`.
  mpc_utils::StatusOr<std::unique_ptr<GGMTree>> ReceiveTree(int64_t num_leaves,
                                                            int64_t index,
                                                            int arity);

  mpc_utils::comm_channel* channel_;
  std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter_;
  emp::SHOTExtension<mpc_utils::CommChannelEMPAdapter> ot_extension_;
};

template <typename T>
mpc_utils::StatusOr<std::unique_ptr<GGMTree>> AllButOneRandomOT::SendTree(
    int64_t num_leaves, int arity) {
  std::unique_ptr<GGMTree> tree;
  while (!tree) {
    // Create tree from random seed.
    GGMTree::Block seed;
    RAND_bytes(reinterpret_cast<unsigned char*>(&seed), sizeof(seed));
    ASSIGN_OR_RETURN(tree, GGMTree::Create(arity, num_leaves, seed));

    // Rejection sampling.
    if (!AcceptTree<T>(*tree)) {
      tree.reset();
    }
  }

  auto xors = tree->GetSiblingWiseXOR();

  std::vector<emp::block> opt0(tree->num_levels() - 1);
  std::vector<emp::block> opt1(tree->num_levels() - 1);

  // Set opt0 (resp. opt1) to xor of left (resp. right) siblings for each level
  for (auto i = 0; i < tree->num_levels() - 1; ++i) {
    opt0[i] = all_but_one_random_ot_internal::GGMTreeToEMPBlock(xors[i][0]);
    opt1[i] = all_but_one_random_ot_internal::GGMTreeToEMPBlock(xors[i][1]);
  }

  ot_extension_.send(opt0.data(), opt1.data(), tree->num_levels() - 1);
  channel_adapter_->send_data(tree->keys().data(),
                              sizeof(GGMTree::Block) * arity);
  channel_adapter_->flush();

  return tree;
}

template <typename T,
          typename std::enable_if<is_modular_integer<T>::value, int>::type = 0>
bool AllButOneRandomOT::AcceptTree(const GGMTree& tree) {
  // Reject tree if any leaf is too close to 2^128 to ensure values reduced
  // modulo `modulus` have all equal probability.
  absl::uint128 upper_bound = std::numeric_limits<absl::uint128>::max();
  // Use NTL::ZZ explicitly in case T is smaller than 64 bits.
  NTL::ZZ upper_bound_zz(1);
  upper_bound_zz <<= 128;
  upper_bound_zz -= upper_bound_zz % NTL::conv<NTL::ZZ>(T::modulus());
  if (NTL::NumBits(upper_bound_zz) <= 128) {
    uint64_t low = NTL::conv<uint64_t>(upper_bound_zz);
    uint64_t high = NTL::conv<uint64_t>(upper_bound_zz >> 64);
    upper_bound = absl::MakeUint128(high, low);
  }
  bool tree_valid = true;
#pragma omp parallel
  {
#pragma omp for reduction(& : tree_valid) schedule(static)
    for (int64_t i = 0; i < tree.num_leaves(); i++) {
      // ValieOrDie() is okay here as long as i stays in [0, num_leaves).
      absl::uint128 leaf = tree.GetValueAtLeaf(i).ValueOrDie();
      tree_valid &= (leaf < upper_bound);
    }
  }
  return tree_valid;
}

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_ALL_BUT_ONE_RANDOM_OT_H_