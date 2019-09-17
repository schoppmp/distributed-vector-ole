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

#include "distributed_vector_ole/internal/ntl_helpers.h"
#include "NTL/ZZ_p.h"
#include "absl/types/optional.h"

namespace distributed_vector_ole {

// NTL moduli are saved at a thread-local scope. We do the same with the number
// of bits.
template <typename T>
int NTLNumBitsCached() {
  thread_local absl::optional<typename T::rep_type> cached_modulus;
  thread_local absl::optional<int> cached_num_bits;
  // Update mask whenever the modulus changes.
  if (!cached_num_bits || !cached_modulus || T::modulus() != *cached_modulus) {
    cached_num_bits = NTL::NumBits(T::modulus() - 1);
    cached_modulus = T::modulus();
  }
  return *cached_num_bits;
}

// Temporary variable to save allocations.
template <typename T>
T* NTLTemp() {
  thread_local absl::optional<T> temp;
  if (!temp) {
    temp = T();
  }
  return &temp.value();
}

template int NTLNumBitsCached<NTL::ZZ_p>();
template NTL::ZZ* NTLTemp<NTL::ZZ>();

}  // namespace distributed_vector_ole