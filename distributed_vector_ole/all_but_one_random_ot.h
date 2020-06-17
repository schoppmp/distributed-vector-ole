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

#ifndef DISTRIBUTED_VECTOR_OLE_ALL_BUT_ONE_RANDOM_OT_H_
#define DISTRIBUTED_VECTOR_OLE_ALL_BUT_ONE_RANDOM_OT_H_

// (N-1)-out-of-N Random OT
// The sender and the receiver obtain the same random vector of lengh `N`,
// except for the ith position, for which the receiver obtains nothing.

#include <vector>
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
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
      mpc_utils::comm_channel *channel, double statistical_security = 40);

  // Runs the Server side of the protocol. `output` must point to an array of
  // pre-allocated Ts.
  template <typename T>
  mpc_utils::Status RunSender(absl::Span<T> output) {
    return RunSenderBatched(absl::MakeSpan(&output, 1));
  }

  // Runs the Server side of the protocol. `output` must point to an array of
  // pre-allocated Ts, and `index` must be between 0 and output.size() - 1.
  // If `output` is empty, `index` is ignored.
  template <typename T>
  mpc_utils::Status RunReceiver(int64_t index, absl::Span<T> output) {
    if (index < 0 ||
        (index >= static_cast<int64_t>(output.size()) && output.size() != 0)) {
      return mpc_utils::InvalidArgumentError("`index` out of range");
    }
    return RunReceiverBatched(absl::MakeConstSpan(&index, 1),
                              absl::MakeSpan(&output, 1));
  }

  template <typename T>
  mpc_utils::Status RunSenderBatched(absl::Span<absl::Span<T>> outputs) {
    std::vector<int64_t> sizes(outputs.size());
    for (int i = 0; i < static_cast<int>(outputs.size()); i++) {
      sizes[i] = outputs[i].size();
    }
    ASSIGN_OR_RETURN(auto trees, SendTrees<T>(sizes, 2));
    NTLContext<T> context;
    context.save();
#pragma omp parallel
    {
      context.restore();
#pragma omp for schedule(dynamic)
      for (int i = 0; i < static_cast<int>(outputs.size()); i++) {
        if (outputs[i].size() == 0) {
          continue;  // Do nothing if output array is empty.
        }
        all_but_one_random_ot_internal::UnpackLastLevel(*trees[i], outputs[i]);
      }
    }
    return mpc_utils::OkStatus();
  }

  template <typename T>
  mpc_utils::Status RunReceiverBatched(absl::Span<const int64_t> indices,
                                       absl::Span<absl::Span<T>> outputs) {
    if (outputs.size() != indices.size()) {
      return mpc_utils::InvalidArgumentError(
          "`indices` and `outputs` must have the same size");
    }
    for (int i = 0; i < static_cast<int>(outputs.size()); i++) {
      if (indices[i] < 0 ||
          (indices[i] >= static_cast<int64_t>(outputs[i].size()) &&
           outputs[i].size() != 0)) {
        return mpc_utils::InvalidArgumentError(
            absl::StrCat("`indices[", i, "]` out of range"));
      }
    }

    std::vector<int64_t> sizes(outputs.size());
    for (int i = 0; i < static_cast<int>(outputs.size()); i++) {
      sizes[i] = outputs[i].size();
    }
    ASSIGN_OR_RETURN(auto trees, ReceiveTrees(sizes, indices, 2));
    NTLContext<T> context;
    context.save();
#pragma omp parallel
    {
      context.restore();
#pragma omp for schedule(dynamic)
      for (int i = 0; i < static_cast<int>(outputs.size()); i++) {
        if (outputs[i].size() == 0) {
          continue;  // Do nothing if output array is empty.
        }
        all_but_one_random_ot_internal::UnpackLastLevel(*trees[i], outputs[i]);
      }
    }
    return mpc_utils::OkStatus();
  }

 private:
  AllButOneRandomOT(
      mpc_utils::comm_channel *channel,
      std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter,
      double statistical_security);

  // For each element of `num_leaves`, constructs a GGMTree and obliviously
  // sends it to the client, except for the values on the path to an index
  // chosen by the client. If T is a NTL modular integer, ensures that when the
  // leaves are subsequently reduced modulo `T::modulus()`, all residue classes
  // are sampled with equal probability.
  template <typename T>
  mpc_utils::StatusOr<std::vector<std::unique_ptr<GGMTree>>> SendTrees(
      absl::Span<const int64_t> num_leaves, int arity);

  // Obliviously receives GGMTrees, where the i-th tree is equal to the server's
  // except at `indices[i]`.
  mpc_utils::StatusOr<std::vector<std::unique_ptr<GGMTree>>> ReceiveTrees(
      absl::Span<const int64_t> num_leaves, absl::Span<const int64_t> indices,
      int arity);

  mpc_utils::comm_channel *channel_;
  std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter_;
  emp::SHOTExtension<mpc_utils::CommChannelEMPAdapter> ot_extension_;
  double statistical_security_;
};

