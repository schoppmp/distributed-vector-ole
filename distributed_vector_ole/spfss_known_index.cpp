#include "distributed_vector_ole/spfss_known_index.h"

namespace distributed_vector_ole {

SPFSSKnownIndex::SPFSSKnownIndex(
    mpc_utils::comm_channel* channel,
    std::unique_ptr<AllButOneRandomOT> all_but_one_rot)
    : channel_(channel), all_but_one_rot_(std::move(all_but_one_rot)) {}

mpc_utils::StatusOr<std::unique_ptr<SPFSSKnownIndex>> SPFSSKnownIndex::Create(
    comm_channel* channel) {
  if (!channel) {
    return mpc_utils::InvalidArgumentError("`channel` must not be NULL");
  }
  // Create AllButOneRandomOT protocol.
  ASSIGN_OR_RETURN(auto all_but_one_rot, AllButOneRandomOT::Create(channel));
  return absl::WrapUnique(
      new SPFSSKnownIndex(channel, std::move(all_but_one_rot)));
}

}  // namespace distributed_vector_ole
