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

#include "distributed_vector_ole/scalar_vector_gilboa_product.h"
#include <thread>
#include "NTL/ZZ_p.h"
#include "NTL/lzz_p.h"
#include "absl/memory/memory.h"
#include "gtest/gtest.h"
#include "mpc_utils/comm_channel.hpp"
#include "mpc_utils/status_matchers.h"
#include "mpc_utils/testing/comm_channel_test_helper.hpp"

namespace distributed_vector_ole {

namespace {

template <typename T>
class ScalarVectorGilboaProductTest : public ::testing::Test {
 protected:
  ScalarVectorGilboaProductTest() : helper_(false) {}
  void SetUp() {
    comm_channel *chan0 = helper_.GetChannel(0);
    comm_channel *chan1 = helper_.GetChannel(1);
    std::thread thread1([this, chan1] {
      ASSERT_OK_AND_ASSIGN(scalar_vector_gilboa_1_,
                           ScalarVectorGilboaProduct::Create(chan1));
    });
    ASSERT_OK_AND_ASSIGN(scalar_vector_gilboa_0_,
                         ScalarVectorGilboaProduct::Create(chan0));
    thread1.join();
  }

  // We need to run TestVector multiple times depending on the template
  // parameter. This is why it's implemented as a function instead of directly
  // in the test.
  void TestVector(int size) {
    std::vector<T> v(size);
    T x(-42);
    std::vector<T> result(size);
    for (int i = 0; i < size; ++i) {
      v[i] = i;
      result[i] = x * v[i];
    }
    std::vector<T> output_0;
    std::vector<T> output_1;
    // TODO: Implement a NTLContext<T> template to choose the correct context.
    NTLContext<T> ntl_context;
    ntl_context.save();
    std::thread thread1(
        [this, &x, &v, &output_0, &ntl_context] {
          ntl_context.restore();
          ASSERT_OK_AND_ASSIGN(
              output_0, scalar_vector_gilboa_0_->RunVectorProvider<T>(v));
        });
    ASSERT_OK_AND_ASSIGN(output_1,
                         scalar_vector_gilboa_1_->RunValueProvider(x, size));
    thread1.join();

    for (int i = 0; i < static_cast<int>(output_0.size()); i++) {
      EXPECT_EQ(T(output_0[i] + output_1[i]), result[i])
          << "\ti = " << i << std::endl;
    }
    helper_.GetChannel(0)->flush();
    helper_.GetChannel(1)->flush();
  }

  mpc_utils::testing::CommChannelTestHelper helper_;
  std::unique_ptr<ScalarVectorGilboaProduct> scalar_vector_gilboa_0_;
  std::unique_ptr<ScalarVectorGilboaProduct> scalar_vector_gilboa_1_;
};

using MyTypes = ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t,
                                 absl::uint128, NTL::ZZ_p, NTL::zz_p>;
TYPED_TEST_SUITE(ScalarVectorGilboaProductTest, MyTypes);

TYPED_TEST(ScalarVectorGilboaProductTest, TestSmallVectors) {
  for (int size = 0; size < 20; size++) {
    if (std::is_same<TypeParam, NTL::ZZ_p>::value) {
      for (const auto &modulus : {
               "340282366920938463463374607431768211456",  // 2^128 (the largest
                                                           // modulus we
                                                           // support)
               // Prime moduli:
               "340282366920938463463374607431768211297",  // 2^128 - 159
               "18446744073709551557",                     // 2^64 - 59
               "4294967291",                               // 2^32 - 5
               "65521",                                    // 2^16 - 15
               "251"                                       // 2^8 - 5
           }) {
        NTL::ZZ_p::init(NTL::conv<NTL::ZZ>(modulus));
        this->TestVector(size);
      }
    } else if (std::is_same<TypeParam, NTL::zz_p>::value) {
      for (int64_t modulus : {
               1125899906842597L,  // 2^50 - 27
               4294967291L,        // 2^32 - 5
               65521L,             // 2^16 - 15
               251L                // 2^8 - 5
           }) {
        NTL::zz_p::init(modulus);
        this->TestVector(size);
      }
    } else {
      this->TestVector(size);
    }
  }
}

TYPED_TEST(ScalarVectorGilboaProductTest, TestConversions) {
  std::vector<TypeParam> values = {
      TypeParam(0), TypeParam(1), TypeParam(23), TypeParam(42), TypeParam(-42),
  };
  NTL::ZZ_p::init(NTL::conv<NTL::ZZ>("4294967291"));
  NTL::zz_p::init(NTL::conv<int64_t>("4294967291"));
  for (const auto &value : values) {
    for (const auto &multiplier : values) {
      std::vector<TypeParam> v(16 / gilboa_internal::SizeOf<TypeParam>(),
                               value);
      emp::block b = gilboa_internal::SpanToEMPBlock<TypeParam>(v, multiplier);
      std::vector<TypeParam> v2 =
          gilboa_internal::EMPBlockToVector<TypeParam>(b);
      for (int i = 0; i < static_cast<int>(v.size()); i++) {
        EXPECT_EQ(TypeParam(v[i] * multiplier), v2[i])
            << "v[i] = " << v[i] << ", multiplier = " << multiplier
            << ", b[0] = " << b[0] << ", b[1] = " << b[1];
      }
    }
  }
}

TYPED_TEST(ScalarVectorGilboaProductTest, TestNegativeLength) {
  auto status =
      this->scalar_vector_gilboa_0_->RunValueProvider(TypeParam(0), -1);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.status().message(), "`y_len` must not be negative");
}

using ScalarVectorGilboaProductNTLOnlyTest =
    ScalarVectorGilboaProductTest<NTL::ZZ_p>;
TEST_F(ScalarVectorGilboaProductNTLOnlyTest, TestModulusTooLarge) {
  NTL::ZZ_p::init((NTL::ZZ(1) << 128) + 1);
  auto status =
      this->scalar_vector_gilboa_0_->RunValueProvider(NTL::ZZ_p(0), 0);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.status().message(), "Integers may be at most 16 bytes long");
  status = this->scalar_vector_gilboa_0_->RunVectorProvider<NTL::ZZ_p>({});
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.status().message(), "Integers may be at most 16 bytes long");
}

TYPED_TEST(ScalarVectorGilboaProductTest, TestDifferentLengths) {
  std::vector<TypeParam> output(2);
  auto status = this->scalar_vector_gilboa_0_->RunVectorProvider(
      {TypeParam(0)}, absl::MakeSpan(output));
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.message(), "`y` and `output` must have the same size");
}

TEST(ScalarVectorGilboaProduct, TestNullChannel) {
  auto status = ScalarVectorGilboaProduct::Create(nullptr);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.status().message(), "`channel` must not be NULL");
}

}  // namespace
}  // namespace distributed_vector_ole
