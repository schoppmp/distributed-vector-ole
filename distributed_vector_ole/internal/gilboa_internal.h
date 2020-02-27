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

#ifndef DISTRIBUTED_VECTOR_OLE_INTERNAL_GILBOA_INTERNAL_H_
#define DISTRIBUTED_VECTOR_OLE_INTERNAL_GILBOA_INTERNAL_H_

// Helper functions for computing Gilboa products with arbitrary types.

#include "NTL/ZZ_p.h"
#include "NTL/lzz_p.h"
#include "NTL/vec_ZZ_p.h"
#include "NTL/vec_lzz_p.h"
#include "absl/meta/type_traits.h"
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "boost/container/vector.hpp"
#include "distributed_vector_ole/internal/ntl_helpers.h"
#include "distributed_vector_ole/internal/scalar_type_helpers.h"
#include "distributed_vector_ole/gf128.h"
#include "emp-ot/emp-ot.h"
#include "mpc_utils/comm_channel.hpp"
#include "mpc_utils/comm_channel_emp_adapter.hpp"
#include "openssl/rand.h"

namespace distributed_vector_ole {
namespace gilboa_internal {

// Returns the size (in bytes) of the type parameter or the argument.
template<typename T, typename std::enable_if<
    std::numeric_limits<T>::is_integer, int>::type = 0>
constexpr int SizeOf() {
  return sizeof(T);
}
template<typename T, typename std::enable_if<
    std::is_same<T, gf128>::value, int>::type = 0>
constexpr int SizeOf() {
  return sizeof(T);
}
template<typename T,
    typename std::enable_if<is_modular_integer<T>::value, int>::type = 0>
int SizeOf() {
  int num_bits = NTLNumBits<T>();
  return (num_bits + 7) / 8;
}

// Returns the k-th bit of x.
template<typename T, typename std::enable_if<
    std::numeric_limits<T>::is_integer, int>::type = 0>
bool GetBit(T x, int k) {
  return ((x >> k) & T(1)) != 0 ? true : false;
}
template<typename T, typename std::enable_if<
    std::is_same<T, gf128>::value, int>::type = 0>
bool GetBit(T x, int k) {
  return GetBit(ToUint128(x), k);
}
template<typename T,
    typename std::enable_if<is_modular_integer<T>::value, int>::type = 0>
bool GetBit(const T &x, int k) {
  return NTL::bit(NTL::rep(x), k);
}

// Returns the bit decomposition of a type T value
template<typename T>
boost::container::vector<bool> GetBits(const T &x) {
  boost::container::vector<bool> out(SizeOf<T>() * 8);
  for (int i = 0; i < static_cast<int>(out.size()); ++i) {
    out[i] = GetBit(x, i);
  }
  return out;
}

// Returns the j-th power of T(2).
template<typename T, typename std::enable_if<
    std::numeric_limits<T>::is_integer, int>::type = 0>
T PowerOf2(int j) {
  return T(1) << j;
}
template<typename T, typename std::enable_if<
    std::is_same<T, gf128>::value, int>::type = 0>
T PowerOf2(int j) {
  return T(absl::uint128(1) << j);
}
template<typename T,
    typename std::enable_if<is_modular_integer<T>::value, int>::type = 0>
T PowerOf2(int j) {
  return NTL::conv<T>(typename T::rep_type(1) << j);
}

// Conversion functions between EMP and Span<T>
// These assume that the total size of the span is 128 bits,
// and thus matches the size of an EMP block
template<typename T>
emp::block SpanToEMPBlock(absl::Span<const T> v, const T &multiplier = T(1)) {
  absl::uint128 packed = 0;
  T temp;  // Saves an allocation.
  for (int i = v.size() - 1; i >= 0; --i) {
    temp = v[i];
    temp *= multiplier;
    packed |= ToUint128<T>(temp);
    if (i > 0) {
      packed <<= SizeOf<T>() * 8;
    }
  }
  uint64_t low = absl::Uint128Low64(packed);
  uint64_t high = absl::Uint128High64(packed);
  return emp::makeBlock(high, low);
}

template<typename T>
void EMPBlockToSpan(emp::block b, absl::Span<T> out) {
  int packing_factor = 16 / SizeOf<T>();  // emp::block are 128 bits
  assert(packing_factor == static_cast<int>(out.size()));
  absl::uint128 packed = absl::MakeUint128(b[1], b[0]);
  for (int i = 0; i < packing_factor; ++i) {
    FromUint128<T>(packed >> (i * SizeOf<T>() * 8), &out[i]);
  }
}

template<typename T>
vector<T> EMPBlockToVector(emp::block b) {
  int packing_factor = 16 / SizeOf<T>();  // emp::block are 128 bits
  std::vector<T> out(packing_factor);
  EMPBlockToSpan(b, absl::MakeSpan(out));
  return out;
}

// Correlated OT for integers modulo 2^b.
template<typename T, typename boost::enable_if_c<
        std::numeric_limits<T>::is_integer ||
        std::is_same<T, gf128>::value
    , int>::type = 0>
std::vector<emp::block> RunOTSender(
    absl::Span<const emp::block> deltas,
    emp::SHOTExtension<mpc_utils::CommChannelEMPAdapter> *ot) {
  std::vector<emp::block> opt0(deltas.size());
  NTL::Vec<T> d, m, res;
  int packing_factor = 16 / SizeOf<T>();
  d.SetLength(packing_factor);
  m.SetLength(packing_factor);
  res.SetLength(packing_factor);
  auto correlator = [deltas, &d, &m, &res](emp::block m0, uint64_t i) {
    EMPBlockToSpan<T>(deltas[i], absl::MakeSpan(d.data(), d.length()));
    EMPBlockToSpan<T>(m0, absl::MakeSpan(m.data(), m.length()));
    for (int i = 0; i < static_cast<int>(d.length()); ++i) {
      res[i] = d[i] + m[i];
    }
    return SpanToEMPBlock<T>(absl::MakeSpan(res.data(), res.length()));
  };
  ot->send_cot_ft(opt0.data(), correlator, opt0.size());
  return opt0;
}

// 1-out-of-2-OT for NTL modular integers.
template<typename T,
    typename std::enable_if<is_modular_integer<T>::value, int>::type = 0>
std::vector<emp::block> RunOTSender(
    absl::Span<const emp::block> deltas,
    emp::SHOTExtension<mpc_utils::CommChannelEMPAdapter> *ot) {
  // Generate random elements. Using NTL::random ensures that the elements are
  // indeed sampled uniformly.
  std::vector<emp::block> opt0(deltas.size());
  std::vector<emp::block> opt1(deltas.size());
  NTL::Vec<T> d, m, res;
  NTL::Vec<T> random_elements;
  int packing_factor = 16 / SizeOf<T>();
  d.SetLength(packing_factor);
  m.SetLength(packing_factor);
  res.SetLength(packing_factor);
  random_elements.SetLength(packing_factor);
  for (int i = 0; i < static_cast<int>(opt0.size()); ++i) {
    NTL::random(random_elements, packing_factor);
    opt0[i] = SpanToEMPBlock(
        absl::MakeConstSpan(random_elements.data(), packing_factor));
    EMPBlockToSpan<T>(deltas[i], absl::MakeSpan(d.data(), d.length()));
    EMPBlockToSpan<T>(opt0[i], absl::MakeSpan(m.data(), m.length()));
    for (int i = 0; i < static_cast<int>(d.length()); ++i) {
      res[i] = d[i] + m[i];
    }
    opt1[i] = SpanToEMPBlock<T>(absl::MakeSpan(res.data(), res.length()));
  }
  ot->send(opt0.data(), opt1.data(), opt0.size());
  return opt0;
}

template<typename T>
inline std::vector<emp::block> RunOTReceiver(
    absl::Span<const bool> choices,
    emp::SHOTExtension<mpc_utils::CommChannelEMPAdapter> *ot) {
  std::vector<emp::block> result(choices.size());
  bool use_correlated_ot = !is_modular_integer<T>::value;
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
