#include "distributed_vector_ole/all_but_one_random_ot.h"
#include <thread>
#include "absl/memory/memory.h"
#include "gtest/gtest.h"
#include "mpc_utils/comm_channel.hpp"
#include "mpc_utils/status_matchers.h"
#include "mpc_utils/testing/comm_channel_test_helper.hpp"

namespace distributed_vector_ole {

namespace {

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

  template <typename T>
  void TestVector(int size, int index) {
    std::vector<T> output_0(size);
    std::vector<T> output_1(size);
    std::thread thread1([this, &output_0] {
      EXPECT_TRUE(
          all_but_one_rot_0_
              ->RunServer(absl::MakeSpan(output_0.data(), output_0.size()))
              .ok());
    });
    EXPECT_TRUE(
        all_but_one_rot_1_
            ->RunClient(index, absl::MakeSpan(output_1.data(), output_1.size()))
            .ok());
    thread1.join();

    for (int i = 0; i < size; i++) {
      if (i != index) {
        EXPECT_EQ(output_0[i], output_1[i]);
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
  }

  mpc_utils::testing::CommChannelTestHelper helper_;
  std::unique_ptr<AllButOneRandomOT> all_but_one_rot_0_;
  std::unique_ptr<AllButOneRandomOT> all_but_one_rot_1_;
};

TEST_F(AllButOneRandomOTTest, TestSmallVectors) {
  for (int size = 1; size < 15; size++) {
    for (int index = 0; index < size; index++) {
      TestAllTypes(size, index);
    }
  }
}

TEST_F(AllButOneRandomOTTest, TestLargeVector) { TestAllTypes(1234567, 1234); }

TEST_F(AllButOneRandomOTTest, TestEmptyOutputServer) {
  auto status = all_but_one_rot_0_->RunClient(0, absl::Span<int>());
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.message(), "`output` must not be empty");
}

TEST_F(AllButOneRandomOTTest, TestEmptyOutputClient) {
  auto status = all_but_one_rot_1_->RunClient(0, absl::Span<int>());
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.message(), "`output` must not be empty");
}

TEST_F(AllButOneRandomOTTest, TestIndexNegative) {
  std::vector<int> dummy(100);
  auto status = all_but_one_rot_1_->RunClient(
      -1, absl::MakeSpan(dummy.data(), dummy.size() + 1));
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.message(), "`index` out of range");
}

TEST_F(AllButOneRandomOTTest, TestIndexTooLarge) {
  std::vector<int> dummy(100);
  auto status = all_but_one_rot_1_->RunClient(
      dummy.size(), absl::MakeSpan(dummy.data(), dummy.size()));
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.message(), "`index` out of range");
}

TEST(AllButOneRandomOT, TestNullChannel) {
  auto status = AllButOneRandomOT::Create(nullptr);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.status().message(), "`channel` must not be NULL");
}

}  // namespace

}  // namespace distributed_vector_ole