template <typename T>
mpc_utils::StatusOr<std::vector<std::unique_ptr<GGMTree>>>
AllButOneRandomOT::SendTrees(absl::Span<const int64_t> num_leaves, int arity) {
  int num_trees = static_cast<int>(num_leaves.size());
  // Check the modulus to satisfy statistical security.
  int64_t total_num_leaves =
      std::accumulate(num_leaves.begin(), num_leaves.end(), 0);
  // Statistical security needed for each leaf to ensure that all leaves are
  // uniform.
  double statistical_security_per_leaf =
      std::log2(double(total_num_leaves)) + statistical_security_;
  if (!ScalarHelper<T>::CanBeHashedInto(statistical_security_per_leaf)) {
    return mpc_utils::InvalidArgumentError(
        absl::StrCat("Cannot ensure statistical security of ",
                     statistical_security_, "bits with the given modulus"));
  }

  std::vector<std::unique_ptr<GGMTree>> trees(num_trees);
  std::vector<emp::block> opt0, opt1;
  std::vector<int> offsets(num_trees + 1, 0);
  std::vector<GGMTree::Block> keys(arity);
  for (int i = 0; i < arity; i++) {
    RAND_bytes(reinterpret_cast<uint8_t *>(&keys[i]), GGMTree::kBlockSize);
  }
  for (int i = 0; i < num_trees; i++) {
    if (num_leaves[i] == 0) {
      offsets[i + 1] = offsets[i];
      continue;
    }

    // Create tree from random seed.
    GGMTree::Block seed;
    RAND_bytes(reinterpret_cast<unsigned char *>(&seed), sizeof(seed));
    ASSIGN_OR_RETURN(trees[i], GGMTree::Create(num_leaves[i], seed, keys));

    offsets[i + 1] = offsets[i] + trees[i]->num_levels() - 1;
    assert(offsets[i + 1] >= offsets[i]);

    auto xors = trees[i]->GetSiblingWiseXOR();
    opt0.resize(offsets[i + 1]);
    opt1.resize(offsets[i + 1]);

    // Set opt0 (resp. opt1) to xor of left (resp. right) siblings for each
    // level
    for (int j = 0; j < static_cast<int>(trees[i]->num_levels()) - 1; ++j) {
      opt0[offsets[i] + j] =
          all_but_one_random_ot_internal::GGMTreeToEMPBlock(xors[j][0]);
      opt1[offsets[i] + j] =
          all_but_one_random_ot_internal::GGMTreeToEMPBlock(xors[j][1]);
    }
  }

  if (opt0.size() > 0) {
    ot_extension_.send(opt0.data(), opt1.data(), opt0.size());
  }

  channel_adapter_->send_data(keys.data(), sizeof(GGMTree::Block) * arity);
  channel_adapter_->flush();
  return trees;
}

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_ALL_BUT_ONE_RANDOM_OT_H_