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

#ifndef DISTRIBUTED_VECTOR_OLE_CUCKOO_HASHER_H_
#define DISTRIBUTED_VECTOR_OLE_CUCKOO_HASHER_H_

// A CuckooHasher allows to hash collections of arbitrary types using simple
// hashing and cuckoo hashing. It is used to implement batching in various
// protocols.

#include <omp.h>
#include <algorithm>
#include <vector>
#include "NTL/ZZ.h"
#include "absl/container/inlined_vector.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "mpc_utils/canonical_errors.h"
#include "mpc_utils/status_macros.h"
#include "mpc_utils/statusor.h"
#include "openssl/aes.h"

namespace distributed_vector_ole {

class CuckooHasher {
 public:
  static const int kDefaultHashFunctions = 3;
  // Creates a new hasher with `num_hash_functions` hash functions, using the
  // passed `seed`.
  //
  // Returns INVALID_ARGUMENT if `num_hash_functions` is not postive.
  static mpc_utils::StatusOr<std::unique_ptr<CuckooHasher>> Create(
      absl::uint128 seed, int num_hash_functions = kDefaultHashFunctions,
      double statistical_security = 40);

  // Hashes the input with each of the hash functions. Returns a vector that
  // contains for each element the indices of the buckets assigned to it.
  //
  // Returns INVALID_ARGUMENT if `num_buckets` is not  positive.
  template <typename T, int compiled_num_hash_functions = kDefaultHashFunctions>
  mpc_utils::StatusOr<
      std::vector<absl::InlinedVector<int64_t, compiled_num_hash_functions>>>
  Hash(absl::Span<const T> inputs, int64_t num_buckets);

  // Hashes the inputs to `num_buckets` buckets using all hash functions created
  // at construction. Returns the vector of buckets, containing indices into
  // `inputs`.
  //
  // Returns INVALID_ARGUMENT if `num_buckets` is not  positive.
  template <typename T>
  mpc_utils::StatusOr<std::vector<std::vector<int64_t>>> HashSimple(
      absl::Span<const T> inputs, int64_t num_buckets);

  // Hashes the inputs to `num_buckets` buckets using Cuckoo Hashing.
  // Returns a vector of indices into `inputs`, or -1 in positions that no input
  // get mapped to.
  //
  // Returns INVALID_ARGUMENT if `num_buckets` is not  positive or
  // `inputs.size()` is larger than `num_buckets`.
  // Returns INTERNAL if insertion fails after trying to insert an element
  // `inputs.size()` times.
  template <typename T>
  mpc_utils::StatusOr<std::vector<int64_t>> HashCuckoo(
      absl::Span<const T> inputs, int64_t num_buckets);

  // Returns the number of buckets necessary such that inserting `num_inputs`
  // inputs fails with probability at most 2**(-statistical_security_). The
  // parameters for this function have been chosen experimentally as described
  // in this paper: https://eprint.iacr.org/2018/579.pdf
  //
  // Returns UNIMPLEMENTED if the number of hash functions is not 2 or 3.
  // Returns INVALID_ARGUMENT if num_inputs is negative.
  mpc_utils::StatusOr<int64_t> GetOptimalNumberOfBuckets(int64_t num_inputs);

 private:
  explicit CuckooHasher(AES_KEY expanded_seed, int num_hash_functions,
                        double statistical_security);

  // Hashes `input` to a uint128.
  template <typename T>
  absl::uint128 HashToUint128(const T &in);

  // Gets the value of the `i`-th hash function to [num_buckets] from the given
  // 128-bit hash.
  int64_t HashToBucket(absl::uint128 hash, int64_t num_buckets,
                       int hash_function);

  AES_KEY expanded_seed_;
  int num_hash_functions_;
  double statistical_security_;
};

template <typename T, int compiled_num_hash_functions>
mpc_utils::StatusOr<
    std::vector<absl::InlinedVector<int64_t, compiled_num_hash_functions>>>
CuckooHasher::Hash(absl::Span<const T> inputs, int64_t num_buckets) {
  if (num_buckets <= 0) {
    return mpc_utils::InvalidArgumentError("`num_buckets` must be positive");
  }
  std::vector<absl::InlinedVector<int64_t, compiled_num_hash_functions>> result(
      inputs.size(), absl::InlinedVector<int64_t, compiled_num_hash_functions>(
                         num_hash_functions_));
#pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < static_cast<int64_t>(inputs.size()); i++) {
    absl::uint128 current_hash = HashToUint128(inputs[i]);
    for (int j = 0; j < num_hash_functions_; j++) {
      result[i][j] = HashToBucket(current_hash, num_buckets, j);
    }
  }
  return result;
}

