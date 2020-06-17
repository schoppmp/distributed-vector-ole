// A UniformRandomBitGenerator based on AES with a fixed key. It uses an
// internal randomness buffer of user-configurable size that gets refreshed
// whenever needed. Default buffer size is 1MiB.
//
// See also:
// https://en.cppreference.com/w/cpp/numeric/random/UniformRandomBitGenerator

#ifndef DISTRIBUTED_VECTOR_OLE_AES_UNIFORM_BIT_GENERATOR_H_
#define DISTRIBUTED_VECTOR_OLE_AES_UNIFORM_BIT_GENERATOR_H_

#include <cstdint>
#include <limits>
#include <vector>
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "mpc_utils/status.h"
#include "mpc_utils/statusor.h"
#include "openssl/aes.h"

namespace distributed_vector_ole {

class AESUniformBitGenerator {
 public:
  using result_type = uint64_t;
  static const int64_t kReseedInterval = (1ll << 48);
  static const int kStateSize = 32;
  static const int kDefaultBufferSize = (1 << 20) / sizeof(result_type);

  // Creates an AESUniformBitGenerator with the given seed. `buffer_size`
  // indicates the number of cached random elements.
  static mpc_utils::StatusOr<AESUniformBitGenerator> Create(
      absl::Span<const uint8_t> seed, int64_t buffer_size = kDefaultBufferSize);

  // Reseeds the AESUniformBitGenerator.
  mpc_utils::Status seed(absl::Span<const uint8_t> seed);

  // Generates a random number.
  result_type operator()();

  static constexpr result_type min() {
    return std::numeric_limits<result_type>::min();
  }

  static constexpr result_type max() {
    return std::numeric_limits<result_type>::max();
  }

 private:
  AESUniformBitGenerator() = delete;
  AESUniformBitGenerator(int64_t buffer_size);

  // Buffer of precomputed random numbers.
  std::vector<result_type> buffer_;
  // Number of elements used from buffer_.
  int64_t elements_used_;
  // Random nonce taken from the seed.
  absl::uint128 nonce_;
  // Counts number of AES calls. seed() needs to be called whenever this exceeds
  // kReseedInterval.
  int64_t counter_;
  // Expanded AES key computed from the seed.
  AES_KEY expanded_key_;
};

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_AES_UNIFORM_BIT_GENERATOR_H_
