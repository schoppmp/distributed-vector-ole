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

#ifndef DISTRIBUTED_VECTOR_OLE_INTERNAL_IS_MODULAR_INTEGER_H_
#define DISTRIBUTED_VECTOR_OLE_INTERNAL_IS_MODULAR_INTEGER_H_

// Helpers to generically work with multiple NTL types. Currently includes:
// - A type trait `is_modular_integer<T>` for SFINAE checks
// - int NTLNumBits<T>() to get the size of the current modulus
// - NTLTemp<T>() to get a temporary variable

#include <type_traits>
#include "NTL/ZZ_p.h"
#include "NTL/lzz_p.h"
#include "absl/meta/type_traits.h"

namespace distributed_vector_ole {

// Type trait to check if a type has a `rep_type` declared, which holds for
// NTL's modular integers.
template <typename T, typename = void>
struct is_modular_integer : std::false_type {};
template <typename T>
struct is_modular_integer<T, absl::void_t<typename T::rep_type>>
    : std::true_type {};

// Templated struct for passing moduli across threads. Does nothing if T is not
// a modular type.
template <typename T, typename = int>
struct NTLContext {
  void save() {}
  void restore() {}
};
template <typename T>
struct NTLContext<
    T, typename std::enable_if<is_modular_integer<T>::value, int>::type>
    : T::context_type {};

// Returns the number of bits of the currently installed modulus.
template <typename T,
          typename std::enable_if<is_modular_integer<T>::value, int>::type = 0>
int NTLNumBits() {
  return NTL::NumBits(T::modulus() - 1);
}
// Specialization with cached modulus. Currently only for ZZ_p.
template <typename T>
int NTLNumBitsCached();
template <>
inline int NTLNumBits<NTL::ZZ_p>() {
  return NTLNumBitsCached<NTL::ZZ_p>();
}

// Returns a pointer to a thread_local temporary NTL:ZZ. Avoids memory
// allocations.
template <typename T>
T* NTLTemp();

}  // namespace distributed_vector_ole

#endif  // DISTRIBUTED_VECTOR_OLE_INTERNAL_IS_MODULAR_INTEGER_H_
