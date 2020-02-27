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
#include "distributed_vector_ole/gf128.h"
#include <thread>
#include "absl/memory/memory.h"
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
      ASSERT_OK_AND_ASSIGN(vole_0_, DistributedVectorOLE<T>::Create(chan1));
    });
    ASSERT_OK_AND_ASSIGN(vole_1_, DistributedVectorOLE<T>::Create(chan0));
    thread1.join();
  }

  void TestVector(int size) {
    Vector<T> u, v, w;
    T x(2);
    NTLContext<T> ntl_context;
    ntl_context.save();
    std::thread thread1([this, size, &ntl_context, &u, &v] {
      ntl_context.restore();
      ASSERT_OK_AND_ASSIGN(std::tie(u, v), vole_0_->RunSender(size));
    });
    ASSERT_OK_AND_ASSIGN(w, vole_1_->RunReceiver(size, x));
    thread1.join();
    Vector<T> w2 = u * x + v;
    EXPECT_EQ(w, w2);
  }

  mpc_utils::testing::CommChannelTestHelper helper_;
  std::unique_ptr<DistributedVectorOLE<T>> vole_0_;
  std::unique_ptr<DistributedVectorOLE<T>> vole_1_;
};

using MyTypes = ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t,
                                 absl::uint128, gf128, NTL::ZZ_p>;
TYPED_TEST_SUITE(DistributedVectorOLETest, MyTypes);

TYPED_TEST(DistributedVectorOLETest, TestSmallVectors) {
  for (int size = 1; size < 20; size += 4) {
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

}  // namespace
}  // namespace distributed_vector_ole