template <typename T>
mpc_utils::StatusOr<std::vector<std::vector<int64_t>>> CuckooHasher::HashSimple(
    absl::Span<const T> inputs, int64_t num_buckets) {
  if (num_buckets <= 0) {
    return mpc_utils::InvalidArgumentError("`num_buckets` must be positive");
  }
  double bits_needed = statistical_security_ + std::log2(inputs.size()) +
                       std::log2(num_hash_functions_) + std::log2(num_buckets);
  if (bits_needed > 128) {
    return mpc_utils::InvalidArgumentError(absl::StrCat(
        "The given sizes would require ", bits_needed,
        "-bit hashes for the desired statistical security of ",
        statistical_security_,
        " bits. The current hash function only supports 128 bits"));
  }
  ASSIGN_OR_RETURN(auto hashes, Hash(inputs, num_buckets));
  std::vector<std::vector<int64_t>> result(num_buckets);
#pragma omp parallel
  {
    // Split the input interval into evenly-sized chunks and assign one to
    // each thread. schedule(static) is needed to ensure the result can be
    // merged in order (see below).
    std::vector<std::vector<int64_t>> thread_result(num_buckets);
#pragma omp for schedule(static) nowait
    for (int64_t i = 0; i < static_cast<int64_t>(inputs.size()); i++) {
      for (int j = 0; j < num_hash_functions_; j++) {
        thread_result[hashes[i][j]].push_back(i);
      }
    }
    // Have each thread append their buckets to the result, ensuring the order
    // remains deterministic.
    // See also: https://stackoverflow.com/a/18671256
#pragma omp for schedule(static) ordered
    for (int thread = 0; thread < omp_get_num_threads(); thread++) {
#pragma omp ordered
      for (int64_t i = 0; i < num_buckets; i++) {
        result[i].insert(result[i].end(), thread_result[i].begin(),
                         thread_result[i].end());
      }
    }
  }
  return result;
}

template <typename T>
mpc_utils::StatusOr<std::vector<int64_t>> CuckooHasher::HashCuckoo(
    absl::Span<const T> inputs, int64_t num_buckets) {
  if (num_hash_functions_ == 1) {
    return mpc_utils::InvalidArgumentError(
        "`HashCuckoo` can only be called when at least 2 hash functions were "
        "specified at construction");
  }
  if (num_buckets <= 0) {
    return mpc_utils::InvalidArgumentError("`num_buckets` must be positive");
  }
  if (static_cast<int64_t>(inputs.size()) > num_buckets) {
    return mpc_utils::InvalidArgumentError(
        "`inputs.size()` must not be larger than `num_buckets`");
  }
  double bits_needed = statistical_security_ + std::log2(inputs.size()) +
                       std::log2(num_hash_functions_) + std::log2(num_buckets);
  if (bits_needed > 128) {
    return mpc_utils::InvalidArgumentError(absl::StrCat(
        "The given sizes would require ", bits_needed,
        "-bit hashes for the desired statistical security of ",
        statistical_security_,
        " bits. The current hash function only supports 128 bits"));
  }
  std::vector<int64_t> buckets(num_buckets, -1);
  std::vector<int> next_hash_function(num_buckets, 0);
  int64_t num_tries = inputs.size();

  // Hash all elements.
  ASSIGN_OR_RETURN(auto hashes, Hash(inputs, num_buckets));

  // Insert inputs one by one.
  for (int64_t i = 0; i < static_cast<int64_t>(inputs.size()); i++) {
    int64_t current_element = i;
    int current_hash_function = 0;
    for (int64_t tries = 0; tries < num_tries + 1; tries++) {
      if (tries == num_tries) {
        return mpc_utils::InternalError(
            "Failed to insert element, maximum number of tries exhausted");
      }
      int64_t index = hashes[current_element][current_hash_function];
      if (buckets[index] == -1) {
        // Bucket empty -> simply insert.
        buckets[index] = current_element;
        break;
      } else {
        // Bucket full -> evict element and increment hash function counter.
        std::swap(current_element, buckets[index]);
        std::swap(current_hash_function, next_hash_function[index]);
        next_hash_function[index] =
            (next_hash_function[index] + 1) % num_hash_functions_;
      }
    }
  }

  //  // Map buckets back to their corresponding key-value pairs.
  //  std::vector<absl::optional<std::pair<K, V>>> result(num_buckets);
  //  for (int64_t i = 0; i < num_buckets; i++) {
  //    if (buckets[i] != -1) {
  //      result[i] = std::make_pair(keys[buckets[i]], values[buckets[i]]);
  //    }
  //  }
  return buckets;
}

// Works for any type that's convertible to absl::uint128.
template <typename T>
absl::uint128 CuckooHasher::HashToUint128(const T &in) {
  absl::uint128 in128(in);
  absl::uint128 result;
  AES_encrypt(reinterpret_cast<const uint8_t *>(&in128),
              reinterpret_cast<uint8_t *>(&result), &expanded_seed_);
  result ^= in128;
  return result;
}

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_CUCKOO_HASHER_H_
