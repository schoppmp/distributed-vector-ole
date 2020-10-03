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

#include "Eigen/Dense"
#include "Eigen/Sparse"
#include "distributed_vector_ole/aes_uniform_bit_generator.h"
#include "distributed_vector_ole/internal/scalar_helpers.h"
#include "distributed_vector_ole/mpfss_known_indices.h"
#include "mpc_utils/boost_serialization/eigen.hpp"

namespace distributed_vector_ole {

// Security parameters. These are defined in distributed_vector_ole.cpp.
struct VOLEParameters {
  // Maximum VOLE size that can be computed with the given set of parameters.
  static const std::vector<int> output_size;
  // Seed size for each VOLE batch of size output_size[i].
  static const std::vector<int> seed_size;
  // Number of LPN noise indices for a VOLE of size at most output_size[i].
  static const std::vector<int> num_noise_indices;
  // Number of nonzeros in each column of code_generator_.
  static const int kCodeGeneratorNonzeros;
};

template <typename T>
using Vector = Eigen::Matrix<T, 1, Eigen::Dynamic>;

template <typename T>
class DistributedVectorOLE {
 public:
  // A random Vector OLE triple has the form w = u * delta + v, where u and v
  // belong to the sender, and w and delta to the receiver.
  struct SenderResult {
    SenderResult() = default;
    Vector<T> u, v;
  };
  struct ReceiverResult {
    ReceiverResult() = default;
    Vector<T> w;
    T delta;
  };

  // Returns a new Vector-OLE generator that communicates over the given
  // comm_channel, with the given statistical_security.
  static mpc_utils::StatusOr<std::unique_ptr<DistributedVectorOLE>> Create(
      mpc_utils::comm_channel *channel, double statistical_security = 40);

  // Performs precomputation such that subsequent calls to RunSender return
  // faster. Optionally updates the batch size.
  mpc_utils::Status PrecomputeSender(int64_t batch_size);

  // Performs precomputation such that subsequent calls to RunReceiver return
  // faster. Optionally updates the batch size. If delta is omitted, it is
  // generated randomly.
  mpc_utils::Status PrecomputeReceiver(int64_t batch_size, T delta);
  mpc_utils::Status PrecomputeReceiver(int64_t batch_size) {
    T delta;
    ScalarHelper<T>::Randomize(absl::MakeSpan(&delta, 1));
    return PrecomputeReceiver(batch_size, delta);
  }

  // Runs the protocol as the Sender role, returning two pseudorandom vectors u,
  // v of length `size`.
  mpc_utils::StatusOr<SenderResult> RunSender(int64_t size);

  // Runs as the Receiver role. Returns a vector w of length `size` and a scalar
  // delta, such that u * delta + v = w. Note that delta will remain the same
  // value until PrecomputeReceiver is called again. For most applications this
  // should not be a problem.
  mpc_utils::StatusOr<ReceiverResult> RunReceiver(int64_t size);

 private:
  DistributedVectorOLE(std::unique_ptr<MPFSSKnownIndices> mpfss,
                       std::unique_ptr<ScalarVectorGilboaProduct> gilboa,
                       Eigen::SparseMatrix<T, Eigen::ColMajor> code_generator,
                       mpc_utils::comm_channel *channel,
                       double statistical_security);

  // Computes the code generator and sets up MPFSS buckets. Called by
  // PrecomputeSender and PrecomputeReceiver.
  mpc_utils::Status PrecomputeCommon(int64_t output_size);

  // Expands the sender's seeds to `size`, using the given LPN noise parameter.
  // Updates sender_cached_, sender_vole_seed_ and sender_mpfss_seed_ with the
  // result of the expansion.
  mpc_utils::Status ExpandSender(int64_t output_size, int new_vole_seed_size,
                                 int new_mpfss_seed_size);
  mpc_utils::Status ExpandSender() {
    return ExpandSender(batch_size_ + vole_seed_size_ + mpfss_seed_size_,
                        vole_seed_size_, mpfss_seed_size_);
  }

  // Expands the receiver's seed to `size` using the given LPN noise parameter.
  mpc_utils::Status ExpandReceiver(int64_t output_size, int new_vole_seed_size,
                                   int new_mpfss_seed_size);
  mpc_utils::Status ExpandReceiver() {
    return ExpandReceiver(batch_size_ + vole_seed_size_ + mpfss_seed_size_,
                          vole_seed_size_, mpfss_seed_size_);
  }

