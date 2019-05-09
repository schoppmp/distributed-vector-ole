// The server generates a GGMTree with N leaves and arity 2, and runs an
// 1-out-of-2 OT for each level of the tree, for the client to receive enough
// information to generates the tree locally, up to one path of the client's
// choice. The messages in the OTs are, per each level, the xor of the
// left(resp. right) children of all the nodes in that level. The OTs are
// implemented using EMP.

#include "distributed_vector_ole/all_but_one_random_ot.h"
#include "absl/memory/memory.h"
#include "boost/container/vector.hpp"
#include "distributed_vector_ole/ggm_tree.h"
#include "openssl/rand.h"

namespace distributed_vector_ole {

namespace {
// Conversion functions between EMP and GGMTree blocks.
GGMTree::Block EMPToGGMTreeBlock(emp::block in) {
  return absl::MakeUint128(static_cast<uint64_t>(in[1]),
                           static_cast<uint64_t>(in[0]));
}

emp::block GGMTreeToEMPBlock(GGMTree::Block in) {
  emp::block out;
  out[0] = static_cast<int64_t>(absl::Uint128Low64(in));
  out[1] = static_cast<int64_t>(absl::Uint128High64(in));
  return out;
}
}  // namespace

namespace all_but_one_random_ot_internal {

void UnpackLastLevel(const GGMTree& tree, absl::Span<NTL::ZZ_p> output) {
  // Save NTL context and restore it in each OMP thread.
  NTL::ZZ_pContext context;
  context.save();
#pragma omp parallel
  {
    context.restore();
    NTL::ZZ leaf_zz;
#pragma omp for schedule(guided)
    for (int64_t i = 0; i < tree.num_leaves(); ++i) {
      // ValieOrDie() is okay here as long as i stays in [0, num_leaves).
      GGMTree::Block leaf = tree.GetValueAtLeaf(i).ValueOrDie();
      leaf_zz = absl::Uint128High64(leaf);
      leaf_zz <<= 64;
      leaf_zz += absl::Uint128Low64(leaf);
      output[i] = NTL::conv<NTL::ZZ_p>(leaf_zz);
    }
  };
}

}  // namespace all_but_one_random_ot_internal

AllButOneRandomOT::AllButOneRandomOT(
    mpc_utils::comm_channel* channel,
    std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter)
    : channel_(channel),
      channel_adapter_(std::move(channel_adapter)),
      ot_extension_(channel_adapter_.get()) {}

mpc_utils::StatusOr<std::unique_ptr<AllButOneRandomOT>>
AllButOneRandomOT::Create(comm_channel* channel) {
  if (!channel) {
    return mpc_utils::InvalidArgumentError("`channel` must not be NULL");
  }
  // Create EMP adapter. Use a direct connection if channel is not measured.
  channel->sync();
  ASSIGN_OR_RETURN(auto adapter, mpc_utils::CommChannelEMPAdapter::Create(
                                     channel, !channel->is_measured()));
  return absl::WrapUnique(new AllButOneRandomOT(channel, std::move(adapter)));
}

mpc_utils::StatusOr<std::unique_ptr<GGMTree>> AllButOneRandomOT::ServerSendTree(
    int64_t num_leaves, int arity, const NTL::ZZ* modulus) {
  std::unique_ptr<GGMTree> tree;
  while (!tree) {
    // Create tree from random seed.
    GGMTree::Block seed;
    RAND_bytes(reinterpret_cast<unsigned char*>(&seed), sizeof(seed));
    ASSIGN_OR_RETURN(tree, GGMTree::Create(arity, num_leaves, seed));

    if (modulus) {
      // Re-sample tree if any leaf is too close to 2^128 to ensure values
      // reduced modulo `modulus` have all equal probability.
      NTL::ZZ upper_bound = (NTL::ZZ(1) << 128);
      upper_bound -= upper_bound % *modulus;
      bool tree_valid = true;

#pragma omp parallel
      {
        NTL::ZZ leaf_zz;
#pragma omp for reduction(& : tree_valid) schedule(guided)
        for (int64_t i = 0; i < num_leaves; i++) {
          // ValieOrDie() is okay here as long as i stays in [0, num_leaves).
          GGMTree::Block leaf = tree->GetValueAtLeaf(i).ValueOrDie();
          leaf_zz = absl::Uint128High64(leaf);
          leaf_zz <<= 64;
          leaf_zz += absl::Uint128Low64(leaf);
          tree_valid &= (leaf_zz < upper_bound);
        }
      }
      if (!tree_valid) {
        tree.reset();
      }
    }
  }

  auto xors = tree->GetSiblingWiseXOR();

  std::vector<emp::block> opt0(tree->num_levels() - 1);
  std::vector<emp::block> opt1(tree->num_levels() - 1);

  // Set opt0 (resp. opt1) to xor of left (resp. right) siblings for each level
  for (auto i = 0; i < tree->num_levels() - 1; ++i) {
    opt0[i] = GGMTreeToEMPBlock(xors[i][0]);
    opt1[i] = GGMTreeToEMPBlock(xors[i][1]);
  }

  ot_extension_.send(opt0.data(), opt1.data(), tree->num_levels() - 1);
  channel_adapter_->send_data(tree->keys().data(),
                              sizeof(GGMTree::Block) * arity);
  channel_adapter_->flush();

  return tree;
}

mpc_utils::StatusOr<std::unique_ptr<GGMTree>>
AllButOneRandomOT::ClientReceiveTree(int64_t num_leaves, int64_t index,
                                     int arity) {
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
    xors[i][choices[i] ? 1 : 0] = EMPToGGMTreeBlock(ot_result[i]);
  }
  ASSIGN_OR_RETURN(auto tree, GGMTree::CreateFromSiblingWiseXOR(
                                  arity, num_leaves, index, xors, keys));

  return tree;
}

}  // namespace distributed_vector_ole
