// An implementation of Gilboa multiplication for scalar-vector product over a
// template type T. Inputs:
//
//    - VectorProvider: a vector `y` of of T
//    - ValueProvider: a number `x` of type T
//
// The output of the protocol is an additive share of `xy`. The implementation
// is based on 1-out-of-2 Correlated Oblivious Transfer (COT), and incorporates
// a packing optimization.

#include <vector>
#include "distributed_vector_ole/internal/gilboa_internal.h"
#include "mpc_utils/canonical_errors.h"
#include "mpc_utils/statusor.h"

namespace distributed_vector_ole {

class ScalarVectorGilboaProduct {
 public:
  // Creates an instance of ScalarVectorGilboaProduct that
  // communicates over the given comm_channel
  static mpc_utils::StatusOr<std::unique_ptr<ScalarVectorGilboaProduct>> Create(
      comm_channel* channel);

  // Runs the Gilboa product with a vector `y` as input.
  template <typename T>
  mpc_utils::StatusOr<std::vector<T>> RunVectorProvider(absl::Span<const T> y);

  // Runs the Gilboa product with a value x as input. `y_len` must be equal to
  // the length of the other party's vector.
  template <typename T>
  mpc_utils::StatusOr<std::vector<T>> RunValueProvider(T x, int64_t y_len);

 private:
  ScalarVectorGilboaProduct(
      std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter);

  std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter_;
  emp::SHOTExtension<mpc_utils::CommChannelEMPAdapter> ot_;
};

template <typename T>
mpc_utils::StatusOr<std::vector<T>>
ScalarVectorGilboaProduct::RunVectorProvider(absl::Span<const T> y) {
  if (gilboa_internal::SizeOf<T>() > 16) {
    return mpc_utils::InvalidArgumentError(
        "Integers may be at most 16 bytes long");
  }
  int64_t y_len = y.size();
  int bit_width = gilboa_internal::SizeOf<T>() * 8;
  int stride =
      16 / gilboa_internal::SizeOf<T>();  // Number of packed integers in a
                                          // single 128-bit OT message.
  int64_t n =
      bit_width * ((y_len + stride - 1) /
                   stride);  // Total number of OTs. Account for the fact
                             // that y_len might not be divisible by stride.
  std::vector<emp::block> deltas(n);
  for (int64_t i = 0; i < y_len; i += stride) {
    // `data` is the data to be packed in one OT execution.
    absl::Span<const T> data = y.subspan(i, stride);
    for (int j = 0; j < bit_width; j++) {
      // For each bit j of x, we run a COT with correlation correlation function
      // f(y) = 2^j * b - y, for which we store 2^j * b in deltas.
      deltas[(i / stride) * bit_width + j] = gilboa_internal::SpanToEMPBlock<T>(
          data, gilboa_internal::PowerOf2<T>(j));
    }
  }
  std::vector<T> result_share(y_len, T(0));
  std::vector<emp::block> ot_result =
      gilboa_internal::RunOTSender<T>(deltas, &ot_);
  channel_adapter_->flush();
  for (int64_t i = 0; i < y_len; i += stride) {
    for (int j = 0; j < bit_width; j++) {
      std::vector<T> data = gilboa_internal::EMPBlockToVector<T>(
          ot_result[(i / stride) * bit_width + j]);
      for (int k = 0; k < stride && (i + k) < y_len; ++k) {
        result_share[i + k] -= data[k];
      }
    }
  }
  return result_share;
}

template <typename T>
mpc_utils::StatusOr<std::vector<T>> ScalarVectorGilboaProduct::RunValueProvider(
    T x, int64_t y_len) {
  if (y_len < 0) {
    return mpc_utils::InvalidArgumentError("`y_len` must not be negative");
  }
  if (gilboa_internal::SizeOf<T>() > 16) {
    return mpc_utils::InvalidArgumentError(
        "Integers may be at most 16 bytes long");
  }
  int bit_width = gilboa_internal::SizeOf<T>() * 8;
  int stride =
      16 / gilboa_internal::SizeOf<T>();  // Number of packed integers in a
                                          // single 128-bit OT message.
  int64_t n =
      bit_width * ((y_len + stride - 1) /
                   stride);  // Total number of OTs. Account for the fact
                             // that y_len might not be divisible by stride.
  boost::container::vector<bool> x_bits = gilboa_internal::GetBits(x);
  boost::container::vector<bool> choices(n);
  for (int64_t i = 0; i < y_len; i += stride) {
    for (int j = 0; j < bit_width; j++) {
      choices[(i / stride) * bit_width + j] = x_bits[j];
    }
  }
  std::vector<T> result_share(y_len, T(0));
  std::vector<emp::block> ot_result =
      gilboa_internal::RunOTReceiver<T>(choices, &ot_);
  channel_adapter_->flush();
  for (int64_t i = 0; i < y_len; i += stride) {
    for (int j = 0; j < bit_width; j++) {
      std::vector<T> data = gilboa_internal::EMPBlockToVector<T>(
          ot_result[(i / stride) * bit_width + j]);
      for (int k = 0; k < stride && (i + k) < y_len; ++k) {
        result_share[i + k] += data[k];
      }
    }
  }
  return result_share;
}

}  // namespace distributed_vector_ole
