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

#include "NTL/ZZ_p.h"
#include "benchmark/benchmark.h"
#include "distributed_vector_ole/scalar_vector_gilboa_product.h"
#include "mpc_utils/testing/comm_channel_test_helper.hpp"

namespace distributed_vector_ole {
namespace {

template <typename T, bool measure_communication>
void BM_RunNative(benchmark::State &state) {
  mpc_utils::testing::CommChannelTestHelper helper(measure_communication);
  int64_t length = state.range(0);
  comm_channel *chan0 = helper.GetChannel(0);
  comm_channel *chan1 = helper.GetChannel(1);
  emp::initialize_relic();

  // Spawn a thread that acts as the ValueProvider.
  NTLContext<T> ntl_context;
  ntl_context.save();
  std::thread thread1([chan1, length, &ntl_context] {
    ntl_context.restore();
    auto gilboa1 = ScalarVectorGilboaProduct::Create(chan1).ValueOrDie();
    bool keep_running;
    T x(23);
    do {
      std::vector<T> output1 =
          gilboa1->RunValueProvider(x, length).ValueOrDie();
      benchmark::DoNotOptimize(output1);
      chan1->recv(keep_running);
    } while (keep_running);
  });

  auto gilboa0 = ScalarVectorGilboaProduct::Create(chan0).ValueOrDie();
  std::vector<T> y(length);
  std::iota(y.begin(), y.end(), T(0));
  std::vector<T> output0 = gilboa0->RunVectorProvider<T>(y).ValueOrDie();
  for (auto _ : state) {
    chan0->send(true);
    chan0->flush();
    output0 = gilboa0->RunVectorProvider<T>(y).ValueOrDie();
    benchmark::DoNotOptimize(output0);
  }
  chan0->send(false);
  chan0->flush();
  thread1.join();

  int64_t bytes_sent0 = 0, bytes_sent1 = 0;
  if (measure_communication) {
    bytes_sent0 = chan0->get_num_bytes_sent();
    bytes_sent1 = chan1->get_num_bytes_sent();
  }
  state.counters["BytesSentVectorProvider"] =
      benchmark::Counter(bytes_sent0, benchmark::Counter::kAvgIterations);
  state.counters["BytesSentValueProvider"] =
      benchmark::Counter(bytes_sent1, benchmark::Counter::kAvgIterations);
}

template <typename T, int num_bits, bool measure_communication>
static void BM_RunNTL(benchmark::State &state) {
  switch (num_bits) {
    case 8:
      T::init(NTL::conv<typename T::rep_type>("251"));  // 2^8 - 5
      break;
    case 16:
      T::init(NTL::conv<typename T::rep_type>("65521"));  // 2^16 - 15
      break;
    case 32:
      T::init(NTL::conv<typename T::rep_type>("4294967291"));  // 2^32 - 5
      break;
    case 60:
      T::init(
          NTL::conv<typename T::rep_type>("1152921504606846883"));  // 2^60 - 93
      break;
    case 64:
      T::init(NTL::conv<typename T::rep_type>(
          "18446744073709551557"));  // 2^64 - 59
      break;
    case 128:
      T::init(NTL::conv<typename T::rep_type>(
          "340282366920938463463374607431768211297"));  // 2^128 - 159
      break;
    default:
      assert(false);  // Unimplemented.
  }
  BM_RunNative<T, measure_communication>(state);
}

// Timing (native).
BENCHMARK_TEMPLATE(BM_RunNative, uint8_t, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNative, uint16_t, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNative, uint32_t, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNative, uint64_t, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNative, absl::uint128, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);

// Timing (ZZ_p).
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::ZZ_p, 8, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::ZZ_p, 16, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::ZZ_p, 32, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::ZZ_p, 60, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::ZZ_p, 64, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::ZZ_p, 128, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);

// Timing (zz_p).
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::zz_p, 8, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::zz_p, 16, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::zz_p, 32, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::zz_p, 60, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);

// Communication (native).
BENCHMARK_TEMPLATE(BM_RunNative, uint8_t, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNative, uint16_t, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNative, uint32_t, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNative, uint64_t, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNative, absl::uint128, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);

// Communication (ZZ_p).
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::ZZ_p, 8, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::ZZ_p, 16, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::ZZ_p, 32, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::ZZ_p, 60, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::ZZ_p, 64, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::ZZ_p, 128, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);

// Communication (zz_p).
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::zz_p, 8, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::zz_p, 16, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::zz_p, 32, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, NTL::zz_p, 60, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);

}  // namespace
}  // namespace distributed_vector_ole