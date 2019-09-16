// Two-Party Multi-Point Function Secret-Sharing, where one party knows the set
// of indices. Inputs:
//
//   Public: Integers N, t > 0.
//   IndexProvider: An additive share of a vector `z`, and a set of t indices 0
//   <= `index` <= N-1.
//   ValueProvider: An additive share of `z` of size t,
//
// The output of the protocol is an additive secret share of a vector `v` of
// length `N` such that `v` is zero in all positions except the ones in indices,
// where `v[indices[i]] = z[i]`.
// We also implement a variant where `z` is not secret-shared, but the product
// of a vector `y` held by the sender, and a scalar `x` held by the receiver.

#ifndef DISTRIBUTED_VECTOR_OLE_MPFSS_KNOWN_INDICES_H_
#define DISTRIBUTED_VECTOR_OLE_MPFSS_KNOWN_INDICES_H_

#include <algorithm>
#include "NTL/ZZ_p.h"
#include "absl/container/flat_hash_set.h"
#include "distributed_vector_ole/cuckoo_hasher.h"
#include "distributed_vector_ole/scalar_vector_gilboa_product.h"
#include "distributed_vector_ole/spfss_known_index.h"
#include "openssl/rand.h"

namespace distributed_vector_ole {

class MPFSSKnownIndices {
 public:
  // Creates an instance of MPFSSKnownIndices that communicates over the
  // given comm_channel. Optionally accepts a pointer to an existing
  // ScalarVectorGilboaProduct instance. If omitted, a new instance will be
  // created and managed by this class.
  static mpc_utils::StatusOr<std::unique_ptr<MPFSSKnownIndices>> Create(
      mpc_utils::comm_channel* channel,
      ScalarVectorGilboaProduct* gilboa = nullptr);

  // Does nothing if cached_output_size_ == output_size. Otherwise hashes the
  // interval [0, output_size) using hasher_, and saves the result in buckets_.
  // Saves computation time if called before Run*.
  mpc_utils::Status UpdateBuckets(int64_t output_size, int num_indices);

  // Runs the ValueProvider side of the protocol. `output` must point to an
  // array of pre-allocated Ts.
  template <typename T>
  mpc_utils::Status RunValueProvider(absl::Span<const T> val_share,
                                     absl::Span<T> output) {
    return mpc_utils::UnimplementedError(
        "MPFSSKnownIndices::RunValueProvider is unimplemented");
  }

  // Runs the IndexProvider side of the protocol. `output` must point to an
  // array of pre-allocated Ts, and all elements in `indices` must be between 0
  // and output.size() - 1.
  template <typename T>
  mpc_utils::Status RunIndexProvider(absl::Span<const T> val_share,
                                     absl::Span<const int64_t> indices,
                                     absl::Span<T> output) {
    return mpc_utils::UnimplementedError(
        "MPFSSKnownIndices::RunIndexProvider is unimplemented");
  }

  // Runs the ValueProvider side of the Vector-OLE optimized protocol. That is,
  // instead of being additively shared, `z` is set to `x y`, where `x` is owned
  // by the server and `y` is a vector of size `y_len` owned by the client.
  template <typename T>
  mpc_utils::Status RunValueProviderVectorOLE(T x, int y_len,
                                              absl::Span<T> output);

  // Runs the IndexProvider side of the Vector-OLE optimized protocol. See
  // RunValueProviderVectorOLE for a description.
  template <typename T>
  mpc_utils::Status RunIndexProviderVectorOLE(absl::Span<const T> y,
                                              absl::Span<const int64_t> indices,
                                              absl::Span<T> output);

 private:
  MPFSSKnownIndices(
      std::unique_ptr<CuckooHasher> hasher,
      std::vector<std::unique_ptr<SPFSSKnownIndex>> spfss,
      std::vector<std::unique_ptr<mpc_utils::comm_channel>> channels,
      std::unique_ptr<ScalarVectorGilboaProduct> owned_gilboa,
      ScalarVectorGilboaProduct* gilboa);

  // Constants for cuckoo hashing.
  static const int kNumHashFunctions = 3;
  static constexpr int NumBuckets(int num_indices) {
    return num_indices < 200 ? 300 : num_indices + num_indices / 2;
  }

  // Precomputed mapping of output indices to buckets.
  std::vector<std::vector<int64_t>> buckets_;

  // Output size that buckets_ was computed for. Will be updated if
  // Run{Server,Client} is called with a different size.
  absl::optional<int64_t> cached_output_size_;

  // Number of indices that buckets_ was computed for. Will be updated if
  // Run{Server,Client} is called with a different size.
  absl::optional<int> cached_num_indices_;

  // CuckooHasher instance used for assigning indices to buckets.
  std::unique_ptr<CuckooHasher> hasher_;

  // Single-point FSS instances. One for each OpenMP thread.
  std::vector<std::unique_ptr<SPFSSKnownIndex>> spfss_;

  // Communication channels used by SPFSS instances.
  std::vector<std::unique_ptr<mpc_utils::comm_channel>> channels_;

