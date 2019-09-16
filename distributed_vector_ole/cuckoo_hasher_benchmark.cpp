#include <numeric>
#include "benchmark/benchmark.h"
#include "distributed_vector_ole/cuckoo_hasher.h"

namespace distributed_vector_ole {

namespace {

void BM_HashCuckoo(benchmark::State& state) {
  int num_hash_functions = 3;
  int num_elements = state.range(0);
  int num_buckets =
      static_cast<int>(std::max(200., std::ceil(1.5 * num_elements)));
  std::vector<int> keys(num_elements);
  std::iota(keys.begin(), keys.end(), 0);
  std::vector<int> values = keys;
  auto hasher = CuckooHasher::Create("seed", num_hash_functions).ValueOrDie();
  for (auto _ : state) {
    auto result = hasher
                      ->HashCuckoo(absl::MakeConstSpan(keys),
                                   absl::MakeConstSpan(values), num_buckets)
                      .ValueOrDie();
    ::benchmark::DoNotOptimize(result);
  }
}
// Benchmark all values of t for G_primal in the VOLE paper
// (https://eprint.iacr.org/2019/273.pdf).
BENCHMARK(BM_HashCuckoo)
    ->Arg(74)
    ->Arg(192)
    ->Arg(382)
    ->Arg(741)
    ->Arg(1442)
    ->Arg(5205);

void BM_HashSimple(benchmark::State& state) {
  int num_hash_functions = 3;
  int num_elements = state.range(0);
  int num_buckets =
      static_cast<int>(std::max(200., std::ceil(1.5 * num_elements)));
  std::vector<int> inputs(num_elements);
  std::iota(inputs.begin(), inputs.end(), 0);
  auto hasher = CuckooHasher::Create("seed", num_hash_functions).ValueOrDie();
  for (auto _ : state) {
    auto result = hasher->HashSimple(absl::MakeConstSpan(inputs), num_buckets)
                      .ValueOrDie();
    ::benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_HashSimple)
    ->Arg(1 << 11)
    ->Arg(1 << 14)
    ->Arg(1 << 16)
    ->Arg(1 << 18)
    ->Arg(1 << 20)
    ->Arg(1 << 24);

}  // namespace

}  // namespace distributed_vector_ole