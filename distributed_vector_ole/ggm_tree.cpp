#include "ggm_tree.h"
#include <cmath>
#include "openssl/rand.h"
#include <cassert>
#include <algorithm>

namespace distributed_vector_ole {

GGMTree::GGMTree(int arity, int64_t num_leaves, absl::string_view seed)
    : arity_(arity),
      num_leaves_(num_leaves),
      num_levels_(static_cast<int>(
          std::ceil(std::log(num_leaves_) / std::log(arity_)))),
      levels_(num_levels_),
      keys_(arity_, std::vector<uint8_t>(kBlockSize)),
      expanded_keys_(arity_) {
  // Generate keys.
  for (int i = 1; i < arity_; i++) {
    RAND_bytes(keys_[i].data(), keys_[i].size());
    AES_set_encrypt_key(keys_[i].data(), 8 * kBlockSize, &expanded_keys_[i]);
  }

  // Allocate memory for levels.
  int64_t level_size;
  for (int i = num_levels_ - 1; i >= 0; i--) {
    if (i == num_levels_ - 1) {
      level_size = num_leaves_;
    } else {
      level_size = (level_size + arity_ - 1) / arity_;
    }
    levels_[i] = std::vector<uint8_t>(kBlockSize * level_size);
  }

  // Copy seed and expand.
  std::copy_n(seed.begin(), std::min(seed.size(), levels_[0].size()), levels_[0].begin());
  ExpandFrom(0, 0);
}

void GGMTree::ExpandFrom(int level, int64_t node) {
  // TODO.
}

}  // namespace distributed_vector_ole
