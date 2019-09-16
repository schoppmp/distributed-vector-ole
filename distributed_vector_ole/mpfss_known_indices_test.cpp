#include "distributed_vector_ole/mpfss_known_indices.h"
#include "NTL/ZZ_p.h"
#include "absl/container/flat_hash_map.h"
#include "gtest/gtest.h"
#include "mpc_utils/status_matchers.h"
#include "mpc_utils/testing/comm_channel_test_helper.hpp"

namespace distributed_vector_ole {
namespace {

template <typename T>
class MPFSSKnownIndicesTest : public ::testing::Test {
 protected:
  MPFSSKnownIndicesTest() : helper_(false) {}
  void SetUp() {
    emp::initialize_relic();
    comm_channel *chan0 = helper_.GetChannel(0);
    comm_channel *chan1 = helper_.GetChannel(1);
    std::thread thread1([this, chan1] {
      ASSERT_OK_AND_ASSIGN(mpfss_known_indices_1_,
                           MPFSSKnownIndices::Create(chan1));
    });
    ASSERT_OK_AND_ASSIGN(mpfss_known_indices_0_,
                         MPFSSKnownIndices::Create(chan0));
    thread1.join();
  }

  void TestVectorOLE() {
    int len = 100;
    T x(2);
    std::vector<T> y({T(1), T(2), T(3)});
    std::vector<int64_t> indices = {23, 0, 42};
    std::vector<T> output_0(len), output_1(len);

    // Run protocol.
    NTL::ZZ_pContext ntl_context;
    ntl_context.save();
    std::thread thread1([this, &output_1, &y, &indices, &ntl_context] {
      ntl_context.restore();
      EXPECT_TRUE(mpfss_known_indices_1_
                      ->RunIndexProviderVectorOLE<T>(y, indices,
                                                     absl::MakeSpan(output_1))
                      .ok());
    });
    EXPECT_TRUE(mpfss_known_indices_0_
                    ->RunValueProviderVectorOLE<T>(x, y.size(),
                                                   absl::MakeSpan(output_0))
                    .ok());
    thread1.join();

    // Check correctness.
    absl::flat_hash_map<int64_t, T> nonzero_map;
    for (int i = 0; i < static_cast<int>(y.size()); i++) {
      nonzero_map[indices[i]] = x * y[i];
    }
    for (int i = 0; i < len; i++) {
      T sum = output_0[i] + output_1[i];
      if (nonzero_map.contains(i)) {
        EXPECT_EQ(sum, nonzero_map[i]);
      } else {
        EXPECT_EQ(sum, 0);
      }
    }
  }

  mpc_utils::testing::CommChannelTestHelper helper_;
  std::unique_ptr<MPFSSKnownIndices> mpfss_known_indices_0_;
  std::unique_ptr<MPFSSKnownIndices> mpfss_known_indices_1_;
};

using MPFSSKnownIndicesTypes =
    ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t, absl::uint128,
                     NTL::ZZ_p>;
TYPED_TEST_SUITE(MPFSSKnownIndicesTest, MPFSSKnownIndicesTypes);

TYPED_TEST(MPFSSKnownIndicesTest, TestVectorOLE) {
  if (std::is_same<TypeParam, NTL::ZZ_p>::value) {
    for (const auto &prime : {
             "340282366920938463463374607431768211297",  // 2^128 - 159
             "18446744073709551557",                     // 2^64 - 59
             "4294967291",                               // 2^32 - 5
             "65521",                                    // 2^16 - 15
             "251"                                       // 2^8 - 5
         }) {
      NTL::ZZ_p::init(NTL::conv<NTL::ZZ>(prime));
      this->TestVectorOLE();
    }
  } else {
    this->TestVectorOLE();
  }
}

TYPED_TEST(MPFSSKnownIndicesTest, TestDifferentLenghts) {
  std::vector<TypeParam> output(2);
  auto status = this->mpfss_known_indices_0_
                    ->template RunIndexProviderVectorOLE<TypeParam>(
                        {TypeParam(0)}, {0, 1}, absl::MakeSpan(output));
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.message(), "`y` and `indices` must have the same size");
}

TYPED_TEST(MPFSSKnownIndicesTest, TestIndicesLongerThanOutput) {
  std::vector<TypeParam> output(1);
  auto status =
      this->mpfss_known_indices_0_
          ->template RunIndexProviderVectorOLE<TypeParam>(
              {TypeParam(0), TypeParam(1)}, {0, 1}, absl::MakeSpan(output));
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.message(), "`output` must be at least as long as `indices`");
}

TYPED_TEST(MPFSSKnownIndicesTest, TestYEmpty) {
  std::vector<TypeParam> output(1);
  auto status = this->mpfss_known_indices_0_
                    ->template RunIndexProviderVectorOLE<TypeParam>(
                        {}, {}, absl::MakeSpan(output));
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.message(), "`y` and `indices` must not be empty");
}

TYPED_TEST(MPFSSKnownIndicesTest, TestRepeatingIndices) {
  std::vector<TypeParam> output(2);
  auto status =
      this->mpfss_known_indices_0_
          ->template RunIndexProviderVectorOLE<TypeParam>(
              {TypeParam(0), TypeParam(1)}, {0, 0}, absl::MakeSpan(output));
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.message(), "All `indices` must be unique");
}

TYPED_TEST(MPFSSKnownIndicesTest, TestYLenNegative) {
  std::vector<TypeParam> output(1);
  auto status = this->mpfss_known_indices_0_->RunValueProviderVectorOLE(
      TypeParam(0), -1, absl::MakeSpan(output));
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.message(), "`y_len` must be positive");
}

}  // namespace
}  // namespace distributed_vector_ole