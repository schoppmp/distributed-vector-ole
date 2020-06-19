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

#ifndef DISTRIBUTED_VECTOR_OLE_SCALAR_VECTOR_GILBOA_H_
#define DISTRIBUTED_VECTOR_OLE_SCALAR_VECTOR_GILBOA_H_

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
#include "mpc_utils/status_macros.h"
#include "mpc_utils/statusor.h"

namespace distributed_vector_ole {

class ScalarVectorGilboaProduct {
 public:
  // Creates an instance of ScalarVectorGilboaProduct that
  // communicates over the given comm_channel
  static mpc_utils::StatusOr<std::unique_ptr<ScalarVectorGilboaProduct>> Create(
      comm_channel* channel, double statistical_security_ = 40);

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
      std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter,
      double statistical_security);

  template <typename T>
  bool UseCorrelatedOT(int64_t size) {
    // Compute statistical security needed for each element to ensure that
    // `size` elements can be computed using correlated OT.
    double statistical_security_per_element =
        std::log2(double(size)) + statistical_security_;
    int packed_hash_bits = ScalarHelper<T>::SizeOf() * 8;
    return ScalarHelper<T>::CanBeHashedInto(statistical_security_per_element,
                                            packed_hash_bits);
  }

  template <typename T>
  std::vector<emp::block> RunOTSender(absl::Span<const emp::block> deltas,
                                      uint64_t size) {
    if (UseCorrelatedOT<T>(size)) {
      return gilboa_internal::RunOTSenderCorrelated<T>(deltas, &ot_);
    } else {
      return gilboa_internal::RunOTSender1of2<T>(deltas, &ot_);
    }
  }

  template <typename T>
  std::vector<emp::block> RunOTReceiver(absl::Span<const bool> choices,
                                        uint64_t size) {
    std::vector<emp::block> result(choices.size());
    if (UseCorrelatedOT<T>(size)) {
      ot_.recv_cot(result.data(), choices.data(), choices.size());
    } else {
      ot_.recv(result.data(), choices.data(), choices.size());
    }
    return result;
  }

  std::unique_ptr<mpc_utils::CommChannelEMPAdapter> channel_adapter_;
  emp::SHOTExtension<mpc_utils::CommChannelEMPAdapter> ot_;
  double statistical_security_;
};

template <typename T>
mpc_utils::Status ScalarVectorGilboaProduct::RunVectorProvider(
    absl::Span<const T> y, absl::Span<T> output) {
  if (y.size() != output.size()) {
    return mpc_utils::InvalidArgumentError(
        "`y` and `output` must have the same size");
  }
  if (ScalarHelper<T>::SizeOf() > 16) {
    return mpc_utils::InvalidArgumentError(
        "Integers may be at most 16 bytes long");
  }
  int64_t y_len = y.size();
  int bit_width = ScalarHelper<T>::SizeOf() * 8;
  int stride = 16 / ScalarHelper<T>::SizeOf();  // Number of packed integers in
                                                // a single 128-bit OT message.
  int64_t n =
      bit_width * ((y_len + stride - 1) /
                   stride);  // Total number of OTs. Account for the fact
                             // that y_len might not be divisible by stride.
  std::vector<emp::block> deltas(n);
  NTL::Vec<T> powers_of_two;
  powers_of_two.SetLength(bit_width);
  for (int i = 0; i < bit_width; i++) {
    powers_of_two[i] = ScalarHelper<T>::SetBit(i);
  }
  for (int64_t i = 0; i < y_len; i += stride) {
    // `data` is the data to be packed in one OT execution.
    absl::Span<const T> data = y.subspan(i, stride);
    for (int j = 0; j < bit_width; j++) {
      // For each bit j of x, we run a COT with correlation correlation function
      // f(y) = 2^j * b - y, for which we store 2^j * b in deltas.
      deltas[(i / stride) * bit_width + j] =
          gilboa_internal::SpanToEMPBlock<T>(data, powers_of_two[j]);
    }
  }
  std::fill(output.begin(), output.end(), T(0));
  std::vector<emp::block> ot_result = RunOTSender<T>(deltas, y_len);
  channel_adapter_->flush();
  NTL::Vec<T> data;
  data.SetLength(stride);
  for (int64_t i = 0; i < y_len; i += stride) {
    for (int j = 0; j < bit_width; j++) {
      gilboa_internal::EMPBlockToSpan<T>(
          ot_result[(i / stride) * bit_width + j],
          absl::MakeSpan(data.data(), data.length()));
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
  if (ScalarHelper<T>::SizeOf() > 16) {
    return mpc_utils::InvalidArgumentError(
        "Integers may be at most 16 bytes long");
  }
  int bit_width = ScalarHelper<T>::SizeOf() * 8;
  int stride = 16 / ScalarHelper<T>::SizeOf();  // Number of packed integers in
                                                // a single 128-bit OT message.
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
  std::vector<emp::block> ot_result = RunOTReceiver<T>(choices, y_len);
  channel_adapter_->flush();
  NTL::Vec<T> data;
  data.SetLength(stride);
  for (int64_t i = 0; i < y_len; i += stride) {
    for (int j = 0; j < bit_width; j++) {
      gilboa_internal::EMPBlockToSpan<T>(
          ot_result[(i / stride) * bit_width + j],
          absl::MakeSpan(data.data(), data.length()));
      for (int k = 0; k < stride && (i + k) < y_len; ++k) {
        output[i + k] += data[k];
      }
    }
  }
  return mpc_utils::OkStatus();
}

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_SCALAR_VECTOR_GILBOA_H_