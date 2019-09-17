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

#include "distributed_vector_ole/cuckoo_hasher.h"
#include <numeric>
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "gtest/gtest.h"
#include "mpc_utils/status_matchers.h"

namespace distributed_vector_ole {

namespace {

template <typename T>
std::vector<T> GenerateInputs(int num_elements) {
  std::vector<T> result(num_elements);
  std::iota(result.begin(), result.end(), 123);
  return result;
}
template <>
std::vector<std::string> GenerateInputs<std::string>(int num_elements) {
  std::vector<std::string> result(num_elements);
  for (int i = 0; i < num_elements; i++) {
    result[i] = std::to_string(123 + i);
  }
  std::sort(result.begin(), result.end());
  return result;
}

template <typename T>
void TestSimpleHashing(int num_elements, int num_buckets,
                       int num_hash_functions) {
  ASSERT_OK_AND_ASSIGN(auto hasher,
                       CuckooHasher::Create("seed", num_hash_functions));
  std::vector<T> input = GenerateInputs<T>(num_elements);

  ASSERT_OK_AND_ASSIGN(
      auto buckets,
      hasher->HashSimple(absl::MakeConstSpan(input), num_buckets));
  // Check that each element appears the right number of times.
  absl::flat_hash_map<T, int> counts;
  for (int i = 0; i < num_buckets; i++) {
    EXPECT_TRUE(std::is_sorted(buckets[i].begin(), buckets[i].end()));
    for (int j = 0; j < static_cast<int>(buckets[i].size()); j++) {
      counts[buckets[i][j]]++;
    }
  }
  for (const T& v : input) {
    EXPECT_EQ(counts[v], num_hash_functions);
  }
}

template <typename T>
void TestCuckooHashing(int num_elements, int num_buckets,
                       int num_hash_functions) {
  ASSERT_OK_AND_ASSIGN(auto hasher,
                       CuckooHasher::Create("seed", num_hash_functions));
  std::vector<T> keys = GenerateInputs<T>(num_elements);
  std::vector<int> values = GenerateInputs<int>(num_elements);

  ASSERT_OK_AND_ASSIGN(
      auto buckets,
      hasher->HashCuckoo(absl::MakeConstSpan(keys), absl::MakeConstSpan(values),
                         num_buckets));
  absl::flat_hash_map<T, int> inputs;
  for (int i = 0; i < static_cast<int>(keys.size()); i++) {
    inputs[keys[i]] = values[i];
  }
  int num_valid = 0;
  for (int i = 0; i < static_cast<int>(buckets.size()); i++) {
    if (buckets[i]) {
      num_valid++;
      EXPECT_EQ(inputs[buckets[i]->first], buckets[i]->second);
    }
  }
  EXPECT_EQ(num_valid, keys.size());
}

TEST(CuckooHasher, TestSimpleHashing) {
  for (int num_elements = 0; num_elements < 1000; num_elements += 300) {
    for (int num_buckets = 1; num_buckets < 100; num_buckets += 10) {
      for (int num_hash_functions = 1; num_hash_functions < 5;
           num_hash_functions++) {
        TestSimpleHashing<int>(num_elements, num_buckets, num_hash_functions);
        TestSimpleHashing<uint64_t>(num_elements, num_buckets,
                                    num_hash_functions);
        TestSimpleHashing<absl::uint128>(num_elements, num_buckets,
                                         num_hash_functions);
        TestSimpleHashing<std::string>(num_elements, num_buckets,
                                       num_hash_functions);
      }
    }
  }
}

TEST(CuckooHasher, TestCuckooHashing) {
  // With 200 buckets
  const int num_hash_functions = 3;
  for (int num_buckets = 200; num_buckets < 1000; num_buckets += 100) {
    for (int num_elements = 0; 1.5 * num_elements < num_buckets;
         num_elements += 100) {
      TestCuckooHashing<int>(num_elements, num_buckets, num_hash_functions);
      TestCuckooHashing<uint64_t>(num_elements, num_buckets,
                                  num_hash_functions);
      TestCuckooHashing<absl::uint128>(num_elements, num_buckets,
                                       num_hash_functions);
      TestCuckooHashing<std::string>(num_elements, num_buckets,
                                     num_hash_functions);
    }
  }
}

TEST(CuckooHasher, TestConstructorNoHashFunctions) {
  auto status = CuckooHasher::Create("seed", 0);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.status().message(), "`num_hash_functions` must be positive");
}

TEST(CuckooHasher, TestSimpleHashingWithoutBuckets) {
  ASSERT_OK_AND_ASSIGN(auto hasher, CuckooHasher::Create("seed", 1));
  auto status = hasher->HashSimple(absl::Span<const int>(), 0);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.status().message(), "`num_buckets` must be positive");
}

TEST(CuckooHasher, TestCuckooHashingWithOneHashFunction) {
  ASSERT_OK_AND_ASSIGN(auto hasher, CuckooHasher::Create("seed", 1));
  auto status =
      hasher->HashCuckoo(absl::Span<const int>(), absl::Span<const int>(), 0);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.status().message(),
            "`HashCuckoo` can only be called when at least 2 hash functions "
            "were specified at construction");
}

TEST(CuckooHasher, TestCuckooHashingWithoutBuckets) {
  ASSERT_OK_AND_ASSIGN(auto hasher, CuckooHasher::Create("seed", 2));
  auto status =
      hasher->HashCuckoo(absl::Span<const int>(), absl::Span<const int>(), 0);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.status().message(), "`num_buckets` must be positive");
}

TEST(CuckooHasher, TestCuckooHashingWithTooFewBuckets) {
  ASSERT_OK_AND_ASSIGN(auto hasher, CuckooHasher::Create("seed", 2));
  auto status = hasher->HashCuckoo(absl::Span<const int>({1, 2}),
                                   absl::Span<const int>({1, 2}), 1);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.status().message(),
            "`ìnputs.size()` must not be larger than `num_buckets`");
}

TEST(CuckooHasher, TestCuckooHashingOverflow) {
  ASSERT_OK_AND_ASSIGN(auto hasher, CuckooHasher::Create("seed", 2));
  const int num_elements = 1 << 20;
  auto input = GenerateInputs<int>(num_elements);
  auto status = hasher->HashCuckoo(absl::MakeConstSpan(input),
                                   absl::MakeConstSpan(input), num_elements);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.status().message(),
            "Failed to insert element, maximum number of tries exhausted");
}

}  // namespace
}  // namespace distributed_vector_ole