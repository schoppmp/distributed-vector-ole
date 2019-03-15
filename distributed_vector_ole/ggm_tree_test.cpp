#include "ggm_tree.h"
#include <cmath>
#include "gtest/gtest.h"
#include "mpc_utils/status_macros.h"

namespace distributed_vector_ole {
namespace {

class GGMTreeTest : public ::testing::Test {
 protected:
  GGMTreeTest() : seed_(42) {}
  void SetUp(int arity, int64_t num_leaves) {
    auto tree = GGMTree::Create(arity, num_leaves, seed_);
    ASSERT_TRUE(tree.ok());
    tree_ = std::move(tree.ValueOrDie());
  }

  std::vector<std::vector<GGMTree::Block>> ExpandNaively() {
    // Allocate levels and copy seed.
    std::vector<std::vector<GGMTree::Block>> levels(tree_->num_levels());
    int64_t level_size = 1;
    for (int i = 0; i < tree_->num_levels(); i++) {
      levels[i].resize(level_size, 0);
      level_size *= tree_->arity();
    }
    auto value = tree_->GetValueAtNode(0, 0);
    EXPECT_OK(value);
    levels[0][0] = value.ValueOrDie();

    // Iterate over levels, then nodes, then keys.
    int64_t max_node_index = 1;
    for (int level_index = 0; level_index < tree_->num_levels() - 1;
         level_index++) {
      for (int64_t node_index = 0; node_index < max_node_index; node_index++) {
        for (int key_index = 0; key_index < tree_->arity(); key_index++) {
          AES_encrypt(
              reinterpret_cast<uint8_t*>(&levels[level_index][node_index]),
              reinterpret_cast<uint8_t*>(
                  &levels[level_index + 1]
                         [tree_->arity() * node_index + key_index]),
              &tree_->expanded_keys()[key_index]);
        }
      }
      max_node_index *= tree_->arity();
    }
    return levels;
  }

  void CheckCorrectness() {
    auto levels_check = ExpandNaively();
    ASSERT_EQ(levels_check.size(), tree_->num_levels());
    // Check for correctness at the leaves.
    for (int64_t i = 0; i < tree_->num_leaves(); i++) {
      auto leaf_check = levels_check.back()[i];
      auto leaf = tree_->GetValueAtLeaf(i);
      ASSERT_TRUE(leaf.ok());
      EXPECT_EQ(leaf_check, leaf.ValueOrDie());
    }
  }

  std::unique_ptr<GGMTree> tree_;
  absl::uint128 seed_;
};

TEST_F(GGMTreeTest, Constructor) {
  auto tree = GGMTree::Create(2, 23, seed_);
  EXPECT_OK(tree);
}

TEST_F(GGMTreeTest, ExpansionSmall) {
  for (int arity = 2; arity < 10; arity++) {
    for (int64_t num_leaves = 1; num_leaves < 100; num_leaves++) {
      SetUp(arity, num_leaves);
      CheckCorrectness();
    }
  }
}

TEST_F(GGMTreeTest, ExpansionLargeOdd) {
  for (int arity = 7; arity < 100; arity += 13) {
    for (int64_t num_leaves = 123; num_leaves < 1 << 20;
         num_leaves = num_leaves * 31 + 17) {
      SetUp(arity, num_leaves);
      CheckCorrectness();
    }
  }
}

TEST_F(GGMTreeTest, ExpansionLargeEven) {
  for (int arity = 2; arity < 1024; arity *= 2) {
    for (int64_t num_leaves = 1; num_leaves < 1 << 20; num_leaves *= 1 << 5) {
      SetUp(arity, num_leaves);
      CheckCorrectness();
    }
  }
}

TEST_F(GGMTreeTest, ConstructorArityMustBeAtLeastTwo) {
  auto tree = GGMTree::Create(1, 23, seed_);
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::error::INVALID_ARGUMENT);
  EXPECT_EQ(tree.status().message(), "arity must be at least 2");
}

TEST_F(GGMTreeTest, ConstructorNumberOfLeavesMustBePositive) {
  auto tree = GGMTree::Create(2, 0, seed_);
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::error::INVALID_ARGUMENT);
  EXPECT_EQ(tree.status().message(), "num_leaves must be positive");
}

TEST_F(GGMTreeTest, GetValueInvalidLevelIndex) {
  SetUp(2, 23);
  auto value = tree_->GetValueAtNode(-1, 0);
  ASSERT_FALSE(value.ok());
  EXPECT_EQ(value.status().code(), mpc_utils::error::INVALID_ARGUMENT);
  EXPECT_EQ(value.status().message(), "level_index out of range");
}

TEST_F(GGMTreeTest, GetValueInvalidNodeIndex) {
  SetUp(2, 23);
  auto value = tree_->GetValueAtNode(0, -1);
  ASSERT_FALSE(value.ok());
  EXPECT_EQ(value.status().code(), mpc_utils::error::INVALID_ARGUMENT);
  EXPECT_EQ(value.status().message(), "node_index out of range");
}

}  // namespace
}  // namespace distributed_vector_ole