  // Returns a cached SenderResult with the given `size`.
  // Returns OUT_OF_RANGE if the cache is smaller than `size`.
  mpc_utils::StatusOr<SenderResult> GetSenderCached(int64_t size);

  // Returns a cached ReceiverResult with the given `size`.
  // Returns OUT_OF_RANGE if the cache is smaller than `size`.
  mpc_utils::StatusOr<ReceiverResult> GetReceiverCached(int64_t size);

  // MPFSS instance for sharing the noise vector.
  std::unique_ptr<MPFSSKnownIndices> mpfss_;

  // Gilboa instance for computing the short seeds during precomputation.
  std::unique_ptr<ScalarVectorGilboaProduct> gilboa_;

  // Code generator matrix for expanding the seeds.
  Eigen::SparseMatrix<T, Eigen::ColMajor> code_generator_;

  // Cached vectors u, v for the sender.
  SenderResult sender_cached_;

  // Sender's seed used in VOLE expansion.
  SenderResult sender_vole_seed_;

  // Sender's seed used in MPFSS.
  SenderResult sender_mpfss_seed_;

  // Cached vector w and delta for the receiver.
  ReceiverResult receiver_cached_;

  // Receiver's seed used in VOLE expansion.
  ReceiverResult receiver_vole_seed_;

  // Receiver's seed used in MPFSS.
  ReceiverResult receiver_mpfss_seed_;

  // Communication channel used by this class.
  mpc_utils::comm_channel *channel_;

  // Batch size. Each expansion will be performed in batches of this size.
  int64_t batch_size_;

  // Size of the VOLE seed. After bootstrapping, this will be equal to
  // sender_vole_seed_.size() receiver_vole_seed.size().
  int vole_seed_size_;

  // Size of the MPFSS seed. After bootstrapping, this will be equal to
  // sender_mpfss_seed_.size() receiver_mpfss_seed_.size().
  int mpfss_seed_size_;

  // Number of noise indices.
  int num_noise_indices_;

  // Whether PrecomputeSender has been called.
  bool sender_precomputation_done_;

  // Whether PrecomputeReceiver has been called.
  bool receiver_precomputation_done_;

