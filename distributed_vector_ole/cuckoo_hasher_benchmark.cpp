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
  absl::uint128 seed(-1234);
  auto hasher = CuckooHasher::Create(seed, num_hash_functions).ValueOrDie();
  for (auto _ : state) {
    auto result =
        hasher->HashCuckoo(absl::MakeConstSpan(keys), num_buckets).ValueOrDie();
    ::benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_HashCuckoo)
    ->Arg(1 << 11)
    ->Arg(1 << 14)
    ->Arg(1 << 16)
    ->Arg(1 << 18)
    ->Arg(1 << 20)
    ->Arg(1 << 24);

void BM_HashSimple(benchmark::State& state) {
  int num_hash_functions = 3;
  int num_elements = state.range(0);
  int num_buckets =
      static_cast<int>(std::max(200., std::ceil(1.5 * num_elements)));
  std::vector<int> inputs(num_elements);
  std::iota(inputs.begin(), inputs.end(), 0);
  absl::uint128 seed(-1234);
  auto hasher = CuckooHasher::Create(seed, num_hash_functions).ValueOrDie();
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