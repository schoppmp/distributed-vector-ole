// An implementation of Gilboa multiplication for scalar-vector product over a
// template type T. Inputs:
//
//    - VectorProvider: a vector `y` of of T
//    - ValueProvider: a number `x` of type T
//
// The output of the protocol is an additive share of `xy`. The implementation
// is based on 1-out-of-2 Correlated Oblivious Transfer (COT), and incorporates
// a packing optimization.

#ifndef DISTRIBUTED_VECTOR_OLE_SCALAR_VECTOR_GILBOA_H_
#define DISTRIBUTED_VECTOR_OLE_SCALAR_VECTOR_GILBOA_H_

#include <vector>
#include "distributed_vector_ole/internal/gilboa_internal.h"
#include "mpc_utils/canonical_errors.h"
#include "mpc_utils/status_macros.h"
#include "mpc_utils/statusor.h"

namespace distributed_vector_ole {

class ScalarVectorGilboaProduct {
 public:
  // Creates an instance of ScalarVectorGilboaProduct that
  // communicates over the given comm_channel
  static mpc_utils::StatusOr<std::unique_ptr<ScalarVectorGilboaProduct>> Create(
      comm_channel* channel);

  // Runs the Gilboa product with a vector `y` as input, writing the output to
  // `output`.
  template <typename T>
  mpc_utils::Status RunVectorProvider(absl::Span<const T> y,
                                      absl::Span<T> output);

  // Runs the Gilboa product with a vector `y` as input, returning the output as
  // a vector.
  template <typename T>
  mpc_utils::StatusOr<std::vector<T>> RunVectorProvider(absl::Span<const T> y) {
    std::vector<T> output(y.size());
    RETURN_IF_ERROR(RunVectorProvider(y, absl::MakeSpan(output)));
    return output;
  }

  // Runs the Gilboa product with a value x as input, writing the output to
  // `output`.
  template <typename T>
  mpc_utils::Status RunValueProvider(T x, absl::Span<T> output);

  // Runs the Gilboa product with a value x as input, returning the output as a
  // vector. `y_len` must be equal to the length of the other party's vector.
  template <typename T>
  mpc_utils::StatusOr<std::vector<T>> RunValueProvider(T x, int64_t y_len) {
    if (y_len < 0) {
      return mpc_utils::InvalidArgumentError("`y_len` must not be negative");
    }
    std::vector<T> output(y_len);
    RETURN_IF_ERROR(RunValueProvider(x, absl::MakeSpan(output)));
    return output;
  }

 private:
  ScalarVectorGilboaProduct(
      std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter);

  std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter_;
  emp::SHOTExtension<mpc_utils::CommChannelEMPAdapter> ot_;
};

template <typename T>
mpc_utils::Status ScalarVectorGilboaProduct::RunVectorProvider(
    absl::Span<const T> y, absl::Span<T> output) {
  if (y.size() != output.size()) {
    return mpc_utils::InvalidArgumentError(
        "`y` and `output` must have the same size");
  }
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
  NTL::Vec<T> powers_of_two;
  powers_of_two.SetLength(bit_width);
  for(int i = 0; i < bit_width; i++) {
    powers_of_two[i] = gilboa_internal::PowerOf2<T>(i);
  }
  for (int64_t i = 0; i < y_len; i += stride) {
    // `data` is the data to be packed in one OT execution.
    absl::Span<const T> data = y.subspan(i, stride);
    for (int j = 0; j < bit_width; j++) {
      // For each bit j of x, we run a COT with correlation correlation function
      // f(y) = 2^j * b - y, for which we store 2^j * b in deltas.
      deltas[(i / stride) * bit_width + j] = gilboa_internal::SpanToEMPBlock<T>(
          data, powers_of_two[j]);
    }
  }
  std::fill(output.begin(), output.end(), T(0));
  std::vector<emp::block> ot_result =
      gilboa_internal::RunOTSender<T>(deltas, &ot_);
  channel_adapter_->flush();
  NTL::Vec<T> data;
  data.SetLength(stride);
  for (int64_t i = 0; i < y_len; i += stride) {
    for (int j = 0; j < bit_width; j++) {
      gilboa_internal::EMPBlockToSpan<T>(
          ot_result[(i / stride) * bit_width + j], absl::MakeSpan(data.data(), data.length()));
      for (int k = 0; k < stride && (i + k) < y_len; ++k) {
        output[i + k] -= data[k];
      }
    }
  }
  return mpc_utils::OkStatus();
}

template <typename T>
mpc_utils::Status ScalarVectorGilboaProduct::RunValueProvider(
    T x, absl::Span<T> output) {
  int y_len = output.size();
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
  std::fill(output.begin(), output.end(), T(0));
  std::vector<emp::block> ot_result =
      gilboa_internal::RunOTReceiver<T>(choices, &ot_);
  channel_adapter_->flush();
  NTL::Vec<T> data;
  data.SetLength(stride);
  for (int64_t i = 0; i < y_len; i += stride) {
    for (int j = 0; j < bit_width; j++) {
      gilboa_internal::EMPBlockToSpan<T>(
          ot_result[(i / stride) * bit_width + j], absl::MakeSpan(data.data(), data.length()));
      for (int k = 0; k < stride && (i + k) < y_len; ++k) {
        output[i + k] += data[k];
      }
    }
  }
  return mpc_utils::OkStatus();
}

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_SCALAR_VECTOR_GILBOA_H_