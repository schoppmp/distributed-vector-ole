// Helper functions for computing Gilboa products with arbitrary types.

#ifndef DISTRIBUTED_VECTOR_OLE_INTERNAL_GILBOA_INTERNAL_H_
#define DISTRIBUTED_VECTOR_OLE_INTERNAL_GILBOA_INTERNAL_H_

#include "NTL/ZZ_p.h"
#include "NTL/vec_ZZ_p.h"
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "boost/container/vector.hpp"
#include "emp-ot/emp-ot.h"
#include "mpc_utils/comm_channel.hpp"
#include "mpc_utils/comm_channel_emp_adapter.hpp"
#include "openssl/rand.h"

namespace distributed_vector_ole {

namespace gilboa_internal {

// Returns the size (in bytes) of the type parameter or the argument.
template <typename T>
constexpr int SizeOf() {
  return sizeof(T);
}
template <>
inline int SizeOf<NTL::ZZ_p>() {
  int num_bits = NTL::NumBits(NTL::ZZ_p::modulus() - 1);
  return (num_bits + 7) / 8;
}
template <typename T>
constexpr int SizeOf(T x) {
  return SizeOf<T>();
}
inline int SizeOf(const NTL::ZZ_p &x) { return SizeOf<NTL::ZZ_p>(); }

// Returns the k-th bit of x.
template <typename T>
bool GetBit(T x, int k) {
  return ((x >> k) & T(1)) != 0 ? true : false;
}
inline bool GetBit(const NTL::ZZ_p &x, int k) {
  return NTL::bit(NTL::rep(x), k);
}

// Returns the bit decomposition of a type T value
template <typename T>
boost::container::vector<bool> GetBits(T x) {
  boost::container::vector<bool> out(SizeOf<T>() * 8);
  for (int i = 0; i < static_cast<int>(out.size()); ++i) {
    out[i] = GetBit(x, i);
  }
  return out;
}

// Returns the j-th power of T(2).
template <typename T>
T PowerOf2(int j) {
  return T(1) << j;
}
template <>
inline NTL::ZZ_p PowerOf2<NTL::ZZ_p>(int j) {
  return NTL::conv<NTL::ZZ_p>(NTL::ZZ(1) << j);
}

// Conversions between absl::uint128 and T.
template <typename T>
absl::uint128 ToUint128(const T &x) {
  return x;
}
template <>
inline absl::uint128 ToUint128<NTL::ZZ_p>(const NTL::ZZ_p &x) {
  NTL::ZZ x_zz = NTL::rep(x);
  uint64_t low = NTL::conv<uint64_t>(x_zz);
  x_zz >>= 64;
  uint64_t high = NTL::conv<uint64_t>(x_zz);
  return absl::MakeUint128(high, low);
}
template <typename T>
T FromUint128(absl::uint128 x) {
  return T(x);
}
template <>
inline NTL::ZZ_p FromUint128<NTL::ZZ_p>(absl::uint128 x) {
  // Mask x before unpacking.
  x &= (absl::uint128(1) << NTL::NumBits(NTL::ZZ_p::modulus())) - 1;
  NTL::ZZ output_zz(0);
  // Use |= instead of assignments, as operators for NTL::ZZ are only defined on
  // signed integers, and implicit conversions screw up the result by
  // interpreting large uint64_t as negative numbers.
  output_zz |= absl::Uint128High64(x);
  output_zz <<= 64;
  output_zz |= absl::Uint128Low64(x);
  return NTL::conv<NTL::ZZ_p>(output_zz);
}

// Conversion functions between EMP and Span<T>
// These assume that the total size of the span is 128 bits,
// and thus matches the size of an EMP block
template <typename T>
emp::block SpanToEMPBlock(absl::Span<const T> v, T multiplier = T(1)) {
  absl::uint128 packed = 0;
  for (int i = v.size() - 1; i >= 0; --i) {
    packed += ToUint128<T>(v[i] * multiplier);
    if (i > 0) {
      packed <<= SizeOf<T>() * 8;
    }
  }
  emp::block out;
  out[0] = absl::Uint128Low64(packed);
  out[1] = absl::Uint128High64(packed);
  return out;
}

template <typename T>
vector<T> EMPBlockToVector(emp::block b) {
  int packing_factor = 16 / SizeOf<T>();  // emp::block are 128 bits
  std::vector<T> out(packing_factor);
  absl::uint128 packed = absl::MakeUint128(b[1], b[0]);
  for (int i = 0; i < packing_factor; ++i) {
    out[i] = FromUint128<T>(packed >> (i * SizeOf<T>() * 8));
  }
  return out;
}

template <typename T>
std::vector<emp::block> RunOTSender(
    absl::Span<const emp::block> deltas,
    emp::SHOTExtension<mpc_utils::CommChannelEMPAdapter> *ot) {
  std::vector<emp::block> opt0(deltas.size());
  auto correlator = [deltas](emp::block m0, uint64_t i) {
    emp::block delta = deltas[i];
    vector<T> d = EMPBlockToVector<T>(delta);
    vector<T> m = EMPBlockToVector<T>(m0);
    vector<T> res(d.size());
    for (int i = 0; i < static_cast<int>(d.size()); ++i) {
      res[i] = d[i] + m[i];
    }
    return SpanToEMPBlock<T>(res);
  };
  bool use_correlated_ot = !std::is_same<T, NTL::ZZ_p>::value;
  if (use_correlated_ot) {
    ot->send_cot_ft(opt0.data(), correlator, opt0.size());
  } else {
    // Generate random elements. Using NTL::random_vec_ZZ_p ensures that the
    // elements are indeed sampled uniformly from ZZ_p.
    int packing_factor = 16 / SizeOf<T>();
    for (int i = 0; i < static_cast<int>(opt0.size()); ++i) {
      NTL::Vec<NTL::ZZ_p> random_elements =
          NTL::random_vec_ZZ_p(packing_factor);
      opt0[i] = SpanToEMPBlock(
          absl::MakeConstSpan(random_elements.data(), packing_factor));
    }
    std::vector<emp::block> opt1(deltas.size());
    for (int i = 0; i < static_cast<int>(opt0.size()); ++i) {
      opt1[i] = correlator(opt0[i], i);
    }
    ot->send(opt0.data(), opt1.data(), opt0.size());
  }
  return opt0;
}

template <typename T>
inline std::vector<emp::block> RunOTReceiver(
    absl::Span<const bool> choices,
    emp::SHOTExtension<mpc_utils::CommChannelEMPAdapter> *ot) {
  std::vector<emp::block> result(choices.size());
  bool use_correlated_ot = !std::is_same<T, NTL::ZZ_p>::value;
  if (use_correlated_ot) {
    ot->recv_cot(result.data(), choices.data(), choices.size());
  } else {
    ot->recv(result.data(), choices.data(), choices.size());
  }
  return result;
}

}  // namespace gilboa_internal

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_INTERNAL_GILBOA_INTERNAL_H_
