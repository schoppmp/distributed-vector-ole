#include "distributed_vector_ole/aes_uniform_bit_generator.h"

#include <random>

#include "gtest/gtest.h"
#include "openssl/rand.h"

namespace distributed_vector_ole {
namespace {

TEST(AESUniformBitGenerator, NotObviouslyBroken) {
  std::vector<uint8_t> seed(32, 0);
  RAND_bytes(seed.data(), seed.size());
  AESUniformBitGenerator rng =
      AESUniformBitGenerator::Create(seed).ValueOrDie();
  uint64_t a = rng(), b = rng();
  EXPECT_NE(a, b);
  EXPECT_NE(a, 0);
  EXPECT_NE(b, 0);
}

TEST(AESUniformBitGenerator, AllBitsGetSampled) {
  int num_samples = 80;  // Enough to be reasonably confident every bit gets set
  // to 1 at least once.
  std::vector<uint8_t> seed(32, 0);
  RAND_bytes(seed.data(), seed.size());
  AESUniformBitGenerator rng =
      AESUniformBitGenerator::Create(seed).ValueOrDie();
  uint64_t rand = 0;
  for (int i = 0; i < num_samples; i++) {
    rand |= rng();
  }
  EXPECT_EQ(rand, 0xFFFFFFFFFFFFFFFF);
}

TEST(AESUniformBitGenerator, UniformReals) {
  int num_samples = 1 << 24;
  double rand = 0.;
  std::vector<uint8_t> seed(32, 0);
  RAND_bytes(seed.data(), seed.size());
  AESUniformBitGenerator rng =
      AESUniformBitGenerator::Create(seed).ValueOrDie();
  std::uniform_real_distribution<double> dist;
  for (int i = 0; i < num_samples; i++) {
    rand += dist(rng);
  }
  EXPECT_LE(std::abs(rand / num_samples - 0.5), 0.01);
}

}  // namespace
}  // namespace distributed_vector_ole