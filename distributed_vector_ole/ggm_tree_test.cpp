#include "ggm_tree.h"
#include "gtest/gtest.h"
#include "mpc_utils/status_macros.h"

namespace distributed_vector_ole {
namespace {

TEST(GGMTree, Constructor) {
  std::vector<uint8_t> seed(AES_BLOCK_SIZE, 0);
  auto tree = GGMTree::Create(2, 1<<20, seed);
  EXPECT_OK(tree);
}

TEST(GGMTree, ArityMustBeAtLeastTwo) {
  std::vector<uint8_t> seed(AES_BLOCK_SIZE, 0);
  auto tree = GGMTree::Create(1, 1<<20, seed);
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::error::INVALID_ARGUMENT);
  EXPECT_EQ(tree.status().message(), "arity must be at least 2");
}

TEST(GGMTree, NumberOfLeavesMustNotBeNegative) {
  std::vector<uint8_t> seed(AES_BLOCK_SIZE, 0);
  auto tree = GGMTree::Create(2, -1, seed);
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::error::INVALID_ARGUMENT);
  EXPECT_EQ(tree.status().message(), "num_leaves must not be negative");
}

TEST(GGMTree, SeedSizeMustMatchBlockSize) {
  std::vector<uint8_t> seed(AES_BLOCK_SIZE - 1, 0);
  auto tree = GGMTree::Create(2, 1<<20, seed);
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::error::INVALID_ARGUMENT);
  EXPECT_EQ(tree.status().message(), "seed must have length kBlockSize");
}

}  // namespace
}  // namespace distributed_vector_ole
