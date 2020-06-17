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

#include "distributed_vector_ole/all_but_one_random_ot.h"
#include "absl/memory/memory.h"
#include "boost/container/vector.hpp"

// The sender generates a GGMTree with N leaves and arity 2, and runs an
// 1-out-of-2 OT for each level of the tree, for the client to receive enough
// information to generates the tree locally, up to one path of the receiver's
// choice. The messages in the OTs are, per each level, the xor of the
// left(resp. right) children of all the nodes in that level. The OTs are
// implemented using EMP.

namespace distributed_vector_ole {

AllButOneRandomOT::AllButOneRandomOT(
    mpc_utils::comm_channel* channel,
    std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter,
    double statistical_security)
    : channel_(channel),
      channel_adapter_(std::move(channel_adapter)),
      ot_extension_(channel_adapter_.get()),
      statistical_security_(statistical_security) {}

mpc_utils::StatusOr<std::unique_ptr<AllButOneRandomOT>>
AllButOneRandomOT::Create(mpc_utils::comm_channel* channel,
                          double statistical_security) {
  if (!channel) {
    return mpc_utils::InvalidArgumentError("`channel` must not be NULL");
  }
  if (statistical_security < 0) {
    return mpc_utils::InvalidArgumentError(
        "`statistical_security` must not be negative.");
  }
  // Create EMP adapter. Use a direct connection if channel is not measured.
  channel->sync();
  ASSIGN_OR_RETURN(auto adapter, mpc_utils::CommChannelEMPAdapter::Create(
                                     channel, !channel->is_measured()));
  return absl::WrapUnique(
      new AllButOneRandomOT(channel, std::move(adapter), statistical_security));
}

mpc_utils::StatusOr<std::vector<std::unique_ptr<GGMTree>>>
AllButOneRandomOT::ReceiveTrees(absl::Span<const int64_t> num_leaves,
                                absl::Span<const int64_t> indices, int arity) {
  if (num_leaves.size() != indices.size()) {
    return mpc_utils::InvalidArgumentError(
        "`num_leaves` and `indices` must have the same size");
  }
  int num_trees = static_cast<int>(num_leaves.size());
  // std::vector<bool> is implemented as a bitstring, and thus does not use a
  // bool * internally, which EMP requires. So we use
  // boost::container::vector<bool> instead.
  boost::container::vector<bool> choices;
  std::vector<emp::block> ot_results;
  std::vector<int> offsets(num_trees + 1, 0);
  for (int i = 0; i < num_trees; i++) {
    if (num_leaves[i] == 0) {
      offsets[i + 1] = offsets[i];
      continue;
    }

    int num_levels = static_cast<int>(
        1 + std::ceil(std::log(num_leaves[i]) / std::log(arity)));
    offsets[i + 1] = offsets[i] + num_levels - 1;
    choices.resize(offsets[i + 1]);

    // Set choice bits to the negation of the binary encoding of `indices[i]`.
    uint64_t index_bits = static_cast<uint64_t>(indices[i]);
    for (int j = 0; j < num_levels - 1; ++j) {
      choices[offsets[i] + num_levels - 2 - j] = (~index_bits >> j) & 1;
    }
  }
  ot_results.resize(choices.size());
  // Run num_levels-OT_{block_size} OT as a receiver, choosing
  // acccording to the bitwise negation of the binary representation of
  // `indices[i]`
  if (choices.size() > 0) {
    ot_extension_.recv(ot_results.data(), choices.data(), choices.size());
  }

  // Receive keys from sender.
  std::vector<GGMTree::Block> keys(arity);
  channel_adapter_->recv_data(keys.data(), sizeof(GGMTree::Block) * arity);

  mpc_utils::Status status = mpc_utils::OkStatus();
  std::vector<std::unique_ptr<GGMTree>> trees(num_trees);
#pragma omp parallel for schedule(dynamic)
  for (int i = 0; i < num_trees; i++) {
    if (num_leaves[i] == 0) {
      continue;
    }
    int num_levels = offsets[i + 1] - offsets[i] + 1;
    assert(num_levels >= 1);
    // Construct sibling-wise xor from the result of the OT,
    // ignoring the positions of the binary expansion of `index`.
    std::vector<std::vector<GGMTree::Block>> xors(
        num_levels - 1, std::vector<GGMTree::Block>(arity, 0));
    for (int j = 0; j < num_levels - 1; ++j) {
      xors[j][choices[offsets[i] + j] ? 1 : 0] =
          all_but_one_random_ot_internal::EMPToGGMTreeBlock(
              ot_results[offsets[i] + j]);
    }
    auto status_or_tree = GGMTree::CreateFromSiblingWiseXOR(
        arity, num_leaves[i], indices[i], xors, keys);
    if (status_or_tree.ok()) {
      trees[i] = std::move(status_or_tree.ValueOrDie());
    } else {
      status = status_or_tree.status();
    }
  }
  if (status.ok()) {
    return trees;
  }
  return status;
}

}  // namespace distributed_vector_ole