  // Statistical security parameter.
  double statistical_security_;
};

template <typename T>
DistributedVectorOLE<T>::DistributedVectorOLE(
    std::unique_ptr<MPFSSKnownIndices> mpfss,
    std::unique_ptr<ScalarVectorGilboaProduct> gilboa,
    Eigen::SparseMatrix<T, Eigen::ColMajor> code_generator,
    mpc_utils::comm_channel *channel, double statistical_security)
    : mpfss_(std::move(mpfss)),
      gilboa_(std::move(gilboa)),
      code_generator_(std::move(code_generator)),
      channel_(channel),
      batch_size_(0),
      vole_seed_size_(0),
      mpfss_seed_size_(0),
      num_noise_indices_(0),
      sender_precomputation_done_(false),
      receiver_precomputation_done_(false),
      statistical_security_(statistical_security) {}

template <typename T>
mpc_utils::StatusOr<std::unique_ptr<DistributedVectorOLE<T>>>
DistributedVectorOLE<T>::Create(comm_channel *channel,
                                double statistical_security) {
  if (!channel) {
    return mpc_utils::InvalidArgumentError("`channel` must not be NULL");
  }
  if (statistical_security < 0) {
    return mpc_utils::InvalidArgumentError(
        "`statistical_security` must not be negative.");
  }
  // Make sure we have enough bits of statistical security for MPFSS, Gilboa and
  // generator sampling to fail independently.
  statistical_security += std::log2(3);

  // Initialize OMP threads with current NTL modulus. Note that if the number of
  // threads gets changed by the user afterwards, NTL will still crash, but
  // there's nothing we can do about that.
  NTLContext<T> context;
  context.save();
#pragma omp parallel
  { context.restore(); }

  ASSIGN_OR_RETURN(auto gilboa, ScalarVectorGilboaProduct::Create(
                                    channel, statistical_security));
  ASSIGN_OR_RETURN(auto mpfss,
                   MPFSSKnownIndices::Create(channel, statistical_security));
  Eigen::SparseMatrix<T, Eigen::ColMajor> code_generator;

  return absl::WrapUnique(new DistributedVectorOLE<T>(
      std::move(mpfss), std::move(gilboa), std::move(code_generator), channel,
      statistical_security));
}

template <typename T>
mpc_utils::Status DistributedVectorOLE<T>::PrecomputeSender(
    int64_t batch_size) {
  if (batch_size < 1) {
    return mpc_utils::InvalidArgumentError("`batch_size` must be positive");
  }
  batch_size_ = 0;  // We're just expanding seeds, we don't want any output.
                    // We'll set it back to batch_size in the end.
  // Compute first seed using Gilboa multiplication.
  num_noise_indices_ = VOLEParameters::num_noise_indices[0];
  vole_seed_size_ = VOLEParameters::seed_size[0];
  ASSIGN_OR_RETURN(mpfss_seed_size_, mpfss_->NumBuckets(num_noise_indices_));
  Vector<T> w(vole_seed_size_ + mpfss_seed_size_);
  sender_cached_.u = Vector<T>(w.size());
  sender_cached_.v = Vector<T>(w.size());
  ScalarHelper<T>::Randomize(absl::MakeSpan(sender_cached_.u));
  ScalarHelper<T>::Randomize(absl::MakeSpan(sender_cached_.v));
  RETURN_IF_ERROR(
      gilboa_->RunVectorProvider<T>(sender_cached_.u, absl::MakeSpan(w)));
  w += sender_cached_.v;
  channel_->send(w);
  channel_->flush();
  ASSIGN_OR_RETURN(sender_vole_seed_, GetSenderCached(vole_seed_size_));
  ASSIGN_OR_RETURN(sender_mpfss_seed_, GetSenderCached(mpfss_seed_size_));

  // Iteratively expand seeds until we have the desired batch size.
  for (int i = 0; i < static_cast<int>(VOLEParameters::seed_size.size()) - 1;
       i++) {
    if (VOLEParameters::output_size[i] >=
        batch_size + mpfss_seed_size_ + vole_seed_size_) {
      // Exit early if the current output size is enough for the given batch
      // size and seed sizes.
      break;
    }
    int next_vole_seed_size = VOLEParameters::seed_size[i + 1];
    ASSIGN_OR_RETURN(
        int next_mpfss_seed_size,
        mpfss_->NumBuckets(VOLEParameters::num_noise_indices[i + 1]));
    int64_t output_size = next_vole_seed_size + next_mpfss_seed_size;

    // Compute code generator.
    RETURN_IF_ERROR(PrecomputeCommon(output_size));

    // Set seed size for the expansion. The seed will be updated after the
    // expansion using the new size.
    RETURN_IF_ERROR(
        ExpandSender(output_size, next_vole_seed_size, next_mpfss_seed_size));
    num_noise_indices_ = VOLEParameters::num_noise_indices[i + 1];
  }

  // Generate code generator for the chosen batch_size. Ensure that it is not
  // larger than the largest supported output size.
  batch_size_ = std::min(
      batch_size,
      static_cast<int64_t>(
          VOLEParameters::output_size[VOLEParameters::output_size.size() - 1] -
          mpfss_seed_size_ - vole_seed_size_));
  RETURN_IF_ERROR(
      PrecomputeCommon(batch_size_ + vole_seed_size_ + mpfss_seed_size_));
  sender_precomputation_done_ = true;
  return mpc_utils::OkStatus();
}

template <typename T>
mpc_utils::Status DistributedVectorOLE<T>::PrecomputeReceiver(
    int64_t batch_size, T delta) {
  if (batch_size < 1) {
    return mpc_utils::InvalidArgumentError("`batch_size` must be positive");
  }
  batch_size_ = 0;  // We're just expanding seeds, we don't want any output.
                    // We'll set it back to batch_size in the end.
  // Compute first seeds using Gilboa multiplication.
  num_noise_indices_ = VOLEParameters::num_noise_indices[0];
  vole_seed_size_ = VOLEParameters::seed_size[0];
  ASSIGN_OR_RETURN(mpfss_seed_size_, mpfss_->NumBuckets(num_noise_indices_));
  Vector<T> w2(vole_seed_size_ + mpfss_seed_size_);
  receiver_cached_.w = Vector<T>(w2.size());
  receiver_cached_.delta = delta;
  RETURN_IF_ERROR(
      gilboa_->RunValueProvider(delta, absl::MakeSpan(receiver_cached_.w)));
  channel_->recv(w2);
  receiver_cached_.w += w2;

  ASSIGN_OR_RETURN(receiver_vole_seed_, GetReceiverCached(vole_seed_size_));
  ASSIGN_OR_RETURN(receiver_mpfss_seed_, GetReceiverCached(mpfss_seed_size_));

  // Iteratively expand seeds until we have the desired batch size.
  for (int i = 0; i < static_cast<int>(VOLEParameters::seed_size.size()) - 1;
       i++) {
    if (VOLEParameters::output_size[i] >=
        batch_size + vole_seed_size_ + mpfss_seed_size_) {
      // Exit early if the current output size is enough for the given batch
      // size and seed sizes.
      break;
    }
    int next_vole_seed_size = VOLEParameters::seed_size[i + 1];
    ASSIGN_OR_RETURN(
        int next_mpfss_seed_size,
        mpfss_->NumBuckets(VOLEParameters::num_noise_indices[i + 1]));
    int64_t output_size = next_vole_seed_size + next_mpfss_seed_size;

    // Compute code generator.
    RETURN_IF_ERROR(PrecomputeCommon(output_size));

    // Set seed size for the expansion. The seed will be updated after the
    // expansion using the new size.
    RETURN_IF_ERROR(
        ExpandReceiver(output_size, next_vole_seed_size, next_mpfss_seed_size));
    num_noise_indices_ = VOLEParameters::num_noise_indices[i + 1];
  }

  // Generate code generator for the chosen batch_size. Ensure that it is not
  // larger than the largest supported output size.
  batch_size_ = std::min(
      batch_size,
      static_cast<int64_t>(
          VOLEParameters::output_size[VOLEParameters::output_size.size() - 1] -
          mpfss_seed_size_ - vole_seed_size_));
  RETURN_IF_ERROR(
      PrecomputeCommon(batch_size_ + vole_seed_size_ + mpfss_seed_size_));
  receiver_precomputation_done_ = true;
  return mpc_utils::OkStatus();
}

template <typename T>
mpc_utils::Status DistributedVectorOLE<T>::PrecomputeCommon(
    int64_t output_size) {
  // Create Buckets for MPFSS.
  RETURN_IF_ERROR(mpfss_->UpdateBuckets(output_size, num_noise_indices_));

  // Lower ID creates random seed for generator  matrix and sends it over.
  std::vector<Eigen::Triplet<T>> triplets;
  triplets.reserve(VOLEParameters::kCodeGeneratorNonzeros * output_size);
  code_generator_.resize(vole_seed_size_, output_size);
  std::vector<uint8_t> seed(32);
  if (channel_->get_id() < channel_->get_peer_id()) {
    RAND_bytes(seed.data(), seed.size());
    channel_->send(seed);
    channel_->flush();
  } else {
    channel_->recv(seed);
  }
  ASSIGN_OR_RETURN(auto rng, AESUniformBitGenerator::Create(
                                 seed, VOLEParameters::kCodeGeneratorNonzeros *
                                           output_size));

  // Check we can sample enough random elements with the given statistical
  // security.
  double statistical_security_per_element =
      std::log2(double(VOLEParameters::kCodeGeneratorNonzeros * output_size)) +
      statistical_security_;
  if (!ScalarHelper<T>::CanBeHashedInto(statistical_security_per_element,
                                        128)) {
    return mpc_utils::InvalidArgumentError(
        "Cannot sample enough random elements for the code generator with the "
        "given statistical security");
  }
  // Sample random elements for code generator.
  Vector<T> random_elements(VOLEParameters::kCodeGeneratorNonzeros *
                            output_size);
  for (int64_t i = 0; i < VOLEParameters::kCodeGeneratorNonzeros * output_size;
       i++) {
    absl::uint128 random128 = absl::MakeUint128(rng(), rng());
    random_elements[i] = ScalarHelper<T>::FromUint128(random128);
  }

  // Sample random indexes for code generator.
  // TODO: This can posibbly be parallelized by inserting into block matrices
  //   and then joining them in the end.
  std::uniform_int_distribution<int> dist(0, vole_seed_size_ - 1);
  code_generator_.reserve(Eigen::VectorXi::Constant(
      output_size, VOLEParameters::kCodeGeneratorNonzeros));
  for (int64_t col = 0; col < output_size; col++) {
    for (int i = 0; i < VOLEParameters::kCodeGeneratorNonzeros; i++) {
      int row = dist(rng);
      code_generator_.coeffRef(row, col) = std::move(
          random_elements[col * VOLEParameters::kCodeGeneratorNonzeros + i]);
    }
  }
  code_generator_.makeCompressed();
  return mpc_utils::OkStatus();
}

template <typename T>
mpc_utils::Status DistributedVectorOLE<T>::ExpandSender(
    int64_t output_size, int new_vole_seed_size, int new_mpfss_seed_size) {
  // Sanity-check code generator dimensions.
  if (sender_vole_seed_.u.size() != sender_vole_seed_.v.size()) {
    return mpc_utils::InternalError("Both seeds must have the same size");
  }

  if (code_generator_.rows() !=
          static_cast<int64_t>(sender_vole_seed_.u.size()) ||
      code_generator_.cols() != output_size) {
    return mpc_utils::InternalError("Code generator has the wrong dimensions");
  }

  // Sample y and indices, and compute MPFSS.
  std::vector<uint8_t> seed(32);
  RAND_bytes(seed.data(), seed.size());
  ASSIGN_OR_RETURN(auto rng,
                   AESUniformBitGenerator::Create(seed, num_noise_indices_));
  absl::flat_hash_set<int64_t> indices_set;
  std::uniform_int_distribution<int64_t> dist(0, output_size - 1);
  while (static_cast<int>(indices_set.size()) < num_noise_indices_) {
    indices_set.insert(dist(rng));
  }
  std::vector<int64_t> indices(indices_set.begin(), indices_set.end());
  Vector<T> y(num_noise_indices_), v0(output_size);
  ScalarHelper<T>::Randomize(absl::MakeSpan(y));
  RETURN_IF_ERROR(mpfss_->RunIndexProviderVectorOLE<T>(
      y, indices, sender_mpfss_seed_.u, sender_mpfss_seed_.v,
      absl::MakeSpan(v0)));

  // Spread y into mu.
  Vector<T> mu(output_size);
  std::fill(mu.data(), mu.data() + output_size, T(0));
  for (int i = 0; i < num_noise_indices_; i++) {
    mu[indices[i]] = y[i];
  }

  // Compute expansion and append it to the cache.
  sender_cached_.u.conservativeResize(sender_cached_.u.size() + output_size);
  sender_cached_.v.conservativeResize(sender_cached_.v.size() + output_size);
  sender_cached_.u.tail(output_size) =
      sender_vole_seed_.u * code_generator_ + mu;
  sender_cached_.v.tail(output_size) =
      sender_vole_seed_.v * code_generator_ - v0;

  // Update seeds.
  ASSIGN_OR_RETURN(sender_vole_seed_, GetSenderCached(new_vole_seed_size));
  ASSIGN_OR_RETURN(sender_mpfss_seed_, GetSenderCached(new_mpfss_seed_size));
  vole_seed_size_ = new_vole_seed_size;
  mpfss_seed_size_ = new_mpfss_seed_size;

  return mpc_utils::OkStatus();
}

template <typename T>
mpc_utils::Status DistributedVectorOLE<T>::ExpandReceiver(
    int64_t output_size, int new_vole_seed_size, int new_mpfss_seed_size) {
  // Sanity-check code generator dimensions.
  if (code_generator_.rows() !=
          static_cast<int64_t>(receiver_vole_seed_.w.size()) ||
      code_generator_.cols() != output_size) {
    return mpc_utils::InternalError("Code generator has the wrong dimensions");
  }

  // Compute MPFSS and expand seed, appending to the cache.
  receiver_cached_.w.conservativeResize(receiver_cached_.w.size() +
                                        output_size);
  Vector<T> v1(output_size);
  RETURN_IF_ERROR(mpfss_->RunValueProviderVectorOLE<T>(
      receiver_mpfss_seed_.delta, num_noise_indices_, receiver_mpfss_seed_.w,
      absl::MakeSpan(v1)));
  receiver_cached_.w.tail(output_size) =
      receiver_vole_seed_.w * code_generator_ + v1;

  // Update seeds.
  ASSIGN_OR_RETURN(receiver_vole_seed_, GetReceiverCached(new_vole_seed_size));
  ASSIGN_OR_RETURN(receiver_mpfss_seed_,
                   GetReceiverCached(new_mpfss_seed_size));
  vole_seed_size_ = new_vole_seed_size;
  mpfss_seed_size_ = new_mpfss_seed_size;

  return mpc_utils::OkStatus();
}

template <typename T>
mpc_utils::StatusOr<typename DistributedVectorOLE<T>::SenderResult>
DistributedVectorOLE<T>::GetSenderCached(int64_t size) {
  if (sender_cached_.u.size() < size) {
    return mpc_utils::OutOfRangeError(absl::StrCat(
        "Requested ", size, " cached sender elements, but cache size is only ",
        sender_cached_.u.size()));
  }
  SenderResult result;
  result.u = sender_cached_.u.tail(size);
  result.v = sender_cached_.v.tail(size);
  sender_cached_.u.conservativeResize(sender_cached_.u.size() - size);
  sender_cached_.v.conservativeResize(sender_cached_.v.size() - size);
  return result;
}

template <typename T>
mpc_utils::StatusOr<typename DistributedVectorOLE<T>::ReceiverResult>
DistributedVectorOLE<T>::GetReceiverCached(int64_t size) {
  if (receiver_cached_.w.size() < size) {
    return mpc_utils::OutOfRangeError(
        absl::StrCat("Requested ", size,
                     " cached receiver elements, but cache size is only ",
                     receiver_cached_.w.size()));
  }
  ReceiverResult result;
  result.w = receiver_cached_.w.tail(size);
  result.delta = receiver_cached_.delta;
  receiver_cached_.w.conservativeResize(receiver_cached_.w.size() - size);
  return result;
}

template <typename T>
mpc_utils::StatusOr<typename DistributedVectorOLE<T>::SenderResult>
DistributedVectorOLE<T>::RunSender(int64_t size) {
  // Run bootstrapping if not already done.
  if (!sender_precomputation_done_) {
    RETURN_IF_ERROR(PrecomputeSender(size));
  }

  // Check if we have enough elements cached.
  {
    auto status = GetSenderCached(size);
    if (status.ok()) {
      return status.ValueOrDie();
    }
  }

  // If not, copy what we have and then call Expand iteratively until we have
  // enough.
  SenderResult result;
  result.u.resize(size);
  result.v.resize(size);
  int64_t num_copied = 0;
  while (num_copied < size) {
    if (num_copied + sender_cached_.u.size() <= size) {
      // Copy the entire cache if it is smaller or equal to the remaining
      // elements. This saves a copy compared to calling GetSenderCached.
      result.u.segment(num_copied, sender_cached_.u.size()) = sender_cached_.u;
      result.v.segment(num_copied, sender_cached_.v.size()) = sender_cached_.v;
      num_copied += sender_cached_.u.size();
      sender_cached_.u.resize(0);
      sender_cached_.v.resize(0);
    } else {
      // Otherwise just get as much as we need using GetSenderCached.
      int64_t remaining = size - num_copied;
      ASSIGN_OR_RETURN(SenderResult cached, GetSenderCached(remaining));
      result.u.tail(remaining) = cached.u;
      result.v.tail(remaining) = cached.v;
      num_copied = size;
    }
    if (num_copied < size) {
      // Do another expansion if necessary.
      RETURN_IF_ERROR(ExpandSender());
    }
  }

  return result;
}

template <typename T>
mpc_utils::StatusOr<typename DistributedVectorOLE<T>::ReceiverResult>
DistributedVectorOLE<T>::RunReceiver(int64_t size) {
  // Run bootstrapping if not already done.
  if (!receiver_precomputation_done_) {
    RETURN_IF_ERROR(PrecomputeReceiver(size));
  }

  // Check if we have enough elements cached.
  {
    auto status = GetReceiverCached(size);
    if (status.ok()) {
      return status.ValueOrDie();
    }
  }

  // If not, copy what we have and then call Expand iteratively until we have
  // enough.
  ReceiverResult result;
  result.w.resize(size);
  result.delta = receiver_cached_.delta;
  int64_t num_copied = 0;
  while (num_copied < size) {
    if (num_copied + receiver_cached_.w.size() <= size) {
      // Copy the entire cache if it is smaller or equal to the remaining
      // elements. This saves a copy compared to calling GetSenderCached.
      result.w.segment(num_copied, receiver_cached_.w.size()) =
          receiver_cached_.w;
      num_copied += receiver_cached_.w.size();
      receiver_cached_.w.resize(0);
    } else {
      // Otherwise just get as much as we need using GetSenderCached.
      int64_t remaining = size - num_copied;
      ASSIGN_OR_RETURN(ReceiverResult cached, GetReceiverCached(remaining));
      result.w.tail(remaining) = cached.w;
      num_copied = size;
    }
    if (num_copied < size) {
      // Do another expansion if necessary.
      RETURN_IF_ERROR(ExpandReceiver());
    }
  }

  return result;
}

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_DISTRIBUTED_VECTOR_OLE_H_
