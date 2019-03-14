#include "ggm_tree.h"
#include "gtest/gtest.h"
#include "mpc_utils/status_macros.h"

namespace distributed_vector_ole {
namespace {

TEST(GGMTreeTest, Constructor) {
  std::vector<uint8_t> seed(AES_BLOCK_SIZE, 0);
  auto tree = GGMTree::Create(2, 1<<20, seed);
  EXPECT_OK(tree);
}

TEST(GGMTreeTest, ConstructorArityMustBeAtLeastTwo) {
  std::vector<uint8_t> seed(AES_BLOCK_SIZE, 0);
  auto tree = GGMTree::Create(1, 1<<20, seed);
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::error::INVALID_ARGUMENT);
  EXPECT_EQ(tree.status().message(), "arity must be at least 2");
}

TEST(GGMTreeTest, ConstructorNumberOfLeavesMustNotBeNegative) {
  std::vector<uint8_t> seed(AES_BLOCK_SIZE, 0);
  auto tree = GGMTree::Create(2, -1, seed);
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::error::INVALID_ARGUMENT);
  EXPECT_EQ(tree.status().message(), "num_leaves must not be negative");
}

TEST(GGMTreeTest, ConstructorSeedSizeMustMatchBlockSize) {
  std::vector<uint8_t> seed(AES_BLOCK_SIZE - 1, 0);
  auto tree = GGMTree::Create(2, 1<<20, seed);
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::error::INVALID_ARGUMENT);
  EXPECT_EQ(tree.status().message(), "seed must have length kBlockSize");
}

TEST(GGMTree, GetValueInvalidLevelIndex) {
  std::vector<uint8_t> seed(AES_BLOCK_SIZE, 0);
  auto tree = GGMTree::Create(2, 1<<20, seed).ValueOrDie();
  auto value = tree->GetValueAtNode(-1, 0);
  ASSERT_FALSE(value.ok());
  EXPECT_EQ(value.status().code(), mpc_utils::error::INVALID_ARGUMENT);
  EXPECT_EQ(value.status().message(), "level_index out of range");
}

TEST(GGMTree, GetValueInvalidNodeIndex) {
  std::vector<uint8_t> seed(AES_BLOCK_SIZE, 0);
  auto tree = GGMTree::Create(2, 1<<20, seed).ValueOrDie();
  auto value = tree->GetValueAtNode(0, -1);
  ASSERT_FALSE(value.ok());
  EXPECT_EQ(value.status().code(), mpc_utils::error::INVALID_ARGUMENT);
  EXPECT_EQ(value.status().message(), "node_index out of range");
}

}  // namespace
}  // namespace distributed_vector_ole
