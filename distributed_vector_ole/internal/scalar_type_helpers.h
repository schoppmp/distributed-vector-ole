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

#ifndef DISTRIBUTED_VECTOR_OLE_DISTRIBUTED_VECTOR_OLE_INTERNAL_SCALAR_TYPE_HELPERS_H_
#define DISTRIBUTED_VECTOR_OLE_DISTRIBUTED_VECTOR_OLE_INTERNAL_SCALAR_TYPE_HELPERS_H_

#include "distributed_vector_ole/gf128.h"
#include "absl/numeric/int128.h"
#include "NTL/ZZ_p.h"
#include "NTL/lzz_p.h"

namespace distributed_vector_ole {

// Conversions between absl::uint128 and various types.
template<typename T, typename std::enable_if<
    std::numeric_limits<T>::is_integer, int>::type = 0>
absl::uint128 ToUint128(const T &x) {
  return x;
}
template<typename T, typename std::enable_if<
    std::is_same<T, gf128>::value, int>::type = 0>
absl::uint128 ToUint128(const T &x) {
  return static_cast<absl::uint128>(x);
}
template<typename T,
    typename std::enable_if<is_modular_integer<T>::value, int>::type = 0>
absl::uint128 ToUint128(const T &x) {
  uint64_t low = NTL::conv<uint64_t>(NTL::rep(x));
  uint64_t high = NTL::conv<uint64_t>(NTL::rep(x) >> 64);
  return absl::MakeUint128(high, low);
}
template<>
inline absl::uint128 ToUint128(const NTL::zz_p &x) {
  return NTL::conv<uint64_t>(NTL::rep(x));
}

template<typename T>
void FromUint128(absl::uint128 x, T *out) {
  *out = T(x);
}
template<>
inline void FromUint128(absl::uint128 x, gf128 *out) {
  *out = gf128(x);
}
template<>
inline void FromUint128<NTL::zz_p>(absl::uint128 x, NTL::zz_p *out) {
  if (NTLNumBits<NTL::zz_p>() < 128) {
    x &= ((absl::uint128(1) << NTLNumBits<NTL::zz_p>()) - 1);
  }
  NTL::conv(*out, absl::Uint128Low64(x));
}
template<>
inline void FromUint128(absl::uint128 x, NTL::ZZ_p *out) {
  if (NTLNumBits<NTL::ZZ_p>() < 128) {
    x &= ((absl::uint128(1) << NTLNumBits<NTL::ZZ_p>()) - 1);
  }
  NTL::conv(*NTLTemp<NTL::ZZ>(), absl::Uint128High64(x));
  *NTLTemp<NTL::ZZ>() <<= 64;
  *NTLTemp<NTL::ZZ>() |= NTL::conv<NTL::ZZ>(absl::Uint128Low64(x));
  NTL::conv(*out, *NTLTemp<NTL::ZZ>());
}

}  // namespace disctributed_vector_ole

#endif  //DISTRIBUTED_VECTOR_OLE_DISTRIBUTED_VECTOR_OLE_INTERNAL_SCALAR_TYPE_HELPERS_H_
