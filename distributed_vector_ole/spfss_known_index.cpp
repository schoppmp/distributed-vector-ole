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

#include "distributed_vector_ole/spfss_known_index.h"

namespace distributed_vector_ole {

SPFSSKnownIndex::SPFSSKnownIndex(
    mpc_utils::comm_channel* channel,
    std::unique_ptr<AllButOneRandomOT> all_but_one_rot)
    : channel_(channel), all_but_one_rot_(std::move(all_but_one_rot)) {}

mpc_utils::StatusOr<std::unique_ptr<SPFSSKnownIndex>> SPFSSKnownIndex::Create(
    mpc_utils::comm_channel* channel, double statistical_security) {
  if (!channel) {
    return mpc_utils::InvalidArgumentError("`channel` must not be NULL");
  }
  if (statistical_security < 0) {
    return mpc_utils::InvalidArgumentError(
        "`statistical_security` must not be negative.");
  }
  // Create AllButOneRandomOT protocol.
  ASSIGN_OR_RETURN(auto all_but_one_rot,
                   AllButOneRandomOT::Create(channel, statistical_security));
  return absl::WrapUnique(
      new SPFSSKnownIndex(channel, std::move(all_but_one_rot)));
}

}  // namespace distributed_vector_ole
