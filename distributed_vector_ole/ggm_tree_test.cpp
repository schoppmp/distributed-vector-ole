#include "ggm_tree.h"
#include "gtest/gtest.h"

namespace distributed_vector_ole {
namespace {

TEST(GGMTree, Constructor) { GGMTree tree(2, 1 << 20, "some seed"); }

}  // namespace
}  // namespace distributed_vector_ole
