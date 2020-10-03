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

#include "distributed_vector_ole/distributed_vector_ole.h"

#include <thread>

#include "absl/memory/memory.h"
#include "distributed_vector_ole/gf128.h"
#include "gtest/gtest.h"
#include "mpc_utils/comm_channel.hpp"
#include "mpc_utils/status_matchers.h"
#include "mpc_utils/testing/comm_channel_test_helper.hpp"

namespace distributed_vector_ole {
namespace {

template <typename T>
class DistributedVectorOLETest : public ::testing::Test {
 protected:
  DistributedVectorOLETest() : helper_(false) {}
  void SetUp() {
    emp::initialize_relic();
    comm_channel *chan0 = helper_.GetChannel(0);
    comm_channel *chan1 = helper_.GetChannel(1);
    std::thread thread1([this, chan1] {
      ASSERT_OK_AND_ASSIGN(vole_1_, DistributedVectorOLE<T>::Create(chan1));
    });
    ASSERT_OK_AND_ASSIGN(vole_0_, DistributedVectorOLE<T>::Create(chan0));
    thread1.join();
  }

  void Precompute(int size) {
    T delta(23);
    NTLContext<T> ntl_context;
    ntl_context.save();
    std::thread thread1([this, size, &ntl_context] {
      ntl_context.restore();
      ASSERT_TRUE(vole_0_->PrecomputeSender(size).ok());
    });
    ASSERT_TRUE(vole_1_->PrecomputeReceiver(size, delta).ok());
    thread1.join();
  }

  void TestVector(int size) {
    typename DistributedVectorOLE<T>::SenderResult sender_result;
    typename DistributedVectorOLE<T>::ReceiverResult receiver_result;
    NTLContext<T> ntl_context;
    ntl_context.save();
    std::thread thread1([this, size, &ntl_context, &sender_result] {
      ntl_context.restore();
      ASSERT_OK_AND_ASSIGN(sender_result, vole_0_->RunSender(size));
    });
    ASSERT_OK_AND_ASSIGN(receiver_result, vole_1_->RunReceiver(size));
    thread1.join();
    Vector<T> w2 = sender_result.u * receiver_result.delta + sender_result.v;
    EXPECT_EQ(receiver_result.w, w2);
    for (int i = 0; i < receiver_result.w.size(); i++) {
      if (receiver_result.w[i] != w2[i]) {
        std::cout << "mismatch: " << i << " " << receiver_result.w[i] << " "
                  << w2[i] << std::endl;
      }
    }
  }

  mpc_utils::testing::CommChannelTestHelper helper_;
  std::unique_ptr<DistributedVectorOLE<T>> vole_0_;
  std::unique_ptr<DistributedVectorOLE<T>> vole_1_;
};

using MyTypes = ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t,
                                 absl::uint128, gf128, NTL::zz_p>;
TYPED_TEST_SUITE(DistributedVectorOLETest, MyTypes);

TYPED_TEST(DistributedVectorOLETest, TestSmallVectors) {
  // Set up NTL.
  int64_t modulus = 1152921504606846883L;  // 2^60 - 93
  if (std::is_same<TypeParam, NTL::ZZ_p>::value) {
    NTL::ZZ_p::init(NTL::conv<NTL::ZZ>(modulus));
  } else if (std::is_same<TypeParam, NTL::zz_p>::value) {
    NTL::zz_p::init(modulus);
  }

  auto sizes = {1, 2, 123};
  this->Precompute(50);
  for (int size : sizes) {
    this->TestVector(size);
  }
}

TYPED_TEST(DistributedVectorOLETest, TestLargeVector) {  // Set up NTL.
  int64_t modulus = 1152921504606846883L;                // 2^60 - 93
  if (std::is_same<TypeParam, NTL::ZZ_p>::value) {
    NTL::ZZ_p::init(NTL::conv<NTL::ZZ>(modulus));
  } else if (std::is_same<TypeParam, NTL::zz_p>::value) {
    NTL::zz_p::init(modulus);
  }

  int size = 500000;
  this->TestVector(size);
}

}  // namespace
}  // namespace distributed_vector_ole
