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

#include "distributed_vector_ole/gf128.h"

#include "openssl/rand.h"

#ifdef USE_ASM
#include <emmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>
#endif

namespace distributed_vector_ole {

const uint64_t gf128::modulus_;
gf128 gf128::multiplicative_generator = gf128(2);

gf128::gf128() : value_{0, 0} {}

gf128::gf128(const uint64_t value_low) : value_{value_low, 0} {}

gf128::gf128(const uint64_t value_high, const uint64_t value_low)
    : value_{value_low, value_high} {}

gf128::gf128(const absl::uint128 value)
    : value_{absl::Uint128Low64(value), absl::Uint128High64(value)} {}

std::vector<uint64_t> gf128::as_words() const {
  return std::vector<uint64_t>({this->value_[0], this->value_[1]});
}

gf128::operator absl::uint128() const {
  return absl::MakeUint128(value_[1], value_[0]);
}

gf128 &gf128::operator+=(const gf128 &other) {
  this->value_[0] ^= other.value_[0];
  this->value_[1] ^= other.value_[1];
  return (*this);
}

gf128 &gf128::operator-=(const gf128 &other) {
  this->value_[0] ^= other.value_[0];
  this->value_[1] ^= other.value_[1];
  return (*this);
}

#ifdef USE_ASM
__attribute__((target("pclmul,sse2")))
#endif
gf128 &
gf128::operator*=(const gf128 &other) {
  /* Does not require *this and other to be different, and therefore
     also works for squaring, implemented below. */
#ifdef USE_ASM
  /* load the two operands and the modulus into 128-bit registers */
  const __m128i a = _mm_loadu_si128((const __m128i *)&(this->value_));
  const __m128i b = _mm_loadu_si128((const __m128i *)&(other.value_));
  const __m128i modulus = _mm_loadl_epi64((const __m128i *)&(this->modulus_));

  /* compute the 256-bit result of a * b with the 64x64-bit multiplication
     intrinsic */
  __m128i mul256_high = _mm_clmulepi64_si128(a, b, 0x11); /* high of both */
  __m128i mul256_low = _mm_clmulepi64_si128(a, b, 0x00);  /* low of both */

  __m128i mul256_mid1 =
      _mm_clmulepi64_si128(a, b, 0x01); /* low of a, high of b */
  __m128i mul256_mid2 =
      _mm_clmulepi64_si128(a, b, 0x10); /* high of a, low of b */

  /* Add the 4 terms together */
  __m128i mul256_mid = _mm_xor_si128(mul256_mid1, mul256_mid2);
  /* lower 64 bits of mid don't intersect with high, and upper 64 bits don't
   * intersect with low */
  mul256_high = _mm_xor_si128(mul256_high, _mm_srli_si128(mul256_mid, 8));
  mul256_low = _mm_xor_si128(mul256_low, _mm_slli_si128(mul256_mid, 8));

  /* done computing mul256_low and mul256_high, time to reduce */

  /* reduce w.r.t. high half of mul256_high */
  __m128i tmp = _mm_clmulepi64_si128(mul256_high, modulus, 0x01);
  mul256_low = _mm_xor_si128(mul256_low, _mm_slli_si128(tmp, 8));
  mul256_high = _mm_xor_si128(mul256_high, _mm_srli_si128(tmp, 8));

  /* reduce w.r.t. low half of mul256_high */
  tmp = _mm_clmulepi64_si128(mul256_high, modulus, 0x00);
  mul256_low = _mm_xor_si128(mul256_low, tmp);

  _mm_storeu_si128((__m128i *)this->value_, mul256_low);

  return (*this);
#else
  /* Slow, but straight-forward */
  uint64_t shifted[2] = {this->value_[0], this->value_[1]};
  uint64_t result[2] = {0, 0};

  for (int64_t i = 0; i < 2; ++i) {
    for (int64_t j = 0; j < 64; ++j) {
      if (other.value_[i] & (1ull << j)) {
        result[0] ^= shifted[0];
        result[1] ^= shifted[1];
      }

      if (shifted[1] & (1ull << 63)) {
        shifted[1] = (shifted[1] << 1) | (shifted[0] >> 63);
        shifted[0] = (shifted[0] << 1) ^ this->modulus_;
      } else {
        shifted[1] = (shifted[1] << 1) | (shifted[0] >> 63);
        shifted[0] = shifted[0] << 1;
      }
    }
  }

  this->value_[0] = result[0];
  this->value_[1] = result[1];

  return (*this);
#endif
}

void gf128::square() { this->operator*=(*this); }

gf128 gf128::operator+(const gf128 &other) const {
  gf128 result(*this);
  return (result += (other));
}

gf128 gf128::operator-(const gf128 &other) const {
  gf128 result(*this);
  return (result -= (other));
}

gf128 gf128::operator-() const {
  return gf128(this->value_[1], this->value_[0]);
}

gf128 gf128::operator*(const gf128 &other) const {
  gf128 result(*this);
  return (result *= (other));
}

gf128 gf128::squared() const {
  gf128 result(*this);
  result.square();
  return result;
}

/* calculate el^{-1} as el^{2^{128}-2}. the addition chain below
   requires 142 mul/sqr operations total. */
gf128 gf128::inverse() const {
  gf128 a(*this);

  gf128 result(0);
  for (int64_t i = 0; i <= 6; ++i) {
    /* entering the loop a = el^{2^{2^i}-1} */
    gf128 b = a;
    for (int64_t j = 0; j < (1 << i); ++j) {
      b.square();
    }
    /* after the loop b = a^{2^i} = el^{2^{2^i}*(2^{2^i}-1)} */
    a *= b;
    /* now a = el^{2^{2^{i+1}}-1} */

    if (i == 0) {
      result = b;
    } else {
      result *= b;
    }
  }
  /* now result = el^{2^128-2} */
  return result;
}

void gf128::randomize() {
  RAND_bytes(reinterpret_cast<unsigned char *>(&this->value_), 128 / 8);
}

bool gf128::operator==(const gf128 &other) const {
  return (this->value_[0] == other.value_[0]) &&
         ((this->value_[1] == other.value_[1]));
}

bool gf128::operator!=(const gf128 &other) const {
  return !(this->operator==(other));
}

bool gf128::is_zero() const {
  return (this->value_[0] == 0) && (this->value_[1] == 0);
}

gf128 gf128::random_element() {
  gf128 result;
  result.randomize();
  return result;
}

gf128 gf128::zero() { return gf128(0); }

gf128 gf128::one() { return gf128(1); }

}  // namespace distributed_vector_ole

std::ostream &operator<<(std::ostream &os,
                         const distributed_vector_ole::gf128 x) {
  os << "gf128(" << static_cast<absl::uint128>(x) << ")";
  return os;
}
