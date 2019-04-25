#include "distributed_vector_ole/ggm_tree.h"
#include <cmath>
#include "gtest/gtest.h"
#include "mpc_utils/status_macros.h"
#include "mpc_utils/status_matchers.h"

namespace distributed_vector_ole {
namespace {

class GGMTreeTest : public ::testing::Test {
 protected:
  GGMTreeTest() : seed_(42) {}
  void SetUp(int arity, int64_t num_leaves) {
    ASSERT_OK_AND_ASSIGN(tree_, GGMTree::Create(arity, num_leaves, seed_));
  }

  mpc_utils::StatusOr<std::vector<std::vector<GGMTree::Block>>>
  ExpandNaively() {
    // Allocate levels and copy seed.
    std::vector<std::vector<GGMTree::Block>> levels(tree_->num_levels());
    int64_t level_size = 1;
    for (int i = 0; i < tree_->num_levels(); i++) {
      levels[i].resize(level_size, 0);
      level_size *= tree_->arity();
    }
    ASSIGN_OR_RETURN(levels[0][0], tree_->GetValueAtNode(0, 0));

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
          levels[level_index + 1][tree_->arity() * node_index + key_index] ^=
              levels[level_index][node_index];
        }
      }
      max_node_index *= tree_->arity();
    }
    return levels;
  }

  void CheckCorrectness() {
    ASSERT_OK_AND_ASSIGN(auto levels_check, ExpandNaively());
    ASSERT_EQ(levels_check.size(), tree_->num_levels());
    // Check for correctness at the leaves.
    for (int64_t i = 0; i < tree_->num_leaves(); i++) {
      auto leaf_check = levels_check.back()[i];
      ASSERT_OK_AND_ASSIGN(auto leaf, tree_->GetValueAtLeaf(i));
      EXPECT_EQ(leaf_check, leaf);
    }
  }

  void CheckExceptOnPathToIndex(GGMTree* tree2, int64_t missing_index) {
    EXPECT_EQ(tree_->arity(), tree2->arity());
    EXPECT_EQ(tree_->num_leaves(), tree2->num_leaves());
    EXPECT_EQ(tree_->num_levels(), tree2->num_levels());
    for (int i = 0; i < tree2->num_leaves(); i++) {
      ASSERT_OK_AND_ASSIGN(auto leaf2, tree2->GetValueAtLeaf(i));
      if (i == missing_index) {
        EXPECT_EQ(leaf2, 0);
      } else {
        ASSERT_OK_AND_ASSIGN(auto leaf, tree2->GetValueAtLeaf(i));
        EXPECT_EQ(leaf, leaf2);
      }
    }

    // Check inner nodes on path to `missing_index`.
    std::vector<int64_t> missing_path(tree2->num_levels());
    missing_path[tree2->num_levels() - 1] = missing_index;
    for (int level = tree2->num_levels() - 2; level > 0; level--) {
      missing_path[level] = missing_path[level + 1] / tree2->arity();
      ASSERT_OK_AND_ASSIGN(auto node,
                           tree2->GetValueAtNode(level, missing_path[level]));
      EXPECT_EQ(node, 0);
    }
  }

  std::unique_ptr<GGMTree> tree_;
  absl::uint128 seed_;
};

TEST_F(GGMTreeTest, Constructor) {
  ASSERT_OK_AND_ASSIGN(auto tree, GGMTree::Create(2, 23, seed_));
}

TEST_F(GGMTreeTest, Expansion) {
  for (int arity = 2; arity < 10; arity++) {
    for (int64_t num_leaves = 1; num_leaves < 50; num_leaves++) {
      SetUp(arity, num_leaves);
      CheckCorrectness();
    }
  }
}

TEST_F(GGMTreeTest, Constructor2) {
  for (int arity = 2; arity < 10; arity++) {
    for (int64_t num_leaves = 1; num_leaves < 50; num_leaves++) {
      for (int64_t missing_index :
           {int64_t(0), 42 % num_leaves, num_leaves - 1}) {
        SetUp(arity, num_leaves);
        auto xors = tree_->GetSiblingWiseXOR();
        ASSERT_OK_AND_ASSIGN(auto tree2, GGMTree::CreateFromSiblingWiseXOR(
                                             arity, num_leaves, missing_index,
                                             xors, tree_->keys()));
        CheckExceptOnPathToIndex(tree2.get(), missing_index);
      }
    }
  }
}