  // Gilboa multiplication instance. If `gilboa` was not passed at construction,
  // `gilboa_` it is equal to `owned_gilboa_.get()`.
  std::unique_ptr<ScalarVectorGilboaProduct> owned_gilboa_;
  ScalarVectorGilboaProduct* gilboa_;
};

template <typename T>
mpc_utils::Status MPFSSKnownIndices::RunValueProviderVectorOLE(
    T x, int y_len, absl::Span<T> output) {
  if (y_len < 1) {
    return mpc_utils::InvalidArgumentError("`y_len` must be positive");
  }
  RETURN_IF_ERROR(UpdateBuckets(output.size(), y_len));
  int num_buckets = buckets_.size();
  ASSIGN_OR_RETURN(std::vector<T> val_share,
                   gilboa_->RunValueProvider(x, num_buckets));

  // Zero out `output`.
  for (auto& val : output) {
    val = T(0);
  }

  NTLContext<T> context;
  context.save();
#pragma omp parallel num_threads(spfss_.size())
  {
    context.restore();
    int thread_id = 0;
#ifdef _OPENMP
    thread_id = omp_get_thread_num();
#endif
    // Compute FSS for each bucket, and map the results  back to `output`.
#pragma omp for schedule(static)
    for (int i = 0; i < num_buckets; i++) {
      int bucket_size = buckets_[i].size();
      if (bucket_size == 0) {
        continue;
      }
      std::vector<T> bucket_outputs(bucket_size);
      spfss_[thread_id]->RunValueProvider<T>(val_share[i],
                                             absl::MakeSpan(bucket_outputs));
#pragma omp critical
      for (int j = 0; j < bucket_size; j++) {
        output[buckets_[i][j]] += bucket_outputs[j];
      }
    }
  }
  return mpc_utils::OkStatus();
}

template <typename T>
mpc_utils::Status MPFSSKnownIndices::RunIndexProviderVectorOLE(
    absl::Span<const T> y, absl::Span<const int64_t> indices,
    absl::Span<T> output) {
  if (y.size() != indices.size()) {
    return mpc_utils::InvalidArgumentError(
        "`y` and `indices` must have the same size");
  }
  if (output.size() < indices.size()) {
    return mpc_utils::InvalidArgumentError(
        "`output` must be at least as long as `indices`");
  }
  if (y.empty()) {
    return mpc_utils::InvalidArgumentError(
        "`y` and `indices` must not be empty");
  }
  RETURN_IF_ERROR(UpdateBuckets(output.size(), y.size()));
  int num_buckets = buckets_.size();
  // Checking for uniqueness of `indices` takes time, so we only do that if
  // cuckoo hashing fails.
  auto status = hasher_->HashCuckoo(indices, y, num_buckets);
  if (!status.ok() && mpc_utils::IsInternal(status.status()) &&
      status.status().message() ==
          "Failed to insert element, maximum number of tries exhausted") {
    // Probably due to repeating indices.
    absl::flat_hash_set<int64_t> indices_set(indices.begin(), indices.end());
    if (indices.size() != indices_set.size()) {
      return mpc_utils::InvalidArgumentError("All `indices` must be unique");
    }
  }
  ASSIGN_OR_RETURN(auto hashed_inputs, status);

  // y padded with zeros and permuted according to hashed_inputs:
  // y_permuted[i] = y[j] if hashed_inputs[i] == j, and 0 otherwise.
  std::vector<T> y_permuted(num_buckets, T(0));
  for (int i = 0; i < num_buckets; i++) {
    if (hashed_inputs[i]) {
      y_permuted[i] = hashed_inputs[i]->second;
    }
  }
  ASSIGN_OR_RETURN(std::vector<T> val_share,
                   gilboa_->RunVectorProvider<T>(y_permuted));

  // Zero out `output`.
  std::fill(output.begin(), output.end(), T(0));

  NTLContext<T> context;
  context.save();
#pragma omp parallel num_threads(spfss_.size())
  {
    context.restore();
    int thread_id = 0;
#ifdef _OPENMP
    thread_id = omp_get_thread_num();
#endif
    // Compute FSS for each bucket, and map the results  back to `output`.
#pragma omp for schedule(static)
    for (int i = 0; i < num_buckets; i++) {
      int bucket_size = buckets_[i].size();
      if (bucket_size == 0) {
        continue;
      }
      int index_in_bucket = 0;
      if (hashed_inputs[i]) {
        // Find the index in the bucket. We can assume the bucket is sorted,
        // as it is created in ascending order in UpdateBuckets, and
        // HashSimple preserves the order.
        index_in_bucket =
            std::lower_bound(buckets_[i].begin(), buckets_[i].end(),
                             hashed_inputs[i]->first) -
            buckets_[i].begin();
      }
      std::vector<T> bucket_outputs(bucket_size);
      spfss_[thread_id]->RunIndexProvider(val_share[i], index_in_bucket,
                                          absl::MakeSpan(bucket_outputs));
#pragma omp critical
      for (int j = 0; j < bucket_size; j++) {
        output[buckets_[i][j]] += bucket_outputs[j];
      }
    }
  }
  return mpc_utils::OkStatus();
}

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_MPFSS_KNOWN_INDICES_H_
