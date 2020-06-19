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
#include "distributed_vector_ole/distributed_vector_ole.h"
#include "distributed_vector_ole/gf128.h"
#include "gperftools/profiler.h"
#include "mpc_utils/testing/comm_channel_test_helper.hpp"

namespace distributed_vector_ole {
namespace {

// Setup NTL modulus. Do nothing if T is not an NTL modular integer.
template <typename T, int num_bits, typename = int>
struct SetupNTLImpl {
  static void _() {}
};

// Specialization for NTL integers.
template <typename T, int num_bits>
struct SetupNTLImpl<
    T, num_bits,
    typename std::enable_if<is_modular_integer<T>::value, int>::type> {
  static void _() {
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
        T::init(NTL::conv<typename T::rep_type>(
            "1152921504606846883"));  // 2^60 - 93
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
  }
};

template <typename T, int num_bits>
void SetupNTL() {
  SetupNTLImpl<T, num_bits>::_();
}

template <typename T, bool measure_communication, int num_bits = 0>
void BM_Precompute(benchmark::State &state) {
  mpc_utils::testing::CommChannelTestHelper helper(measure_communication);
  int64_t length = state.range(0);
  comm_channel *chan0 = helper.GetChannel(0);
  comm_channel *chan1 = helper.GetChannel(1);
  emp::initialize_relic();
  int64_t bytes_sent0 = 0, bytes_sent1 = 0;
  SetupNTL<T, num_bits>();

  for (auto _ : state) {
    // Set up new VOLE instance in each iteration, but don't measure the time
    // that takes.
    state.PauseTiming();
    NTLContext<T> ntl_context;
    ntl_context.save();
    std::thread thread1([chan1, length, &ntl_context, &bytes_sent1] {
      ntl_context.restore();
      auto vole1 = DistributedVectorOLE<T>::Create(chan1).ValueOrDie();
      chan1->sync();
      benchmark::DoNotOptimize(vole1->Precompute(length));
    });

    auto vole0 = DistributedVectorOLE<T>::Create(chan0).ValueOrDie();
    chan0->sync();
    state.ResumeTiming();
    benchmark::DoNotOptimize(vole0->Precompute(length));
    thread1.join();
  }

  if (measure_communication) {
    bytes_sent0 = chan0->get_num_bytes_sent();
    bytes_sent1 = chan1->get_num_bytes_sent();
  }
  state.counters["BytesSentSender"] =
      benchmark::Counter(bytes_sent0, benchmark::Counter::kAvgIterations);
  state.counters["BytesSentReceiver"] =
      benchmark::Counter(bytes_sent1, benchmark::Counter::kAvgIterations);
}

// Timing (native).
BENCHMARK_TEMPLATE(BM_Precompute, uint8_t, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, uint16_t, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, uint32_t, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, uint64_t, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, absl::uint128, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, gf128, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);

// Timing (ZZ_p).
BENCHMARK_TEMPLATE(BM_Precompute, NTL::ZZ_p, false, 8)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::ZZ_p, false, 16)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::ZZ_p, false, 32)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::ZZ_p, false, 60)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::ZZ_p, false, 64)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::ZZ_p, false, 128)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);

// Timing (zz_p).
BENCHMARK_TEMPLATE(BM_Precompute, NTL::zz_p, false, 8)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::zz_p, false, 16)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::zz_p, false, 32)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::zz_p, false, 60)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);

// Communication (native).
BENCHMARK_TEMPLATE(BM_Precompute, uint8_t, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, uint16_t, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, uint32_t, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, uint64_t, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, absl::uint128, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, gf128, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);

// Communication (ZZ_p).
BENCHMARK_TEMPLATE(BM_Precompute, NTL::ZZ_p, true, 8)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::ZZ_p, true, 16)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::ZZ_p, true, 32)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::ZZ_p, true, 60)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::ZZ_p, true, 64)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::ZZ_p, true, 128)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);

// Communication (zz_p).
BENCHMARK_TEMPLATE(BM_Precompute, NTL::zz_p, true, 8)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::zz_p, true, 16)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::zz_p, true, 32)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Precompute, NTL::zz_p, true, 60)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);