TEST_F(GGMTreeTest, GetSiblingXOR) {
  SetUp(8, 1 << 15);
  auto sums = tree_->GetSiblingWiseXOR();
  for (int level = 0; level < tree_->num_levels() - 1; level++) {
    for (int sibling = 0; sibling < tree_->arity(); sibling++) {
      GGMTree::Block sum = 0;
      for (int node = 0; node < tree_->level_size(level); node++) {
        sum ^= tree_->GetValueAtNode(level + 1, node * tree_->arity() + sibling)
                   .ValueOrDie();
      }
      EXPECT_EQ(sum, sums[level][sibling]);
    }
  }
}

TEST_F(GGMTreeTest, ConstructorArityMustBeAtLeastTwo) {
  auto tree = GGMTree::Create(1, 23, seed_);
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::StatusCode::kInvalidArgument);
  EXPECT_EQ(tree.status().message(), "arity must be at least 2");
}

TEST_F(GGMTreeTest, ConstructorNumberOfLeavesMustBePositive) {
  auto tree = GGMTree::Create(2, 0, seed_);
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::StatusCode::kInvalidArgument);
  EXPECT_EQ(tree.status().message(), "num_leaves must be positive");
}

TEST_F(GGMTreeTest, GetValueInvalidLevelIndex) {
  SetUp(2, 23);
  auto value = tree_->GetValueAtNode(-1, 0);
  ASSERT_FALSE(value.ok());
  EXPECT_EQ(value.status().code(), mpc_utils::StatusCode::kInvalidArgument);
  EXPECT_EQ(value.status().message(), "level_index out of range");
}

TEST_F(GGMTreeTest, GetValueInvalidNodeIndex) {
  SetUp(2, 23);
  auto value = tree_->GetValueAtNode(0, -1);
  ASSERT_FALSE(value.ok());
  EXPECT_EQ(value.status().code(), mpc_utils::StatusCode::kInvalidArgument);
  EXPECT_EQ(value.status().message(), "node_index out of range");
}

TEST_F(GGMTreeTest, Constructor2ArityMustBeAtLeastTwo) {
  auto tree = GGMTree::CreateFromSiblingWiseXOR(1, 0, 0, {}, {});
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::StatusCode::kInvalidArgument);
  EXPECT_EQ(tree.status().message(), "arity must be at least 2");
}

TEST_F(GGMTreeTest, Constructor2NumberOfLeavesMustBePositive) {
  auto tree = GGMTree::CreateFromSiblingWiseXOR(2, 0, 0, {}, {});
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::StatusCode::kInvalidArgument);
  EXPECT_EQ(tree.status().message(), "`num_leaves` must be positive");
}

TEST_F(GGMTreeTest, Constructor2MissingIndexTooLarge) {
  auto tree = GGMTree::CreateFromSiblingWiseXOR(2, 1, 2, {}, {});
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::StatusCode::kInvalidArgument);
  EXPECT_EQ(tree.status().message(),
            "`missing_index` must be smaller than `num_leaves`");
}

TEST_F(GGMTreeTest, Constructor2SameLengths) {
  auto tree = GGMTree::CreateFromSiblingWiseXOR(2, 3, 0, {{23, 42}, {123}}, {});
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::StatusCode::kInvalidArgument);
  EXPECT_EQ(tree.status().message(),
            "All elements of `sibling_wise_xors` must have length `arity`");
}

TEST_F(GGMTreeTest, Constructor2NumLevelsTooSmall) {
  std::vector<std::vector<GGMTree::Block>> partial_seeds = {
      {23, 42}, {23, 42}, {23, 42}};
  auto tree = GGMTree::CreateFromSiblingWiseXOR(2, 9, 0, partial_seeds, {});
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::StatusCode::kInvalidArgument);
  EXPECT_EQ(tree.status().message(),
            "Dimensions passed in `sibling_wise_xors` to small for "
            "`num_leaves`");
}

TEST_F(GGMTreeTest, Constructor2KeysEmpty) {
  auto tree = GGMTree::CreateFromSiblingWiseXOR(2, 1, 0, {{23, 42}}, {});
  ASSERT_FALSE(tree.ok());
  EXPECT_EQ(tree.status().code(), mpc_utils::StatusCode::kInvalidArgument);
  EXPECT_EQ(tree.status().message(), "`keys` must not be empty");
}

}  // namespace
}  // namespace distributed_vector_ole
