// Single-Point FSS with known index (index provided by client)
// A two party protocol involving a server and a client with:
// Public input: integer N > 0.
// Server input: an additive secret share of `val`,
// client inputs: an additive secret share of `val`, and
// an index 0 <= `index` <= N-1.
// The output of the protocol is an additive secret share
// of a vector `v` of length `N` such that
// v is zero in all positions except N, where v[N] = val.

#include "absl/types/span.h"
#include "distributed_vector_ole/all_but_one_random_ot.h"
#include "mpc_utils/benchmarker.hpp"
#include "mpc_utils/boost_serialization/abseil.hpp"
#include "mpc_utils/comm_channel.hpp"
#include "mpc_utils/status_macros.h"
#include "mpc_utils/statusor.h"

namespace distributed_vector_ole {

// This class implementes a a protocol for SPFSS with known
// index as described above that is
// based on (n-1)-out-of-n Random OT (by means of AllButOneRandomOT)
class SPFSSKnownIndex {
 public:
  // Creates an instance of SPFSSKnownIndex that communicates over the given
  // comm_channel. This corresponds to an instance of AllButOneRandomOT.
  static mpc_utils::StatusOr<std::unique_ptr<SPFSSKnownIndex>> Create(
      comm_channel* channel);

  // Runs the Server side of the protocol. `output` must point to an array of
  // pre-allocated Ts.
  template <typename T>
  mpc_utils::Status RunServer(T val_share, absl::Span<T> output,
                              mpc_utils::Benchmarker* benchmarker = nullptr) {
    RETURN_IF_ERROR(all_but_one_rot_->RunServer(output));
    T sum = val_share;
    for (int64_t i = 0; i < static_cast<int64_t>(output.size()); ++i) {
      sum += output[i];
      output[i] = -output[i];
    }
    channel_->send(sum);
    channel_->flush();
    return mpc_utils::OkStatus();
  }

  // Runs the Server side of the protocol. `output` must point to an array of
  // pre-allocated Ts, and `ìndex` must be between 0 and output.size() - 1.
  template <typename T>
  mpc_utils::Status RunClient(T val_share, int64_t index, absl::Span<T> output,
                              mpc_utils::Benchmarker* benchmarker = nullptr) {
    RETURN_IF_ERROR(all_but_one_rot_->RunClient(index, output));
    T sum;
    channel_->recv(sum);
    sum += val_share;
    for (int64_t i = 0; i < static_cast<int64_t>(output.size()); ++i) {
      if (i != index) {
        sum -= output[i];
      }
    }
    output[index] = sum;
    return mpc_utils::OkStatus();
  }

 private:
  SPFSSKnownIndex(mpc_utils::comm_channel* channel,
                  std::unique_ptr<AllButOneRandomOT> all_but_one_rot);

  mpc_utils::comm_channel* channel_;
  std::unique_ptr<AllButOneRandomOT> all_but_one_rot_;
};
}  // namespace distributed_vector_ole
