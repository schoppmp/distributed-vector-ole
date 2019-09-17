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
    std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter)
    : channel_(channel),
      channel_adapter_(std::move(channel_adapter)),
      ot_extension_(channel_adapter_.get()) {}

mpc_utils::StatusOr<std::unique_ptr<AllButOneRandomOT>>
AllButOneRandomOT::Create(mpc_utils::comm_channel* channel) {
  if (!channel) {
    return mpc_utils::InvalidArgumentError("`channel` must not be NULL");
  }
  // Create EMP adapter. Use a direct connection if channel is not measured.
  channel->sync();
  ASSIGN_OR_RETURN(auto adapter, mpc_utils::CommChannelEMPAdapter::Create(
                                     channel, !channel->is_measured()));
  return absl::WrapUnique(new AllButOneRandomOT(channel, std::move(adapter)));
}

mpc_utils::StatusOr<std::unique_ptr<GGMTree>> AllButOneRandomOT::ReceiveTree(
    int64_t num_leaves, int64_t index, int arity) {
  int num_levels =
      static_cast<int>(1 + std::ceil(std::log(num_leaves) / std::log(arity)));

  // std::vector<bool> is implemented as a bitstring, and thus
  // does not use a bool * internally, which EMP requires
  boost::container::vector<bool> choices(num_levels - 1);
  std::vector<emp::block> ot_result(num_levels - 1);

  // Set choice bits to the negation of the binary encoding of `index`
  uint64_t index_bits = static_cast<uint64_t>(index);
  for (int i = 0; i < num_levels - 1; ++i) {
    choices[num_levels - 2 - i] = (~index_bits >> i) & 1;
  }

  // Run num_levels-OT_{block_size} OT as a receiver, choosing
  // acccording to the bitwise negation of the binary representation of `index`
  ot_extension_.recv(ot_result.data(), choices.data(), num_levels - 1);

  // receive keys from sender
  std::vector<GGMTree::Block> keys(arity);
  channel_adapter_->recv_data(keys.data(), sizeof(GGMTree::Block) * arity);

  // Construct sibling-wise xor from the result of the OT,
  // ignoring the positions of the binary expansion of `index`.
  std::vector<std::vector<GGMTree::Block>> xors(
      num_levels - 1, std::vector<GGMTree::Block>(arity, 0));
  for (auto i = 0; i < num_levels - 1; ++i) {
    xors[i][choices[i] ? 1 : 0] =
        all_but_one_random_ot_internal::EMPToGGMTreeBlock(ot_result[i]);
  }
  ASSIGN_OR_RETURN(auto tree, GGMTree::CreateFromSiblingWiseXOR(
                                  arity, num_leaves, index, xors, keys));

  return tree;
}

}  // namespace distributed_vector_ole
