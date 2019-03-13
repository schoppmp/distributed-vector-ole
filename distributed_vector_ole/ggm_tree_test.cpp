#include "ggm_tree.h"
#include "gtest/gtest.h"

namespace distributed_vector_ole {
namespace {

TEST(GGMTree, Constructor) {
  std::vector<uint8_t> seed(AES_BLOCK_SIZE, 0);
  GGMTree tree(2, 1 << 20, seed);
}

}  // namespace
}  // namespace distributed_vector_ole
