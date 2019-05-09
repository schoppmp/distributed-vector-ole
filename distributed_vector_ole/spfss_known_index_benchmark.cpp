#include "benchmark/benchmark.h"
#include "distributed_vector_ole/spfss_known_index.h"
#include "mpc_utils/testing/comm_channel_test_helper.hpp"

namespace distributed_vector_ole {
namespace {

template <typename T, bool measure_communication>
static void BM_RunNative(benchmark::State &state) {
  mpc_utils::testing::CommChannelTestHelper helper(measure_communication);
  int64_t length = state.range(0);
  comm_channel *chan0 = helper.GetChannel(0);
  comm_channel *chan1 = helper.GetChannel(1);

  // Spawn a thread that acts as the server.
  NTL::ZZ_pContext ntl_context;
  ntl_context.save();
  std::thread thread1([chan1, length, &ntl_context] {
    ntl_context.restore();
    auto spfss1 = SPFSSKnownIndex::Create(chan1).ValueOrDie();
    bool keep_running;
    std::vector<T> output1(length);
    T share1(23);
    do {
      spfss1->RunServer(share1, absl::MakeSpan(output1));
      benchmark::DoNotOptimize(output1);
      chan1->recv(keep_running);
    } while (keep_running);
  });

  // Run the client in the main thread.
  auto spfss0 = SPFSSKnownIndex::Create(chan0).ValueOrDie();
  std::vector<T> output0(length);
  T share0(42);
  int index = 0;
  spfss0->RunClient(share0, index, absl::MakeSpan(output0));
  for (auto _ : state) {
    chan0->send(true);
    chan0->flush();
    spfss0->RunClient(share0, index, absl::MakeSpan(output0));
    benchmark::DoNotOptimize(output0);
    index++;
  }
  chan0->send(false);
  chan0->flush();
  thread1.join();

  int64_t bytes_sent0 = 0, bytes_sent1 = 0;
  if (measure_communication) {
    bytes_sent0 = chan0->get_num_bytes_sent();
    bytes_sent1 = chan1->get_num_bytes_sent();
  }
  state.counters["BytesSentClient"] =
      benchmark::Counter(bytes_sent0, benchmark::Counter::kAvgIterations);
  state.counters["BytesSentServer"] =
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
BENCHMARK_TEMPLATE(BM_RunNative, uint8_t, false)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNative, uint16_t, false)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNative, uint32_t, false)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNative, uint64_t, false)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNative, absl::uint128, false)->Range(1 << 12, 1 << 24);

// Timing (NTL).
BENCHMARK_TEMPLATE(BM_RunNTL, 8, false)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNTL, 16, false)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNTL, 32, false)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNTL, 64, false)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNTL, 128, false)->Range(1 << 12, 1 << 24);

// Communication (native).
BENCHMARK_TEMPLATE(BM_RunNative, uint8_t, true)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNative, uint16_t, true)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNative, uint32_t, true)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNative, uint64_t, true)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNative, absl::uint128, true)->Range(1 << 12, 1 << 24);

// Communication (NTL).
BENCHMARK_TEMPLATE(BM_RunNTL, 8, true)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNTL, 16, true)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNTL, 32, true)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNTL, 64, true)->Range(1 << 12, 1 << 24);
BENCHMARK_TEMPLATE(BM_RunNTL, 128, true)->Range(1 << 12, 1 << 24);

}  // namespace
}  // namespace distributed_vector_ole