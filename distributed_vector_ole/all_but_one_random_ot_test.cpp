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

#include "distributed_vector_ole/all_but_one_random_ot.h"

#include <thread>

#include "NTL/lzz_p.h"
#include "absl/memory/memory.h"
#include "distributed_vector_ole/internal/ntl_helpers.h"
#include "gtest/gtest.h"
#include "mpc_utils/comm_channel.hpp"
#include "mpc_utils/status_matchers.h"
#include "mpc_utils/testing/comm_channel_test_helper.hpp"

namespace distributed_vector_ole {

namespace {

template <typename T>
class AllButOneRandomOTTest : public ::testing::Test {
 protected:
  AllButOneRandomOTTest() : helper_(false) {}
  void SetUp() {
    comm_channel *chan0 = helper_.GetChannel(0);
    comm_channel *chan1 = helper_.GetChannel(1);
    std::thread thread1([this, chan1] {
      ASSERT_OK_AND_ASSIGN(all_but_one_rot_1_,
                           AllButOneRandomOT::Create(chan1));
    });
    ASSERT_OK_AND_ASSIGN(all_but_one_rot_0_, AllButOneRandomOT::Create(chan0));
    thread1.join();
  }

  void TestVector(int size, int index) {
    std::vector<T> output_0(size);
    std::vector<T> output_1(size);

    NTLContext<T> ntl_context;
    ntl_context.save();
    std::thread thread1([this, &output_0, &ntl_context] {
      // Re-initialize modulus for current thread.
      ntl_context.restore();
      EXPECT_TRUE(
          all_but_one_rot_0_
              ->RunSender(absl::MakeSpan(output_0.data(), output_0.size()))
              .ok());
    });
    EXPECT_TRUE(all_but_one_rot_1_
                    ->RunReceiver(
                        index, absl::MakeSpan(output_1.data(), output_1.size()))
                    .ok());
    thread1.join();

    for (int i = 0; i < size; i++) {
      if (i != index) {
        EXPECT_EQ(output_0[i], output_1[i]);
      }
    }
  }

  mpc_utils::testing::CommChannelTestHelper helper_;
  std::unique_ptr<AllButOneRandomOT> all_but_one_rot_0_;
  std::unique_ptr<AllButOneRandomOT> all_but_one_rot_1_;
};

using MyTypes = ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t,
                                 absl::uint128, NTL::ZZ_p, NTL::zz_p>;
TYPED_TEST_SUITE(AllButOneRandomOTTest, MyTypes);

TYPED_TEST(AllButOneRandomOTTest, TestSmallVectors) {
  for (int size = 1; size < 15; size++) {
    for (int index = 0; index < size; index++) {
      if (std::is_same<TypeParam, NTL::ZZ_p>::value) {
        for (const auto &modulus : {
                 "340282366920938463463374607431768211456",  // 2^128 (the
                                                             // largest modulus
                                                             // we support)
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

TYPED_TEST(AllButOneRandomOTTest, TestEmpty) { this->TestVector(0, 0); }

TYPED_TEST(AllButOneRandomOTTest, TestDifferentSizesReceiverMulti) {
  std::vector<int> output = {1};
  std::vector<absl::Span<int>> outputs = {absl::MakeSpan(output)};
  auto status = this->all_but_one_rot_1_->RunReceiverBatched(
      absl::Span<int64_t>(), absl::MakeSpan(outputs));
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.message(),
            "`indices` and `outputs` must have the same size");
}

TYPED_TEST(AllButOneRandomOTTest, TestIndexNegative) {
  std::vector<int> dummy(100);
  auto status = this->all_but_one_rot_1_->RunReceiver(
      -1, absl::MakeSpan(dummy.data(), dummy.size() + 1));
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.message(), "`index` out of range");
}

TYPED_TEST(AllButOneRandomOTTest, TestIndexTooLarge) {
  std::vector<int> dummy(100);
  auto status = this->all_but_one_rot_1_->RunReceiver(
      dummy.size(), absl::MakeSpan(dummy.data(), dummy.size()));
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.message(), "`index` out of range");
}

TYPED_TEST(AllButOneRandomOTTest, TestNegativeStatisticalSecurity) {
  auto status = AllButOneRandomOT::Create(this->helper_.GetChannel(0), -1.0);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.status().message(),
            "`statistical_security` must not be negative.");
}

TEST(AllButOneRandomOT, TestNullChannel) {
  auto status = AllButOneRandomOT::Create(nullptr);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.status().message(), "`channel` must not be NULL");
}

}  // namespace

}  // namespace distributed_vector_ole
