#include "benchmark/benchmark.h"
#include "distributed_vector_ole/ggm_tree.h"
#include "gtest/gtest.h"

namespace distributed_vector_ole {
namespace {

static void BM_Expand(benchmark::State& state) {
  GGMTree::Block seed(42);
  int arity = state.range(0);
  int64_t num_leaves = state.range(1);
  for (auto _ : state) {
    auto tree = GGMTree::Create(arity, num_leaves, seed);
    EXPECT_OK(tree);
  }
}
BENCHMARK(BM_Expand)->Ranges({
    {2, 1024},          // arity
    {1 << 12, 1 << 21}  // num_leaves
});

}  // namespace
}  // namespace distributed_vector_ole