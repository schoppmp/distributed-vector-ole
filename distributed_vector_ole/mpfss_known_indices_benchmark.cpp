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

#include "benchmark/benchmark.h"
#include "distributed_vector_ole/gf128.h"
#include "distributed_vector_ole/mpfss_known_indices.h"
#include "mpc_utils/testing/comm_channel_test_helper.hpp"

namespace distributed_vector_ole {
namespace {

int GetNumIndicesForLength(int length) {
  switch (length) {
    case 1 << 10:
      return 57;
    case 1 << 12:
      return 98;
    case 1 << 14:
      return 198;
    case 1 << 16:
      return 389;
    case 1 << 18:
      return 760;
    case 1 << 20:
      return 1419;
    case 1 << 22:
      return 2735;
  }
  return 0;
}

template <typename T, bool measure_communication>
static void BM_RunNative(benchmark::State &state) {
  mpc_utils::testing::CommChannelTestHelper helper(measure_communication);
  int64_t length = state.range(0);
  comm_channel *chan0 = helper.GetChannel(0);
  comm_channel *chan1 = helper.GetChannel(1);
  emp::initialize_relic();
  auto mpfss0 = MPFSSKnownIndices::Create(chan0).ValueOrDie();

  // Compute number of indices and VOLE correlation.
  int y_len = GetNumIndicesForLength(length);
  T x(23);
  int num_buckets = mpfss0->NumBuckets(y_len).ValueOrDie();
  Vector<T> u(num_buckets), v(num_buckets), w(num_buckets);
  ScalarHelper<T>::Randomize(absl::MakeSpan(u));
  ScalarHelper<T>::Randomize(absl::MakeSpan(v));
  w = u * x + v;

  // Spawn a thread that acts as the server.
  NTLContext<T> ntl_context;
  ntl_context.save();
  std::thread thread1([chan1, length, &ntl_context, y_len, &w, &x] {
    ntl_context.restore();
    auto mpfss1 = MPFSSKnownIndices::Create(chan1).ValueOrDie();
    bool keep_running;
    std::vector<T> output1(length);
    do {
      mpfss1->RunValueProviderVectorOLE<T>(x, y_len, w,
                                           absl::MakeSpan(output1));
      benchmark::DoNotOptimize(output1);
      chan1->recv(keep_running);
    } while (keep_running);
  });

  // Run the client in the main thread.
  std::vector<T> output0(length);
  std::vector<T> y(y_len);
  std::fill(y.begin(), y.end(), T(42));
  std::vector<int64_t> indices(y_len);
  std::iota(indices.begin(), indices.end(), 0);
  mpfss0->RunIndexProviderVectorOLE<T>(y, indices, u, v,
                                       absl::MakeSpan(output0));
  for (auto _ : state) {
    chan0->send(true);
    chan0->flush();
    mpfss0->RunIndexProviderVectorOLE<T>(y, indices, u, v,
                                         absl::MakeSpan(output0));
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
  state.counters["BytesSentSender"] =
      benchmark::Counter(bytes_sent0, benchmark::Counter::kAvgIterations);
  state.counters["BytesSentReceiver"] =
      benchmark::Counter(bytes_sent1, benchmark::Counter::kAvgIterations);
}

template <int num_bits, bool measure_communication>
static void BM_RunNTL(benchmark::State &state) {
  switch (num_bits) {
    case 8:
      NTL::ZZ_p::init(NTL::conv<NTL::ZZ>("251"));  // 2^8 - 5
      break;
    case 16:
      NTL::ZZ_p::init(NTL::conv<NTL::ZZ>("65521"));  // 2^16 - 15
      break;
    case 32:
      NTL::ZZ_p::init(NTL::conv<NTL::ZZ>("4294967291"));  // 2^32 - 5
      break;
    case 64:
      NTL::ZZ_p::init(NTL::conv<NTL::ZZ>("18446744073709551557"));  // 2^64 - 59
      break;
    case 128:
      NTL::ZZ_p::init(NTL::conv<NTL::ZZ>(
          "340282366920938463463374607431768211297"));  // 2^128 - 159
      break;
    default:
      assert(false);  // Unimplemented.
  }
  BM_RunNative<NTL::ZZ_p, measure_communication>(state);
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
BENCHMARK_TEMPLATE(BM_RunNative, gf128, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);

// Timing (NTL).
BENCHMARK_TEMPLATE(BM_RunNTL, 8, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, 16, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, 32, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, 64, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, 128, false)
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
BENCHMARK_TEMPLATE(BM_RunNative, gf128, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);

// Communication (NTL).
BENCHMARK_TEMPLATE(BM_RunNTL, 8, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, 16, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, 32, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, 64, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);
BENCHMARK_TEMPLATE(BM_RunNTL, 128, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 22);

}  // namespace
}  // namespace distributed_vector_ole