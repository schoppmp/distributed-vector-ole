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

}  // namespace
}  // namespace distributed_vector_ole
