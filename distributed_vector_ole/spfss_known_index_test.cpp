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

#include "distributed_vector_ole/spfss_known_index.h"
#include "distributed_vector_ole/internal/ntl_helpers.h"
#include <thread>
#include <vector>
#include "absl/memory/memory.h"
#include "boost/container/vector.hpp"
#include "gtest/gtest.h"
#include "mpc_utils/comm_channel.hpp"
#include "mpc_utils/status_matchers.h"
#include "mpc_utils/testing/comm_channel_test_helper.hpp"
#include "NTL/lzz_p.h"

namespace distributed_vector_ole {

namespace {

template<typename T>
class SPFSSKnownIndexTest : public ::testing::Test {
 protected:
  SPFSSKnownIndexTest() : helper_(false) {}
  void SetUp() {
    comm_channel *chan0 = helper_.GetChannel(0);
    comm_channel *chan1 = helper_.GetChannel(1);
    std::thread thread1([this, chan1] {
      ASSERT_OK_AND_ASSIGN(spfss_known_index_1_,
                           SPFSSKnownIndex::Create(chan1));
    });
    ASSERT_OK_AND_ASSIGN(spfss_known_index_0_, SPFSSKnownIndex::Create(chan0));
    thread1.join();
  }

  void TestVector(int size, int index, T val_share_0 = T(23),
                  T val_share_1 = T(42)) {
    T val = val_share_0;
    val += val_share_1;
    std::vector<T> output_0(size);
    std::vector<T> output_1(size);

    NTLContext<T> ntl_context;
    ntl_context.save();
    std::thread thread1([this, &output_0, val_share_0, &ntl_context] {
      // Re-initialize modulus for current thread.
      ntl_context.restore();
      EXPECT_TRUE(
          spfss_known_index_0_
              ->RunValueProvider(
                  val_share_0, absl::MakeSpan(output_0.data(), output_0.size()))
              .ok());
    });
    EXPECT_TRUE(
        spfss_known_index_1_
            ->RunIndexProvider(val_share_1, index,
                               absl::MakeSpan(output_1.data(), output_1.size()))
            .ok());
    thread1.join();

    for (int i = 0; i < size; i++) {
      T sum = output_0[i];
      sum += output_1[i];
      if (i != index) {
        EXPECT_EQ(sum, 0);
      } else {
        EXPECT_EQ(sum, val);
      }
    }
  }

  mpc_utils::testing::CommChannelTestHelper helper_;
  std::unique_ptr<SPFSSKnownIndex> spfss_known_index_0_;
  std::unique_ptr<SPFSSKnownIndex> spfss_known_index_1_;
};

using MyTypes = ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t,
                                 absl::uint128, NTL::ZZ_p, NTL::zz_p>;
TYPED_TEST_SUITE(SPFSSKnownIndexTest, MyTypes);

TYPED_TEST(SPFSSKnownIndexTest, TestSmallVectors) {
  for (int size = 1; size < 15; size++) {
    for (int index = 0; index < size; index++) {
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
          this->TestVector(size, index);
        }
      } else if (std::is_same<TypeParam, NTL::zz_p>::value) {
        for (int64_t modulus : {
            1125899906842597L,  // 2^50 - 27
            4294967291L,        // 2^32 - 5
            65521L,             // 2^16 - 15
            251L                // 2^8 - 5
        }) {
          NTL::zz_p::init(modulus);
          this->TestVector(size, index);
        }
      } else {
        this->TestVector(size, index);
      }
    }
  }
}

TEST(SPFSSKnownIndex, TestNullChannel) {
  auto status = SPFSSKnownIndex::Create(nullptr);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.status().message(), "`channel` must not be NULL");
}

}  // namespace

}  // namespace distributed_vector_ole