template <typename T, bool measure_communication, int num_bits = 0>
void BM_Run(benchmark::State &state) {
  // Check if CPU profiler is enabled and stop it. We only want to profile the
  // main loop.
  struct ProfilerState profiler_state;
  ProfilerGetCurrentState(&profiler_state);
  if (profiler_state.enabled) {
    ProfilerStop();
  }

  mpc_utils::testing::CommChannelTestHelper helper(measure_communication);
  int64_t length = state.range(0);
  comm_channel *chan0 = helper.GetChannel(0);
  comm_channel *chan1 = helper.GetChannel(1);
  emp::initialize_relic();
  int64_t bytes_sent0 = 0, bytes_sent1 = 0;
  SetupNTL<T, num_bits>();

  // Spawn a thread that acts as the Receiver.
  NTLContext<T> ntl_context;
  ntl_context.save();
  std::thread thread1(
      [chan1, length, &ntl_context, &bytes_sent1, &profiler_state] {
        ntl_context.restore();
        auto vole1 = DistributedVectorOLE<T>::Create(chan1).ValueOrDie();
        vole1->Precompute(length);
        chan1->sync();
        if (measure_communication) {
          bytes_sent1 = chan1->get_num_bytes_sent();
        }
        bool keep_running;
        T x(23);
        chan1->recv(keep_running);
        do {
          auto output1 = vole1->RunReceiver(length, x).ValueOrDie();
          benchmark::DoNotOptimize(output1);
          chan1->recv(keep_running);
        } while (keep_running);
      });

  // Run the client in the main thread.
  auto vole0 = DistributedVectorOLE<T>::Create(chan0).ValueOrDie();
  vole0->Precompute(length);
  chan0->sync();
  if (measure_communication) {
    bytes_sent0 = chan0->get_num_bytes_sent();
  }

  // Re-enable profiling if it was enabled.
  if (profiler_state.enabled) {
    ProfilerStart(profiler_state.profile_name);
  }
  for (auto _ : state) {
    chan0->send(true);
    chan0->flush();
    auto output0 = vole0->RunSender(length).ValueOrDie();
    benchmark::DoNotOptimize(output0);
  }
  chan0->send(false);
  chan0->flush();
  thread1.join();

  // Count number of bytes sent, subtracting precomputation.
  if (measure_communication) {
    bytes_sent0 = chan0->get_num_bytes_sent() - bytes_sent0;
    bytes_sent1 = chan1->get_num_bytes_sent() - bytes_sent1;
  }
  state.counters["BytesSentSender"] =
      benchmark::Counter(bytes_sent0, benchmark::Counter::kAvgIterations);
  state.counters["BytesSentReceiver"] =
      benchmark::Counter(bytes_sent1, benchmark::Counter::kAvgIterations);
}

// Timing (native).
BENCHMARK_TEMPLATE(BM_Run, uint8_t, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, uint16_t, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, uint32_t, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, uint64_t, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, absl::uint128, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, gf128, false)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);

// Timing (ZZ_p).
BENCHMARK_TEMPLATE(BM_Run, NTL::ZZ_p, false, 8)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::ZZ_p, false, 16)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::ZZ_p, false, 32)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::ZZ_p, false, 60)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::ZZ_p, false, 64)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::ZZ_p, false, 128)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);

// Timing (zz_p).
BENCHMARK_TEMPLATE(BM_Run, NTL::zz_p, false, 8)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::zz_p, false, 16)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::zz_p, false, 32)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::zz_p, false, 60)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);

// Communication (native).
BENCHMARK_TEMPLATE(BM_Run, uint8_t, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, uint16_t, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, uint32_t, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, uint64_t, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, absl::uint128, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, gf128, true)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);

// Communication (ZZ_p).
BENCHMARK_TEMPLATE(BM_Run, NTL::ZZ_p, true, 8)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::ZZ_p, true, 16)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::ZZ_p, true, 32)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::ZZ_p, true, 60)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::ZZ_p, true, 64)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::ZZ_p, true, 128)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);

// Communication (zz_p).
BENCHMARK_TEMPLATE(BM_Run, NTL::zz_p, true, 8)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::zz_p, true, 16)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::zz_p, true, 32)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_Run, NTL::zz_p, true, 60)
    ->RangeMultiplier(4)
    ->Range(1 << 12, 1 << 24);

}  // namespace
}  // namespace distributed_vector_ole