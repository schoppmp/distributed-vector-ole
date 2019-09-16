#include "distributed_vector_ole/mpfss_known_indices.h"
#include <numeric>
#include "openssl/rand.h"

namespace distributed_vector_ole {

MPFSSKnownIndices::MPFSSKnownIndices(
    std::unique_ptr<CuckooHasher> hasher,
    std::vector<std::unique_ptr<SPFSSKnownIndex>> spfss,
    std::vector<std::unique_ptr<mpc_utils::comm_channel>> channels,
    std::unique_ptr<ScalarVectorGilboaProduct> owned_gilboa,
    ScalarVectorGilboaProduct* gilboa)
    : hasher_(std::move(hasher)),
      spfss_(std::move(spfss)),
      channels_(std::move(channels)),
      owned_gilboa_(std::move(owned_gilboa)),
      gilboa_(gilboa) {}

mpc_utils::StatusOr<std::unique_ptr<MPFSSKnownIndices>>
MPFSSKnownIndices::Create(mpc_utils::comm_channel* channel,
                          ScalarVectorGilboaProduct* gilboa) {
#ifdef _OPENMP
  int num_threads = omp_get_max_threads();
#else
  int num_threads = 1;
#endif
  if (channel->is_measured()) {
    // Communication gets only measured on a single channel.
    num_threads = 1;
  }

  std::vector<std::unique_ptr<SPFSSKnownIndex>> spfss(num_threads);
  std::vector<std::unique_ptr<mpc_utils::comm_channel>> channels(num_threads);
  if (num_threads == 1) {
    // Use a single channel if we only have one thread or are measuring
    // communication.
    channel->sync();
    ASSIGN_OR_RETURN(spfss[0], SPFSSKnownIndex::Create(channel));
    channels.resize(0);
  } else {
    // Create SPFSS instances with a new comm_channel each.
    for (int i = 0; i < num_threads; i++) {
      channel->sync();
      channels[i] =
          absl::WrapUnique(new mpc_utils::comm_channel(channel->clone()));
      channels[i]->sync();
      ASSIGN_OR_RETURN(spfss[i], SPFSSKnownIndex::Create(channels[i].get()));
    }
  }

  // Seed CuckooHasher: lower ID sends seed to higher ID.
  std::vector<uint8_t> hasher_seed(16);
  if (channel->get_id() < channel->get_peer_id()) {
    RAND_bytes(hasher_seed.data(), hasher_seed.size());
    channel->send(hasher_seed);
    channel->flush();
  } else {
    channel->recv(hasher_seed);
  }

  // Allocate CuckooHasher.
  ASSIGN_OR_RETURN(
      auto hasher,
      CuckooHasher::Create(
          absl::string_view(reinterpret_cast<const char*>(hasher_seed.data()),
                            hasher_seed.size()),
          kNumHashFunctions));

  // Allocate Gilboa instance if we didn't get one from the caller.
  std::unique_ptr<ScalarVectorGilboaProduct> owned_gilboa = nullptr;
  if (!gilboa) {
    ASSIGN_OR_RETURN(owned_gilboa, ScalarVectorGilboaProduct::Create(channel));
    gilboa = owned_gilboa.get();
  }

  return absl::WrapUnique(new MPFSSKnownIndices(
      std::move(hasher), std::move(spfss), std::move(channels),
      std::move(owned_gilboa), gilboa));
}

mpc_utils::Status MPFSSKnownIndices::UpdateBuckets(int64_t output_size,
                                                   int num_indices) {
  if (!cached_output_size_ || *cached_output_size_ != output_size) {
    std::vector<int64_t> all_indices(output_size);
    std::iota(all_indices.begin(), all_indices.end(), 0);
    ASSIGN_OR_RETURN(buckets_,
                     hasher_->HashSimple(absl::MakeConstSpan(all_indices),
                                         NumBuckets(num_indices)));
    cached_output_size_ = output_size;
  }
  return mpc_utils::OkStatus();
}

}  // namespace distributed_vector_ole