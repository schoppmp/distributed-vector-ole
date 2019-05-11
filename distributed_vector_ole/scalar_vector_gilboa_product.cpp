#include "distributed_vector_ole/scalar_vector_gilboa_product.h"
#include "mpc_utils/status_macros.h"

namespace distributed_vector_ole {

ScalarVectorGilboaProduct::ScalarVectorGilboaProduct(
    std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter)
    : channel_adapter_(std::move(channel_adapter)),
      ot_(channel_adapter_.get()) {}

mpc_utils::StatusOr<std::unique_ptr<ScalarVectorGilboaProduct>>
ScalarVectorGilboaProduct::Create(mpc_utils::comm_channel* channel) {
  if (!channel) {
    return mpc_utils::InvalidArgumentError("`channel` must not be NULL");
  }
  // Create EMP adapter. Use a direct connection if channel is not measured.
  channel->sync();
  ASSIGN_OR_RETURN(auto adapter, mpc_utils::CommChannelEMPAdapter::Create(
                                     channel, !channel->is_measured()));
  return absl::WrapUnique(new ScalarVectorGilboaProduct(std::move(adapter)));
}

}  // namespace distributed_vector_ole
