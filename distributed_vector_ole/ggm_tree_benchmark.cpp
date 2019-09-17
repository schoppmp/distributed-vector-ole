//    Distributed Vector-OLE
//    Copyright (C) 2019 Phillipp Schoppmann and Adria Gascon
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Affero General Public License as
//    published by the Free Software Foundation, either version 3 of the
//    License, or (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Affero General Public License for more details.
//
//    You should have received a copy of the GNU Affero General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "benchmark/benchmark.h"
#include "distributed_vector_ole/ggm_tree.h"

namespace distributed_vector_ole {
namespace {

static void BM_Create(benchmark::State& state) {
  GGMTree::Block seed(42);
  int arity = state.range(0);
  int64_t num_leaves = state.range(1);
  for (auto _ : state) {
    auto tree = GGMTree::Create(arity, num_leaves, seed);
    benchmark::DoNotOptimize(tree);
  }
}
BENCHMARK(BM_Create)->Ranges({
    {2, 1024},          // arity
    {1 << 12, 1 << 24}  // num_leaves
});

static void BM_SiblingWiseXOR(benchmark::State& state) {
  GGMTree::Block seed(42);
  int arity = state.range(0);
  int64_t num_leaves = state.range(1);
  auto tree = GGMTree::Create(arity, num_leaves, seed).ValueOrDie();
  for (auto _ : state) {
    auto values = tree->GetSiblingWiseXOR();
    benchmark::DoNotOptimize(values);
  }
}
BENCHMARK(BM_SiblingWiseXOR)
    ->Ranges({
        {2, 1024},          // arity
        {1 << 12, 1 << 24}  // num_leaves
    });

static void BM_CreateFromSiblingWiseXOR(benchmark::State& state) {
  GGMTree::Block seed(42);
  int arity = state.range(0);
  int64_t num_leaves = state.range(1);
  int missing_index = 42 % num_leaves;
  auto tree = GGMTree::Create(arity, num_leaves, seed).ValueOrDie();
  auto xors = tree->GetSiblingWiseXOR();
  for (auto _ : state) {
    auto tree2 = GGMTree::CreateFromSiblingWiseXOR(
        arity, num_leaves, missing_index, xors, tree->keys());
    benchmark::DoNotOptimize(tree2);
  }
}
BENCHMARK(BM_CreateFromSiblingWiseXOR)
    ->Ranges({
        {2, 1024},          // arity
        {1 << 12, 1 << 24}  // num_leaves
    });

}  // namespace
}  // namespace distributed_vector_ole
