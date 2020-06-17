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
#include <cmath>
#include "absl/memory/memory.h"
#include "openssl/err.h"

namespace distributed_vector_ole {

CuckooHasher::CuckooHasher(AES_KEY expanded_seed, int num_hash_functions,
                           double statistical_security)
    : expanded_seed_(expanded_seed),
      num_hash_functions_(num_hash_functions),
      statistical_security_(statistical_security) {}

mpc_utils::StatusOr<std::unique_ptr<CuckooHasher>> CuckooHasher::Create(
    absl::uint128 seed, int num_hash_functions, double statistical_security) {
  if (num_hash_functions <= 0) {
    return mpc_utils::InvalidArgumentError(
        "`num_hash_functions` must be positive");
  }
  // Expand seed as AES key.
  AES_KEY expanded_seed;
  if (0 != AES_set_encrypt_key(reinterpret_cast<uint8_t *>(&seed),
                               8 * sizeof(seed), &expanded_seed)) {
    return mpc_utils::InternalError(ERR_reason_error_string(ERR_get_error()));
  }
  return absl::WrapUnique(new CuckooHasher(expanded_seed, num_hash_functions,
                                           statistical_security));
}

int64_t CuckooHasher::HashToBucket(absl::uint128 hash, int64_t num_buckets,
                                   int hash_function) {
  if (hash_function > 0) {
    // Hash functions other than the first are computed by re-hashing with the
    // hash function number XOR-ed to the input.
    hash = HashToUint128(hash ^ absl::uint128(hash_function));
  }
  return absl::Uint128Low64(hash % num_buckets);
}

mpc_utils::StatusOr<int64_t> CuckooHasher::GetOptimalNumberOfBuckets(
    int64_t num_inputs) {
  if (num_inputs < 0) {
    return mpc_utils::InvalidArgumentError("`num_inputs` must be positive");
  }
  if (num_inputs == 0) {
    return 1;  // num_buckets must be positive in other CuckooHasher functions.
  }

  int stash_size =
      0;  // We curently don't support a stash, but we might in the future.
  double log_n = std::log2(num_inputs);

  // The following is based on this version of cryptoTools:
  // https://github.com/ladnir/cryptoTools/blob/85da63e335c3ad3019af3958b48d3ff6750c3d92/cryptoTools/Common/CuckooIndex.cpp#L122
  if (stash_size == 0 && num_hash_functions_ == 3) {
    const double a_max = 123.5, b_max = -130, a_sd = 2.3, b_sd = 2.18,
                 a_mean = 6.3, b_mean = 6.45;

    // Slope = 123.5 - some small terms when log_n < 12.
    const double a =
        a_max / 2 * (1 + std::erf((log_n - a_mean) / (a_sd * std::sqrt(2))));
    // y-intercept = -130 - log_n + some small terms when log_n < 12.
    const double b =
        b_max / 2 * (1 + std::erf((log_n - b_mean) / (b_sd * std::sqrt(2)))) -
        log_n;

    // We have that statistical_security_ = a e + b, where e = |cuckoo|/|set| is
    // the expansion factor. Therefore we have that
    //
    //   e = (statistical_security_ - b) / a.
    return static_cast<int64_t>(
        std::ceil((statistical_security_ - b) / a * num_inputs));
  } else if (num_hash_functions_ == 2) {
    const double a = -0.8, b = 3.3, c = 2.5, d = 14, f = 5, g = 0.65;

    // - For e > 8, statistical_security_ = (1 + 0.65 * stashSize) (b *
    //   std::log2(e) + a + nn).
    // - For e < 8, statistical_security_ -> 0 at e = 2. This is what the
    //   pow(...) does...
    auto sec = [=](double e) {
      return (1 + g * stash_size) *
             (b * std::log2(e) + a + log_n - (f * log_n + d) * std::pow(e, -c));
    };

    // Increase e util we have large enough security.
    double e = 1;
    double s = 0;
    while (s < statistical_security_) {
      e += 1;
      s = sec(e);
    }

    return static_cast<int64_t>(std::ceil(e * num_inputs));
  }
  return mpc_utils::UnimplementedError(
      "Automatic estimation of the number of buckets only implemented for 2 or "
      "3 hash functions.");
}

}  // namespace distributed_vector_ole
