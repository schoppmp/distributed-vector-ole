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

#ifndef DISTRIBUTED_VECTOR_OLE_INTERNAL_IS_MODULAR_INTEGER_H_
#define DISTRIBUTED_VECTOR_OLE_INTERNAL_IS_MODULAR_INTEGER_H_

// Helpers to generically work with multiple NTL types. Currently includes:
// - A type trait `is_modular_integer<T>` for SFINAE checks
// - int NTLNumBits<T>() to get the size of the current modulus
// - NTLTemp<T>() to get a temporary variable

#include <type_traits>
#include "NTL/ZZ_p.h"
#include "NTL/lzz_p.h"
#include "absl/meta/type_traits.h"
#include "absl/numeric/int128.h"

namespace distributed_vector_ole {

// Type trait to check if a type has a `rep_type` declared, which holds for
// NTL's modular integers.
template <typename T, typename = void>
struct is_modular_integer : std::false_type {};
template <typename T>
struct is_modular_integer<T, absl::void_t<typename T::rep_type>>
    : std::true_type {};

// Templated struct for passing moduli across threads. Does nothing if T is not
// a modular type.
template <typename T, typename = int>
struct NTLContext {
  void save() {}
  void restore() {}
};
template <typename T>
struct NTLContext<
    T, typename std::enable_if<is_modular_integer<T>::value, int>::type>
    : T::context_type {};

// Returns the number of bits of the currently installed modulus.
template <typename T,
          typename std::enable_if<is_modular_integer<T>::value, int>::type = 0>
int NTLNumBits() {
  return NTL::NumBits(T::modulus() - 1);
}
// Specialization with cached modulus. Currently only for ZZ_p.
template <typename T>
int NTLNumBitsCached();
template <>
inline int NTLNumBits<NTL::ZZ_p>() {
  return NTLNumBitsCached<NTL::ZZ_p>();
}

// Returns a pointer to a thread_local temporary NTL:ZZ. Avoids memory
// allocations.
template <typename T>
T* NTLTemp();

// Conversions between absl::uint128 and various types.
template <typename T, typename std::enable_if<
    std::numeric_limits<T>::is_integer, int>::type = 0>
absl::uint128 ToUint128(const T& x) {
  return x;
}
template <typename T,
    typename std::enable_if<is_modular_integer<T>::value, int>::type = 0>
absl::uint128 ToUint128(const T& x) {
  uint64_t low = NTL::conv<uint64_t>(NTL::rep(x));
  uint64_t high = NTL::conv<uint64_t>(NTL::rep(x) >> 64);
  return absl::MakeUint128(high, low);
}
template<>
inline absl::uint128 ToUint128(const NTL::zz_p& x) {
  return NTL::conv<uint64_t>(NTL::rep(x));
}

template <typename T>
void FromUint128(absl::uint128 x, T* out) {
  *out = T(x);
}
template <>
inline void FromUint128<NTL::zz_p>(absl::uint128 x, NTL::zz_p *out) {
  if (NTLNumBits<NTL::zz_p>() < 128) {
    x &= ((absl::uint128(1) << NTLNumBits<NTL::zz_p>()) - 1);
  }
  NTL::conv(*out, absl::Uint128Low64(x));
}
template<>
inline void FromUint128(absl::uint128 x, NTL::ZZ_p* out) {
  if (NTLNumBits<NTL::ZZ_p>() < 128) {
    x &= ((absl::uint128(1) << NTLNumBits<NTL::ZZ_p>()) - 1);
  }
  NTL::conv(*NTLTemp<NTL::ZZ>(), absl::Uint128High64(x));
  *NTLTemp<NTL::ZZ>() <<= 64;
  *NTLTemp<NTL::ZZ>() |= NTL::conv<NTL::ZZ>(absl::Uint128Low64(x));
  NTL::conv(*out, *NTLTemp<NTL::ZZ>());
}

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_INTERNAL_IS_MODULAR_INTEGER_H_
