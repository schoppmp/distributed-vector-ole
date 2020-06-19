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

#ifndef DISTRIBUTED_VECTOR_OLE_INTERNAL_SCALAR_HELPERS_H_
#define DISTRIBUTED_VECTOR_OLE_INTERNAL_SCALAR_HELPERS_H_

// This file declares helper functions that need to be implemented by scalar
// types to be used with the Vector OLE generator. To add support for other
// types, add a matching template specialization of ScalarHelper.

#include "NTL/ZZ_p.h"
#include "NTL/lzz_p.h"
#include "NTL/vec_ZZ_p.h"
#include "NTL/vec_lzz_p.h"
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "distributed_vector_ole/gf128.h"
#include "distributed_vector_ole/internal/ntl_helpers.h"
#include "openssl/rand.h"

namespace distributed_vector_ole {

// Defines helper functions that need to be implemented by all scalar types we
// want to use. Specialize this template for any new scalar type.
template <typename T, typename = int>
struct ScalarHelper {
  // Converts to an absl::uint128, truncating in case of overflow.
  static absl::uint128 ToUint128(const T &x);
  // Converts an absl::uint128 to T, truncating in case of overflow.
  static T FromUint128(absl::uint128 x);
  // Returns the size of a T in bytes. Refers to the number of bytes needed to
  // represent the number described by a T, not necessarily the size of an
  // instance of T in memory.
  static int SizeOf();
  // Returns the k-th bit of x.
  static bool GetBit(T x, int k);
  // Returns a T with the k-th bit set.
  static T SetBit(int k);
  // Sets all elements of `output` to uniformly random elements of type T.
  static void Randomize(absl::Span<T> output);
  // Returns true if hash values of size hash_bits can be directly mapped to
  // instances of T with probability at least 1 - 2^-statistical_security.
  static bool CanBeHashedInto(double statistical_security = 40,
                              int hash_bits = 128);
};

// Integers, including absl::uint128.
template <typename T>
struct ScalarHelper<
    T, typename std::enable_if<std::numeric_limits<T>::is_integer, int>::type> {
  static absl::uint128 ToUint128(const T &x) { return absl::uint128(x); }
  static T FromUint128(absl::uint128 x) { return T(x); }
  static constexpr int SizeOf() { return sizeof(T); }
  static bool GetBit(T x, int k) { return ((x >> k) & T(1)) != 0; }
  static T SetBit(int k) { return T(1) << k; }
  static constexpr bool CanBeHashedInto(double statistical_security = 40,
                                        int hash_bits = 128) {
    return hash_bits >= SizeOf() * 8;
  }
  static void Randomize(absl::Span<T> output) {
    RAND_bytes(reinterpret_cast<uint8_t *>(output.data()),
               output.size() * sizeof(T));
  }
};

// GF128 elements.
template <>
struct ScalarHelper<gf128> {
  static absl::uint128 ToUint128(const gf128 &x) {
    return static_cast<absl::uint128>(x);
  }
  static gf128 FromUint128(absl::uint128 x) { return gf128(x); }
  static constexpr int SizeOf() { return sizeof(gf128); }
  static bool GetBit(gf128 x, int k) {
    return ScalarHelper<absl::uint128>::GetBit(static_cast<absl::uint128>(x),
                                               k);
  }
  static gf128 SetBit(int k) { return gf128(absl::uint128(1) << k); }
  static constexpr bool CanBeHashedInto(double statistical_security = 40,
                                        int hash_bits = 128) {
    return hash_bits >= SizeOf() * 8;
  }
  static void Randomize(absl::Span<gf128> output) {
    RAND_bytes(reinterpret_cast<uint8_t *>(output.data()),
               output.size() * sizeof(gf128));
  }
};

// NTL modular integers.
template <typename T, typename = int>
struct ScalarHelperImpl {};
// NTL::ZZ_p.
template <>
struct ScalarHelperImpl<NTL::ZZ_p> {
  static absl::uint128 ToUint128(const NTL::ZZ_p &x) {
    uint64_t low = NTL::conv<uint64_t>(NTL::rep(x));
    uint64_t high = NTL::conv<uint64_t>(NTL::rep(x) >> 64);
    return absl::MakeUint128(high, low);
  }
  static NTL::ZZ_p FromUint128(absl::uint128 x) {
    typename NTL::ZZ_p::rep_type *temp =
        NTLTemp<typename NTL::ZZ_p::rep_type>();
    *temp = typename NTL::ZZ_p::rep_type(0);
    NTL::conv(*temp, absl::Uint128High64(x));
    *temp <<= 64;
    *temp |= NTL::conv<typename NTL::ZZ_p::rep_type>(absl::Uint128Low64(x));
    return NTL::conv<NTL::ZZ_p>(*temp);
  }
  static bool CanBeHashedInto(double statistical_security, int hash_bits) {
    // Check that when reducing an absl::uint128 modulo T::modulus(), the result
    // is nearly uniform, i.e., the probability for any set of results is at
    // most 2^-statistical_security higher than in a uniform distribution.
    uint64_t modulus_low = NTL::conv<uint64_t>(NTL::ZZ_p::modulus());
    uint64_t modulus_high = NTL::conv<uint64_t>(NTL::ZZ_p::modulus() >> 64);
    absl::uint128 modulus = absl::MakeUint128(modulus_high, modulus_low);
    if (modulus == 0) {  // Modulus is a multiple of 2**128
      return true;
    }
    absl::uint128 max_value = absl::Uint128Max();
    if (hash_bits < 128) {
      max_value /= (absl::uint128(1) << hash_bits);
    }
    absl::uint128 rem = ((max_value % modulus) + 1) % modulus;
    return hash_bits - std::log2(double(rem)) > statistical_security;
  }
  static void Randomize(absl::Span<NTL::ZZ_p> output) {
    NTL::Vec<NTL::ZZ_p> ntl_vec = NTL::random_vec_ZZ_p(output.size());
    for (int64_t i = 0; i < static_cast<int64_t>(output.size()); i++) {
      output[i] = std::move(ntl_vec[i]);
    }
  }
};
// NTL::zz_p.
template <>
struct ScalarHelperImpl<NTL::zz_p> {
  static absl::uint128 ToUint128(const NTL::zz_p &x) {
    return static_cast<absl::uint128>(NTL::rep(x));
  }
  static NTL::zz_p FromUint128(absl::uint128 x) {
    return NTL::conv<NTL::zz_p>(absl::Uint128Low64(x % NTL::zz_p::modulus()));
  }
  static bool CanBeHashedInto(double statistical_security, int hash_bits) {
    long modulus = NTL::zz_p::modulus();  // NTL::zz_p::modulus() is guaranteed
                                          // to fit in a long.
    absl::uint128 max_value = absl::Uint128Max();
    if (hash_bits < 128) {
      max_value /= (absl::uint128(1) << hash_bits);
    }
    absl::uint128 rem = ((max_value % modulus) + 1) % modulus;
    return hash_bits - std::log2(double(rem)) > statistical_security;
  }
  static void Randomize(absl::Span<NTL::zz_p> output) {
    NTL::Vec<NTL::zz_p> ntl_vec = NTL::random_vec_zz_p(output.size());
    for (int64_t i = 0; i < static_cast<int64_t>(output.size()); i++) {
      output[i] = ntl_vec[i];
    }
  }
};

// Common to all modular integers.
template <typename T>
struct ScalarHelper<
    T, typename std::enable_if<is_modular_integer<T>::value, int>::type> {
  static absl::uint128 ToUint128(const T &x) {
    return ScalarHelperImpl<T>::ToUint128(x);
  }
  static T FromUint128(absl::uint128 x) {
    return ScalarHelperImpl<T>::FromUint128(x);
  }
  static int SizeOf() { return (NTLNumBits<T>() + 7) / 8; }
  static bool GetBit(const T &x, int k) { return NTL::bit(NTL::rep(x), k); }
  static T SetBit(int k) { return NTL::conv<T>(typename T::rep_type(1) << k); }
  static bool CanBeHashedInto(double statistical_security = 40,
                              int hash_bits = 128) {
    return ScalarHelperImpl<T>::CanBeHashedInto(statistical_security,
                                                hash_bits);
  }
  static void Randomize(absl::Span<T> output) {
    ScalarHelperImpl<T>::Randomize(output);
  }
};

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_INTERNAL_SCALAR_HELPERS_H_
