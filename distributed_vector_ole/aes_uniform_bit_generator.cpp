#include "distributed_vector_ole/aes_uniform_bit_generator.h"
#include <stdexcept>
#include "absl/strings/str_cat.h"
#include "mpc_utils/canonical_errors.h"
#include "mpc_utils/status_macros.h"
#include "openssl/err.h"

namespace distributed_vector_ole {

AESUniformBitGenerator::AESUniformBitGenerator(int64_t buffer_size)
    : buffer_(buffer_size),  // Make sure buffer size is even.
      elements_used_(
          buffer_size)  // All elements are marked as used in the beginning.
{}

mpc_utils::StatusOr<AESUniformBitGenerator> AESUniformBitGenerator::Create(
    absl::Span<const uint8_t> seed, int64_t buffer_size) {
  if (buffer_size < 2) {
    buffer_size = 2;
  }
  AESUniformBitGenerator result(buffer_size);
  RETURN_IF_ERROR(result.seed(seed));
  return result;
}

mpc_utils::Status AESUniformBitGenerator::seed(absl::Span<const uint8_t> seed) {
  if (seed.size() < kStateSize) {
    return mpc_utils::InvalidArgumentError(
        absl::StrCat("`seed` must be at least ", kStateSize, " bytes long"));
  }
  if (0 != AES_set_encrypt_key(
               reinterpret_cast<const uint8_t *>(seed.data()), 128,
               &expanded_key_)) {  // Use first 16 bytes of seed as key.
    return mpc_utils::InternalError(ERR_reason_error_string(ERR_get_error()));
  }
  nonce_ = *(reinterpret_cast<const absl::uint128 *>(
      seed.data() + 16));  // Use second 16 bytes of seed as nonce.
  counter_ = 0;            // Reset counter.
  elements_used_ = static_cast<int64_t>(buffer_.size());  // Reset buffer.
  return mpc_utils::OkStatus();
}

AESUniformBitGenerator::result_type AESUniformBitGenerator::operator()() {
  int64_t buffer_size = static_cast<int64_t>(buffer_.size());
  if (elements_used_ >= buffer_size) {
    // Re-randomize the entire buffer using AES in counter mode. We use a
    // separate vector for the nonces which allows the CPU to pipeline calls to
    // AES_encrypt.
    int64_t num_blocks = buffer_size / 2;
    std::vector<absl::uint128> nonces(num_blocks);
    for (int64_t i = 0; i < num_blocks; i++) {
      nonces[i] = nonce_;
      nonce_++;
    }
    for (int64_t i = 0; i < num_blocks; i++) {
      AES_encrypt(reinterpret_cast<const uint8_t *>(&nonces[i]),
                  reinterpret_cast<uint8_t *>(&buffer_[2 * i]), &expanded_key_);
    }
    elements_used_ = 0;
    counter_ += num_blocks;
    // Check if we exceeded the reseed interval. Since the return type of
    // operator() is required to be result_type, we have to throw an exception
    // here.
    if (counter_ > kReseedInterval) {
      throw std::runtime_error(
          absl::StrCat("`seed()` must be called every ", 2 * kReseedInterval,
                       " calls to this AESUniformBitGenerator"));
    }
  }
  result_type result = buffer_[elements_used_];
  elements_used_++;
  return result;
}

}  // namespace distributed_vector_ole