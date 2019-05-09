#include "distributed_vector_ole/spfss_known_index.h"
#include <thread>
#include <vector>
#include "absl/memory/memory.h"
#include "gtest/gtest.h"
#include "mpc_utils/comm_channel.hpp"
#include "mpc_utils/status_matchers.h"
#include "mpc_utils/testing/comm_channel_test_helper.hpp"

namespace distributed_vector_ole {

namespace {

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

  template <typename T>
  void TestVector(int size, int index, T val_share_0 = T(23),
                  T val_share_1 = T(42)) {
    T val = val_share_0 + val_share_1;
    std::vector<T> output_0(size);
    std::vector<T> output_1(size);

    NTL::ZZ_pContext ntl_context;
    ntl_context.save();
    std::thread thread1([this, &output_0, val_share_0, &ntl_context] {
      // Re-initialize modulus for current thread.
      ntl_context.restore();
      EXPECT_TRUE(spfss_known_index_0_
                      ->RunServer(val_share_0, absl::MakeSpan(output_0.data(),
                                                              output_0.size()))
                      .ok());
    });
    EXPECT_TRUE(
        spfss_known_index_1_
            ->RunClient(val_share_1, index,
                        absl::MakeSpan(output_1.data(), output_1.size()))
            .ok());
    thread1.join();

    for (int i = 0; i < size; i++) {
      T sum = output_0[i] + output_1[i];
      if (i != index) {
        EXPECT_EQ(sum, 0);
      } else {
        EXPECT_EQ(sum, val);
      }
    }
  }

  void TestAllTypes(int size, int index) {
    // Unsigned.
    TestVector<uint8_t>(size, index);
    TestVector<uint16_t>(size, index);
    TestVector<uint32_t>(size, index);
    TestVector<uint64_t>(size, index);
    TestVector<absl::uint128>(size, index);
    // Signed.
    TestVector<int8_t>(size, index);
    TestVector<int16_t>(size, index);
    TestVector<int32_t>(size, index);
    TestVector<int64_t>(size, index);
    // NTL.
    NTL::ZZ_p::init(NTL::conv<NTL::ZZ>(
        "340282366920938463463374607431768211297"));  // 2^128 - 159
    TestVector<NTL::ZZ_p>(size, index);
    NTL::ZZ_p::init(NTL::conv<NTL::ZZ>("18446744073709551557"));  // 2^64 - 59
    TestVector<NTL::ZZ_p>(size, index);
    NTL::ZZ_p::init(NTL::conv<NTL::ZZ>("4294967291"));  // 2^32 - 5
    TestVector<NTL::ZZ_p>(size, index);
    NTL::ZZ_p::init(NTL::conv<NTL::ZZ>("65521"));  // 2^16 - 15
    TestVector<NTL::ZZ_p>(size, index);
    NTL::ZZ_p::init(NTL::conv<NTL::ZZ>("251"));  // 2^8 - 5
    TestVector<NTL::ZZ_p>(size, index);
  }

  mpc_utils::testing::CommChannelTestHelper helper_;
  std::unique_ptr<SPFSSKnownIndex> spfss_known_index_0_;
  std::unique_ptr<SPFSSKnownIndex> spfss_known_index_1_;
};

TEST_F(SPFSSKnownIndexTest, TestSmallVectors) {
  for (int size = 1; size < 15; size++) {
    for (int index = 0; index < size; index++) {
      TestAllTypes(size, index);
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
