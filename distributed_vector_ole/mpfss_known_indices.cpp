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

#include "distributed_vector_ole/mpfss_known_indices.h"

#include <numeric>

#include "openssl/rand.h"

namespace distributed_vector_ole {

MPFSSKnownIndices::MPFSSKnownIndices(std::unique_ptr<CuckooHasher> hasher,
                                     std::unique_ptr<SPFSSKnownIndex> spfss,
                                     mpc_utils::comm_channel* channel)
    : hasher_(std::move(hasher)), spfss_(std::move(spfss)), channel_(channel) {}

mpc_utils::StatusOr<std::unique_ptr<MPFSSKnownIndices>>
MPFSSKnownIndices::Create(mpc_utils::comm_channel* channel,
                          double statistical_security) {
  if (!channel) {
    return mpc_utils::InvalidArgumentError("`channel` must not be NULL");
  }
  if (statistical_security < 0) {
    return mpc_utils::InvalidArgumentError(
        "`statistical_security` must not be negative.");
  }
  // Make sure we have enough bits of statistical security for SPFSS and
  // Cuckoo hashing to fail independently.
  statistical_security += 1;

  std::unique_ptr<SPFSSKnownIndex> spfss;
  channel->sync();
  ASSIGN_OR_RETURN(spfss,
                   SPFSSKnownIndex::Create(channel, statistical_security));

  // Seed CuckooHasher: lower ID sends seed to higher ID.
  absl::uint128 hasher_seed;
  if (channel->get_id() < channel->get_peer_id()) {
    ScalarHelper<absl::uint128>::Randomize(absl::MakeSpan(&hasher_seed, 1));
    channel->send(hasher_seed);
    channel->flush();
  } else {
    channel->recv(hasher_seed);
  }

  // Allocate CuckooHasher.
  ASSIGN_OR_RETURN(auto hasher,
                   CuckooHasher::Create(hasher_seed, kNumHashFunctions,
                                        statistical_security));

  return absl::WrapUnique(
      new MPFSSKnownIndices(std::move(hasher), std::move(spfss), channel));
}

mpc_utils::Status MPFSSKnownIndices::UpdateBuckets(int64_t output_size,
                                                   int num_indices) {
  bool needs_update =
      !cached_output_size_ || *cached_output_size_ != output_size ||
      !cached_num_indices_ || *cached_num_indices_ != num_indices;
  if (needs_update) {
    std::vector<int64_t> all_indices(output_size);
    std::iota(all_indices.begin(), all_indices.end(), 0);
    ASSIGN_OR_RETURN(int64_t num_buckets,
                     hasher_->GetOptimalNumberOfBuckets(num_indices));
    ASSIGN_OR_RETURN(
        buckets_,
        hasher_->HashSimple(absl::MakeConstSpan(all_indices), num_buckets));
    cached_output_size_ = output_size;
    cached_num_indices_ = num_indices;
  }
  return mpc_utils::OkStatus();
}

}  // namespace distributed_vector_ole