// A CuckooHasher allows to hash collections of arbitrary types using simple
// hashing and cuckoo hashing. It is used to implement batching in various
// protocols.

#ifndef DISTRIBUTED_VECTOR_OLE_CUCKOO_HASHER_H_
#define DISTRIBUTED_VECTOR_OLE_CUCKOO_HASHER_H_

#include <omp.h>
#include <vector>
#include "NTL/ZZ.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "mpc_utils/canonical_errors.h"
#include "mpc_utils/statusor.h"
#include "openssl/sha.h"

namespace distributed_vector_ole {

// UpdateHash needs to be implemented for each type that should be hashed using
// CuckooHasher. Here we provide default implementations for integer types and
// string_views.
template <typename T, typename std::enable_if<
                          std::numeric_limits<T>::is_integer, int>::type = 0>
void UpdateHash(const T& input, SHA256_CTX* ctx) {
  SHA256_Update(ctx, &input, sizeof(input));
}

inline void UpdateHash(absl::string_view input, SHA256_CTX* ctx) {
  SHA256_Update(ctx, input.data(), input.size());
}

class CuckooHasher {
 public:
  // Creates a new hasher with `num_hash_functions` hash functions, using the
  // passed `seed`.
  //
  // Returns INVALID_ARGUMENT if `num_hash_functions` is not postive.
  static mpc_utils::StatusOr<std::unique_ptr<CuckooHasher>> Create(
      absl::string_view seed, int num_hash_functions = 3);

  // Hashes the inputs to `num_buckets` buckets using all hash functions created
  // at construction. Returns the vector of buckets, containing copies of the
  // original elements.
  //
  // Returns INVALID_ARGUMENT if `num_buckets` is not  positive.
  template <typename T>
  mpc_utils::StatusOr<std::vector<std::vector<T>>> HashSimple(
      absl::Span<const T> inputs, int64_t num_buckets) {
    if (num_buckets <= 0) {
      return mpc_utils::InvalidArgumentError("`num_buckets` must be positive");
    }
    std::vector<std::vector<T>> result(num_buckets);
#pragma omp parallel
    {
      // Split the input interval into evenly-sized chunks and assign one to
      // each thread. schedule(static) is needed to ensure the result can be
      // merged in order (see below).
      std::vector<std::vector<T>> thread_result(num_buckets);
#pragma omp for schedule(static) nowait
      for (int64_t i = 0; i < static_cast<int64_t>(inputs.size()); i++) {
        for (int j = 0; j < static_cast<int>(hash_states_.size()); j++) {
          thread_result[Hash(inputs[i], num_buckets, j)].push_back(inputs[i]);
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

  // Hashes the inputs to `num_buckets` buckets using Cuckoo Hashing.
  //
  // Returns INVALID_ARGUMENT if `num_buckets` is not  positive or
  // `inputs.size()` is larger than `num_buckets`.
  // Returns INTERNAL if insertion fails after trying to insert an element
  // `inputs.size()` times.
  template <typename K, typename V>
  mpc_utils::StatusOr<std::vector<absl::optional<std::pair<K, V>>>> HashCuckoo(
      absl::Span<const K> keys, absl::Span<const V> values,
      int64_t num_buckets) {
    if (hash_states_.size() == 1) {
      return mpc_utils::InvalidArgumentError(
          "`HashCuckoo` can only be called when at least 2 hash functions were "
          "specified at construction");
    }
    if (num_buckets <= 0) {
      return mpc_utils::InvalidArgumentError("`num_buckets` must be positive");
    }
    if (static_cast<int64_t>(keys.size()) > num_buckets) {
      return mpc_utils::InvalidArgumentError(
          "`ìnputs.size()` must not be larger than `num_buckets`");
    }
    std::vector<absl::optional<std::pair<K, V>>> result(num_buckets);
    std::vector<int> next_hash_function(num_buckets, 0);
    int64_t num_tries = keys.size();

    // Insert inputs one by one.
    for (int i = 0; i < static_cast<int>(keys.size()); i++) {
      std::pair<K, V> current_element =
          std::make_pair(std::move(keys[i]), std::move(values[i]));
      int current_hash_function = 0;
      for (int64_t tries = 0; tries < num_tries + 1; tries++) {
        if (tries == num_tries) {
          return mpc_utils::InternalError(
              "Failed to insert element, maximum number of tries exhausted");
        }
        int64_t index =
            Hash(current_element.first, num_buckets, current_hash_function);
        if (!result[index]) {
          // Bucket empty -> simply insert.
          result[index] = std::move(current_element);
          break;
        } else {
          // Bucket full -> evict element and increment hash function counter.
          std::swap(current_element, *result[index]);
          std::swap(current_hash_function, next_hash_function[index]);
          next_hash_function[index] =
              (next_hash_function[index] + 1) % hash_states_.size();
        }
      }
    }
    return result;
  }

 private:
  explicit CuckooHasher(std::vector<SHA256_CTX> hash_states);

  // Hashes `input` to an index less than `n` using hash function number `i`.
  template <typename T>
  int64_t Hash(const T& input, int64_t num_buckets, int hash_function_index) {
    SHA256_CTX state = hash_states_[hash_function_index];
    UpdateHash(input, &state);
    std::vector<uint8_t> digest(SHA256_DIGEST_LENGTH);
    SHA256_Final(digest.data(), &state);
    // TODO: Ensure all positions are chosen with exactly equal probability.
    NTL::ZZ digest_ntl;
    NTL::ZZFromBytes(digest_ntl, digest.data(), digest.size());
    return NTL::conv<uint64_t>(digest_ntl % num_buckets);
  }

  // One per hash function.
  std::vector<SHA256_CTX> hash_states_;
};

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_CUCKOO_HASHER_H_
