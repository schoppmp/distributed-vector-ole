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

#include "distributed_vector_ole/scalar_vector_gilboa_product.h"

#include "mpc_utils/status_macros.h"

namespace distributed_vector_ole {

ScalarVectorGilboaProduct::ScalarVectorGilboaProduct(
    std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter,
    double statistical_security)
    : channel_adapter_(std::move(channel_adapter)),
      ot_(channel_adapter_.get()),
      statistical_security_(statistical_security) {}

mpc_utils::StatusOr<std::unique_ptr<ScalarVectorGilboaProduct>>
ScalarVectorGilboaProduct::Create(mpc_utils::comm_channel* channel,
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
      new ScalarVectorGilboaProduct(std::move(adapter), statistical_security));
}

}  // namespace distributed_vector_ole
