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

#ifndef DISTRIBUTED_VECTOR_OLE_SPFSS_KNOWN_INDEX_H_
#define DISTRIBUTED_VECTOR_OLE_SPFSS_KNOWN_INDEX_H_

// A two party protocol involving two parties with the following inputs:
//
//   Public: Integer N > 0.
//   IndexProvider: An additive share of `val`, and an index 0 <= `index` <=
//   N-1. ValueProvider: An additive share of `val`,
//
// The output of the protocol is an additive secret share of a vector `v` of
// length `N` such that `v` is zero in all positions except `index`, where
// `v[index] = val`.

#include "absl/types/span.h"
#include "distributed_vector_ole/all_but_one_random_ot.h"
#include "distributed_vector_ole/internal/ntl_helpers.h"
#include "distributed_vector_ole/internal/scalar_helpers.h"
#include "mpc_utils/boost_serialization/abseil.hpp"
#include "mpc_utils/boost_serialization/ntl.hpp"
#include "mpc_utils/canonical_errors.h"
#include "mpc_utils/comm_channel.hpp"
#include "mpc_utils/status_macros.h"
#include "mpc_utils/statusor.h"

namespace distributed_vector_ole {

// This class implementes a a protocol for SPFSS with known index as described
// above that is based on (n-1)-out-of-n Random OT (by means of
// AllButOneRandomOT)
class SPFSSKnownIndex {
 public:
  // Creates an instance of SPFSSKnownIndex that communicates over the given
  // comm_channel. This corresponds to an instance of AllButOneRandomOT.
  static mpc_utils::StatusOr<std::unique_ptr<SPFSSKnownIndex>> Create(
      mpc_utils::comm_channel *channel, double statistical_security = 40);

  // Runs the ValueProvider side of the protocol. `output` must point to an
  // array of pre-allocated Ts.
  template <typename T>
  mpc_utils::Status RunValueProvider(T val_share, absl::Span<T> output) {
    return RunValueProviderBatched(absl::MakeConstSpan(&val_share, 1), absl::MakeSpan(&output, 1));
  }
  template <typename T>
  mpc_utils::StatusOr<std::vector<T>> RunValueProvider(T val_share, int64_t size) {
    std::vector<T> output(size);
    RETURN_IF_ERROR(RunValueProvider<T>(val_share, absl::MakeSpan(output)));
    return output;
  }
  template <typename T>
  mpc_utils::Status RunValueProviderBatched(absl::Span<const T> val_shares,
                                            absl::Span<absl::Span<T>> outputs);

  // Runs the IndexProvider side of the protocol. `output` must point to an
  // array of pre-allocated Ts, and `ìndex` must be between 0 and output.size()
  // - 1.
  template <typename T>
  mpc_utils::Status RunIndexProvider(T val_share, int64_t index, absl::Span<T> output) {
    return RunIndexProviderBatched(absl::MakeConstSpan(&val_share, 1),
                                   absl::MakeConstSpan(&index, 1), absl::MakeSpan(&output, 1));
  }
  template <typename T>
  mpc_utils::StatusOr<std::vector<T>> RunIndexProvider(T val_share, int64_t index, int64_t size) {
    std::vector<T> output(size);
    RETURN_IF_ERROR(RunIndexProvider<T>(val_share, index, absl::MakeSpan(output)));
    return output;
  }
  template <typename T>
  mpc_utils::Status RunIndexProviderBatched(absl::Span<const T> val_shares,
                                            absl::Span<const int64_t> indices,
                                            absl::Span<absl::Span<T>> outputs);

 private:
  SPFSSKnownIndex(mpc_utils::comm_channel *channel,
                  std::unique_ptr<AllButOneRandomOT> all_but_one_rot);

  mpc_utils::comm_channel *channel_;
  std::unique_ptr<AllButOneRandomOT> all_but_one_rot_;
};

template <typename T>
mpc_utils::Status SPFSSKnownIndex::RunValueProviderBatched(absl::Span<const T> val_shares,
                                                           absl::Span<absl::Span<T>> outputs) {
  if (val_shares.size() != outputs.size()) {
    return mpc_utils::InvalidArgumentError("`val-shares` and `outputs` must have the same size");
  }
  RETURN_IF_ERROR(all_but_one_rot_->RunSenderBatched(outputs));
  std::vector<T> sums(val_shares.begin(), val_shares.end());
  T *sums_data = sums.data();  // OpenMP wants a pointer.
  int len = static_cast<int>(val_shares.size());
  NTLContext<T> context;
  context.save();
#pragma omp parallel for schedule(static)
  for (int j = 0; j < len; j++) {
#pragma omp parallel
    {
      context.restore();
#pragma omp for reduction(+ : sums_data[:len]) schedule(static)
      for (int64_t i = 0; i < static_cast<int64_t>(outputs[j].size()); ++i) {
        sums_data[j] += outputs[j][i];
        outputs[j][i] = -outputs[j][i];
      }
    };
  }
  channel_->send(sums);
  channel_->flush();
  return mpc_utils::OkStatus();
}

template <typename T>
mpc_utils::Status SPFSSKnownIndex::RunIndexProviderBatched(absl::Span<const T> val_shares,
                                                           absl::Span<const int64_t> indices,
                                                           absl::Span<absl::Span<T>> outputs) {
  if (val_shares.size() != outputs.size() || val_shares.size() != indices.size()) {
    return mpc_utils::InvalidArgumentError(
        "`val-shares`, `indices`, and `outputs` must have the same size");
  }
  RETURN_IF_ERROR(all_but_one_rot_->RunReceiverBatched(indices, outputs));
  std::vector<T> sums(val_shares.begin(), val_shares.end());
  std::vector<T> sums_server;
  T *sums_data = sums.data();  // OpenMP wants a pointer.
  int len = static_cast<int>(val_shares.size());
  NTLContext<T> context;
  context.save();
#pragma omp parallel for schedule(static)
  for (int j = 0; j < len; j++) {
#pragma omp parallel
    {
      context.restore();
#pragma omp for reduction(+ : sums_data[:len]) schedule(static)
      for (int64_t i = 0; i < static_cast<int64_t>(outputs[j].size()); ++i) {
        if (i != indices[j]) {
          sums_data[j] -= outputs[j][i];
        }
      }
    };
  }
  channel_->recv(sums_server);
  for (int j = 0; j < len; j++) {
    if (outputs[j].empty()) {
      continue;
    }
    outputs[j][indices[j]] = sums[j] + sums_server[j];
  }
  return mpc_utils::OkStatus();
}

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_SPFSS_KNOWN_INDEX_H_