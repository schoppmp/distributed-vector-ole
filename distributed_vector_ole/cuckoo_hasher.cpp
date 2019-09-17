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
#include "absl/memory/memory.h"

namespace distributed_vector_ole {

CuckooHasher::CuckooHasher(std::vector<SHA256_CTX> hash_states)
    : hash_states_(std::move(hash_states)) {}

mpc_utils::StatusOr<std::unique_ptr<CuckooHasher>> CuckooHasher::Create(
    absl::string_view seed, int num_hash_functions) {
  if (num_hash_functions <= 0) {
    return mpc_utils::InvalidArgumentError(
        "`num_hash_functions` must be positive");
  }
  std::vector<SHA256_CTX> hash_states(num_hash_functions);
  for (int64_t i = 0; i < num_hash_functions; i++) {
    SHA256_Init(&hash_states[i]);
    UpdateHash(i, &hash_states[i]);
    UpdateHash(seed, &hash_states[i]);
  }
  return absl::WrapUnique(new CuckooHasher(std::move(hash_states)));
}

}  // namespace distributed_vector_ole
