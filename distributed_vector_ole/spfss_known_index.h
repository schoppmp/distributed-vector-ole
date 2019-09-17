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

#include "NTL/ZZ_p.h"
#include "NTL/lzz_p.h"
#include "absl/types/span.h"
#include "distributed_vector_ole/all_but_one_random_ot.h"
#include "mpc_utils/benchmarker.hpp"
#include "mpc_utils/boost_serialization/abseil.hpp"
#include "mpc_utils/boost_serialization/ntl.hpp"
#include "mpc_utils/comm_channel.hpp"
#include "mpc_utils/status_macros.h"
#include "mpc_utils/statusor.h"

// OpenMP custom reductions for NTL::ZZ_p and absl::uint128
#pragma omp declare reduction(+: NTL::ZZ_p: omp_out += omp_in) initializer (omp_priv = NTL::ZZ_p(0))
#pragma omp declare reduction(+: NTL::zz_p: omp_out += omp_in) initializer (omp_priv = NTL::zz_p(0))
#pragma omp declare reduction(+: absl::uint128: omp_out += omp_in) initializer (omp_priv = 0)

namespace distributed_vector_ole {

// This class implementes a a protocol for SPFSS with known index as described
// above that is based on (n-1)-out-of-n Random OT (by means of
// AllButOneRandomOT)
class SPFSSKnownIndex {
 public:
  // Creates an instance of SPFSSKnownIndex that communicates over the given
  // comm_channel. This corresponds to an instance of AllButOneRandomOT.
  static mpc_utils::StatusOr<std::unique_ptr<SPFSSKnownIndex>> Create(
      mpc_utils::comm_channel* channel);

  // Runs the ValueProvider side of the protocol. `output` must point to an
  // array of pre-allocated Ts.
  template <typename T>
  mpc_utils::Status RunValueProvider(T val_share, absl::Span<T> output);
  template <typename T>
  mpc_utils::StatusOr<std::vector<T>> RunValueProvider(T val_share,
                                                       int64_t size) {
    std::vector<T> output(size);
    RETURN_IF_ERROR(RunValueProvider<T>(val_share, absl::MakeSpan(output)));
    return output;
  }

  // Runs the IndexProvider side of the protocol. `output` must point to an
  // array of pre-allocated Ts, and `ìndex` must be between 0 and output.size()
  // - 1.
  template <typename T>
  mpc_utils::Status RunIndexProvider(T val_share, int64_t index,
                                     absl::Span<T> output);
  template <typename T>
  mpc_utils::StatusOr<std::vector<T>> RunIndexProvider(T val_share,
                                                       int64_t index,
                                                       int64_t size) {
    std::vector<T> output(size);
    RETURN_IF_ERROR(
        RunIndexProvider<T>(val_share, index, absl::MakeSpan(output)));
    return output;
  }

 private:
  SPFSSKnownIndex(mpc_utils::comm_channel* channel,
                  std::unique_ptr<AllButOneRandomOT> all_but_one_rot);

  mpc_utils::comm_channel* channel_;
  std::unique_ptr<AllButOneRandomOT> all_but_one_rot_;
};

template <typename T>
mpc_utils::Status SPFSSKnownIndex::RunValueProvider(T val_share,
                                                    absl::Span<T> output) {
  RETURN_IF_ERROR(all_but_one_rot_->RunSender(output));
  T sum(val_share);
  NTLContext<T> context;
  context.save();
#pragma omp parallel
  {
    context.restore();
#pragma omp for reduction(+ : sum) schedule(static)
    for (int64_t i = 0; i < static_cast<int64_t>(output.size()); ++i) {
      sum += output[i];
    }
#pragma omp for schedule(static)
    for (int64_t i = 0; i < static_cast<int64_t>(output.size()); ++i) {
      // Server negates their shares to obtain additively shared outputs.
      output[i] = -output[i];
    }
  }
  channel_->send(sum);
  channel_->flush();
  return mpc_utils::OkStatus();
}

template <typename T>
mpc_utils::Status SPFSSKnownIndex::RunIndexProvider(T val_share, int64_t index,
                                                    absl::Span<T> output) {
  RETURN_IF_ERROR(all_but_one_rot_->RunReceiver(index, output));
  T sum(val_share), sum_server;
  NTLContext<T> context;
  context.save();
#pragma omp parallel
  {
    context.restore();
#pragma omp for reduction(+ : sum) schedule(static)
    for (int64_t i = 0; i < static_cast<int64_t>(output.size()); ++i) {
      if (i != index) {
        sum -= output[i];
      }
    }
  }
  try {
    channel_->recv(sum_server);
  } catch (boost::exception& ex) {
    std::cerr << boost::diagnostic_information(ex);
    throw;
  }
  output[index] = sum + sum_server;
  return mpc_utils::OkStatus();
}

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_SPFSS_KNOWN_INDEX_H_