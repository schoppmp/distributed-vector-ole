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

#ifndef DISTRIBUTED_VECTOR_OLE_DISTRIBUTED_VECTOR_OLE_H_
#define DISTRIBUTED_VECTOR_OLE_DISTRIBUTED_VECTOR_OLE_H_

// Implements a distributed Vector-OLE that is run between a Sender and a
// Receiver. The Sender receives two vectors u, v, and the Receiver receives a
// vector w and a scalar x, such that ux + v = w.

#include <random>
#include "Eigen/Sparse"
#include "distributed_vector_ole/mpfss_known_indices.h"
#include "mpc_utils/boost_serialization/eigen.hpp"
#include "mpc_utils/openssl_uniform_bit_generator.hpp"

namespace distributed_vector_ole {

namespace distributed_vector_ole_internal {

// Returns the number of indices sufficient for 80-bit security according to
// Table 1 in https://eprint.iacr.org/2019/273.pdf (Parameter t).
inline mpc_utils::StatusOr<int> NumIndicesForSize(int64_t size) {
  if (size <= 57) {
    return size;  // Ensures t <= n.
  } else if (size <= 1 << 10) {
    return 57;
  } else if (size <= 1 << 11) {
    return 74;
  } else if (size <= 1 << 12) {
    return 98;
  } else if (size <= 1 << 14) {
    return 192;
  } else if (size <= 1 << 16) {
    return 382;
  } else if (size <= 1 << 18) {
    return 741;
  } else if (size <= 1 << 20) {
    return 1422;
  } else if (size <= 1 << 22) {
    return 2735;
  } else if (size <= 1 << 24) {
    return 5205;
  }
  return mpc_utils::InvalidArgumentError(
      "No security parameters provided for given `size`");
}

// Same as NumIndicesForSize, but for the size of the short seed vectors
// (Parameter k).
inline mpc_utils::StatusOr<int> SeedSizeForSize(int64_t size) {
  if (size <= 1 << 10) {
    return 652;
  } else if (size <= 1 << 11) {
    return 984;
  } else if (size <= 1 << 12) {
    return 1589;
  } else if (size <= 1 << 14) {
    return 3482;
  } else if (size <= 1 << 16) {
    return 7391;
  } else if (size <= 1 << 18) {
    return 15336;
  } else if (size <= 1 << 20) {
    return 32771;
  } else if (size <= 1 << 22) {
    return 67440;
  } else if (size <= 1 << 24) {
    return 139959;
  }
  return mpc_utils::InvalidArgumentError(
      "No security parameters provided for given `size`");
}
}  // namespace distributed_vector_ole_internal

template <typename T>
using Vector = Eigen::Matrix<T, 1, Eigen::Dynamic>;

template <typename T>
class DistributedVectorOLE {
 public:
  // Returns a new Vector-OLE generator that communicates over the given
  // comm_channel.
  static mpc_utils::StatusOr<std::unique_ptr<DistributedVectorOLE>> Create(
      mpc_utils::comm_channel* channel);

  // Performs precomputation such that subsequent calls to RunSender and
  // RunReceiver return faster.
  mpc_utils::Status Precompute(int64_t size);

  // Runs the protocol as the Sender role, returning two random vectors u, v of
  // length `size`.
  mpc_utils::StatusOr<std::pair<Vector<T>, Vector<T>>> RunSender(int64_t size);

  // Runs as the Receiver role. Returns a vector w of length `size`, such that
  // ux+v = w.
  mpc_utils::StatusOr<Vector<T>> RunReceiver(int64_t size, const T& x);

  // Runs as the Receiver, but with random input x. Returns (w, x).
  mpc_utils::StatusOr<std::pair<Vector<T>, T>> RunReceiver(int64_t size) {
    T x;
    Randomize(absl::MakeSpan(&x, 1));
    ASSIGN_OR_RETURN(Vector<T> w, RunReceiver(size, x));
    return std::make_pair(std::move(w), std::move(x));
  }

