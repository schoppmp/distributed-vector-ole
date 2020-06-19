// Based on the following GF128 implementation in libiop:
// https://github.com/scipr-lab/libiop/blob/45d5f219a784a88d6fc4557b2bf69107264a24d1/libiop/algebra/fields/gf128.hpp
//
// Copyright 2019 the libiop authors
// (https://github.com/scipr-lab/libiop/blob/master/AUTHORS), licensed under the
// MIT license.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef DISTRIBUTED_VECTOR_OLE_GF128_H_
#define DISTRIBUTED_VECTOR_OLE_GF128_H_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <vector>

#include "absl/numeric/int128.h"
#include "boost/serialization/base_object.hpp"

namespace distributed_vector_ole {

// gf128 implements the field GF(2)/(x^128 + x^7 + x^2 + x + 1).
// Elements are represented internally with two uint64s.
class gf128 {
  friend class boost::serialization::access;

 public:
  // x^128 + x^7 + x^2 + x + 1
  static const constexpr uint64_t modulus_ = 0b10000111;
  static const constexpr uint64_t num_bits = 128;

  explicit gf128();
  // We need a constructor that only initializes the low half of value_ to
  // be able to do gf128(0) and gf128(1).
  explicit gf128(const uint64_t value_low);
  explicit gf128(const uint64_t value_high, const uint64_t value_low);
  explicit gf128(const absl::uint128);
  // Returns the constituent bits in 64 bit words, in little-endian order.
  std::vector<uint64_t> as_words() const;

  // Conversion to abls::uint128.
  explicit operator absl::uint128() const;

  gf128 &operator+=(const gf128 &other);
  gf128 &operator-=(const gf128 &other);
  gf128 &operator*=(const gf128 &other);
  void square();

  gf128 operator+(const gf128 &other) const;
  gf128 operator-(const gf128 &other) const;
  gf128 operator-() const;
  gf128 operator*(const gf128 &other) const;
  gf128 squared() const;

  gf128 inverse() const;

  void randomize();

  bool operator==(const gf128 &other) const;
  bool operator!=(const gf128 &other) const;

  bool is_zero() const;

  static gf128 random_element();

  static gf128 zero();
  static gf128 one();
  static gf128 multiplicative_generator;  // generator of gf128^*.

  static std::size_t extension_degree() { return 128; }

  // Support for boost::serialization.
  template <class Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar &value_;
  }

  // Support for absl::Hash.
  template <typename H>
  friend H AbslHashValue(H h, const gf128 &x) {
    return H::combine(std::move(h), x.value_[1], x.value_[0]);
  }

 private:
  uint64_t value_[2];  // Little endian.
};

}  // namespace distributed_vector_ole

// Output operator for printing.
std::ostream &operator<<(std::ostream &os,
                         const distributed_vector_ole::gf128 x);

#endif  // DISTRIBUTED_VECTOR_OLE_GF128_H_