 private:
  DistributedVectorOLE(std::unique_ptr<MPFSSKnownIndices> mpfss,
                       std::unique_ptr<ScalarVectorGilboaProduct> gilboa,
                       Eigen::SparseMatrix<T, Eigen::ColMajor> code_generator,
                       mpc_utils::comm_channel* channel);

  // Generates uniformly random elements of type T and writes them to output.
  void Randomize(absl::Span<T> output);

  // Number of nonzeros in each column of code_generator_.
  const int kCodeGeneratorNonzeros = 10;

  // MPFSS instance for sharing the noise vector.
  std::unique_ptr<MPFSSKnownIndices> mpfss_;

  // Gilboa instance for computing the product of x with the Sender's short
  // seed.
  std::unique_ptr<ScalarVectorGilboaProduct> gilboa_;

  // Code generator matrix for expanding the seeds.
  Eigen::SparseMatrix<T, Eigen::ColMajor> code_generator_;

  // Size for which MPFSS buckets and code_generator were precomputed.
  absl::optional<int64_t> precomputed_size_;

  // Communication channel used by this class.
  mpc_utils::comm_channel* channel_;

  // Used to sample code_generator_.
  mpc_utils::OpenSSLUniformBitGenerator rng_;
};

template <typename T>
DistributedVectorOLE<T>::DistributedVectorOLE(
    std::unique_ptr<MPFSSKnownIndices> mpfss,
    std::unique_ptr<ScalarVectorGilboaProduct> gilboa,
    Eigen::SparseMatrix<T, Eigen::ColMajor> code_generator,
    mpc_utils::comm_channel* channel)
    : mpfss_(std::move(mpfss)),
      gilboa_(std::move(gilboa)),
      code_generator_(std::move(code_generator)),
      channel_(channel) {}

template <typename T>
void DistributedVectorOLE<T>::Randomize(absl::Span<T> output) {
  RAND_bytes(reinterpret_cast<uint8_t*>(output.data()),
             output.size() * sizeof(T));
}
template <>
void DistributedVectorOLE<NTL::ZZ_p>::Randomize(absl::Span<NTL::ZZ_p> output) {
  NTL::Vec<NTL::ZZ_p> ntl_vec = NTL::random_vec_ZZ_p(output.size());
  for (int64_t i = 0; i < static_cast<int64_t>(output.size()); i++) {
    output[i] = std::move(ntl_vec[i]);
  }
}

template <typename T>
mpc_utils::Status DistributedVectorOLE<T>::Precompute(int64_t size) {
  if (size < 1) {
    return mpc_utils::InvalidArgumentError("`size` must be positive");
  }
  if (precomputed_size_ && *precomputed_size_ == size) {
    return mpc_utils::OkStatus();  // Already precomputed for this size.
  }
  ASSIGN_OR_RETURN(int t,
                   distributed_vector_ole_internal::NumIndicesForSize(size));
  ASSIGN_OR_RETURN(int k,
                   distributed_vector_ole_internal::SeedSizeForSize(size));

  // Create Buckets for MPFSS.
  RETURN_IF_ERROR(mpfss_->UpdateBuckets(size, t));

  // Lower ID creates generator matrix and sends it over.
  // TODO: We can dynamically resize code_generator_ and only generate missing
  //   elements.
  std::vector<Eigen::Triplet<T>> triplets;
  triplets.reserve(kCodeGeneratorNonzeros * size);
  code_generator_.resize(k, size);
  if (channel_->get_id() < channel_->get_peer_id()) {
    Vector<T> random_elements(kCodeGeneratorNonzeros * size);
    Randomize(absl::MakeSpan(random_elements));
    std::uniform_int_distribution<int> dist(0, k - 1);
    for (int64_t col = 0; col < size; col++) {
      for (int i = 0; i < kCodeGeneratorNonzeros; i++) {
        int row = dist(rng_);
        triplets.push_back(Eigen::Triplet<T>(
            row, col,
            std::move(random_elements[col * kCodeGeneratorNonzeros + i])));
      }
    }
    channel_->send(triplets);
    channel_->flush();
  } else {
    channel_->recv(triplets);
  }
  code_generator_.setFromTriplets(triplets.begin(), triplets.end());
  precomputed_size_ = size;
  return mpc_utils::OkStatus();
}

template <typename T>
mpc_utils::StatusOr<std::pair<Vector<T>, Vector<T>>>
DistributedVectorOLE<T>::RunSender(int64_t size) {
  RETURN_IF_ERROR(Precompute(size));
  ASSIGN_OR_RETURN(int t,
                   distributed_vector_ole_internal::NumIndicesForSize(size));
  ASSIGN_OR_RETURN(int k,
                   distributed_vector_ole_internal::SeedSizeForSize(size));

  // Sample a and b, and compute c = ax+b.
  Vector<T> a(k), b(k), c(k);
  Randomize(absl::MakeSpan(a));
  Randomize(absl::MakeSpan(b));
  RETURN_IF_ERROR(gilboa_->RunVectorProvider<T>(a, absl::MakeSpan(c)));
  c += b;
  channel_->send(c);
  channel_->flush();

  // Sample y and indices, and compute MPFSS.
  absl::flat_hash_set<int64_t> indices_set;
  std::uniform_int_distribution<int64_t> dist(0, size - 1);
  while (static_cast<int>(indices_set.size()) < t) {
    indices_set.insert(dist(rng_));
  }
  std::vector<int64_t> indices(indices_set.begin(), indices_set.end());
  Vector<T> y(t), v0(size);
  Randomize(absl::MakeSpan(y));
  RETURN_IF_ERROR(
      mpfss_->RunIndexProviderVectorOLE<T>(y, indices, absl::MakeSpan(v0)));

  // Spread y into mu.
  Vector<T> mu = Vector<T>::Constant(size, T(0));
  std::fill(mu.data(), mu.data() + size, T(0));
  for (int i = 0; i < t; i++) {
    mu[indices[i]] = y[i];
  }

  // Compute u and v.
  Vector<T> u = a * code_generator_ + mu;
  Vector<T> v = b * code_generator_ - v0;
  return std::make_pair(std::move(u), std::move(v));
}

template <typename T>
mpc_utils::StatusOr<Vector<T>> DistributedVectorOLE<T>::RunReceiver(
    int64_t size, const T& x) {
  RETURN_IF_ERROR(Precompute(size));
  ASSIGN_OR_RETURN(int t,
                   distributed_vector_ole_internal::NumIndicesForSize(size));
  ASSIGN_OR_RETURN(int k,
                   distributed_vector_ole_internal::SeedSizeForSize(size));

  // Compute c = ax+b.
  Vector<T> c(k), c2(k);
  RETURN_IF_ERROR(gilboa_->RunValueProvider(x, absl::MakeSpan(c)));
  channel_->recv(c2);
  c += c2;

  // Compute MPFSS and return result.
  Vector<T> v1(size);
  RETURN_IF_ERROR(mpfss_->RunValueProviderVectorOLE(x, t, absl::MakeSpan(v1)));
  return Vector<T>(c * code_generator_ + v1);
}

template <typename T>
mpc_utils::StatusOr<std::unique_ptr<DistributedVectorOLE<T>>>
DistributedVectorOLE<T>::Create(comm_channel* channel) {
  // Initialize OMP threads with current NTL modulus. Note that if the number of
  // threads gets changed by the user afterwards, NTL will still crash, but
  // there's nothing we can do about that.
  NTLContext<T> context;
  context.save();
#pragma omp parallel
  { context.restore(); }

  ASSIGN_OR_RETURN(auto gilboa, ScalarVectorGilboaProduct::Create(channel));
  ASSIGN_OR_RETURN(auto mpfss,
                   MPFSSKnownIndices::Create(channel, gilboa.get()));
  Eigen::SparseMatrix<T, Eigen::ColMajor> code_generator;

  return absl::WrapUnique(new DistributedVectorOLE<T>(
      std::move(mpfss), std::move(gilboa), std::move(code_generator), channel));
}

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_DISTRIBUTED_VECTOR_OLE_H_