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

#include <algorithm>
#include <iostream>
#include <typeinfo>
#include <vector>

#include "absl/strings/string_view.h"
#include "distributed_vector_ole/distributed_vector_ole.h"
#include "distributed_vector_ole/scalar_vector_gilboa_product.h"
#include "mpc_utils/canonical_errors.h"
#include "mpc_utils/mpc_config.hpp"
#include "mpc_utils/status.h"
#include "mpc_utils/status_macros.h"
#include "mpc_utils/statusor.h"

namespace {

// Pretty strings for type names.
// https://stackoverflow.com/a/1055563
template <typename T>
struct TypeParseTraits;

#define REGISTER_PARSE_TYPE(X) \
  template <>                  \
  struct TypeParseTraits<X> {  \
    static const char *name;   \
  };                           \
  const char *TypeParseTraits<X>::name = #X

REGISTER_PARSE_TYPE(uint32_t);
REGISTER_PARSE_TYPE(uint64_t);
REGISTER_PARSE_TYPE(absl::uint128);
REGISTER_PARSE_TYPE(NTL::zz_p);
REGISTER_PARSE_TYPE(NTL::ZZ_p);

struct ExperimentResult {
  std::string protocol_name;
  std::string type_name;
  int bit_width;
  int size;
  int num_threads;
  double time;
  bool measure_communication;
  double bytes_sent;
  double bytes_received;

  static void PrintHeaders() {
    std::cout << "protocol_name\t"
                 "value_type\t"
                 "bit_width\t"
                 "size\t"
                 "num_hreads\t"
                 "time\t"
                 "measure_communication\t"
                 "bytes_sent\t"
                 "bytes_received\n";
  }

  void Print() {
    std::cout << protocol_name << "\t" << type_name << "\t" << bit_width << "\t"
              << size << "\t" << num_threads << "\t" << time << "\t"
              << measure_communication << "\t" << bytes_sent << "\t"
              << bytes_received << "\n";
  }
};

template <typename T>
mpc_utils::Status RunVOLE(int size, mpc_utils::comm_channel *channel,
                          mpc_utils::Benchmarker *benchmarker) {
  ASSIGN_OR_RETURN(
      auto ole,
      distributed_vector_ole::DistributedVectorOLE<T>::Create(channel));
  channel->sync();
  int64_t precomputation_bytes_sent = 0, precomputation_bytes_received = 0;
  if (channel->is_measured()) {
    precomputation_bytes_sent = channel->get_num_bytes_sent();
    precomputation_bytes_received = channel->get_num_bytes_received();
  }
  auto start = benchmarker->StartTimer();
  if (channel->get_id() == 0) {
    ASSIGN_OR_RETURN(auto result, ole->RunSender(size));
  } else {
    ASSIGN_OR_RETURN(auto result, ole->RunReceiver(size));
  }
  benchmarker->AddSecondsSinceStart("time", start);
  if (channel->is_measured()) {
    benchmarker->AddAmount("bytes_sent", channel->get_num_bytes_sent() -
                                             precomputation_bytes_sent);
    benchmarker->AddAmount("bytes_received", channel->get_num_bytes_received() -
                                                 precomputation_bytes_received);
  }
  return mpc_utils::OkStatus();
}

template <typename T>
mpc_utils::Status RunGilboa(int size, mpc_utils::comm_channel *channel,
                            mpc_utils::Benchmarker *benchmarker) {
  ASSIGN_OR_RETURN(
      auto gilboa,
      distributed_vector_ole::ScalarVectorGilboaProduct::Create(channel));
  std::vector<T> input(size);
  std::iota(input.begin(), input.end(), T(42));
  channel->sync();
  int64_t precomputation_bytes_sent = 0, precomputation_bytes_received = 0;
  if (channel->is_measured()) {
    precomputation_bytes_sent = channel->get_num_bytes_sent();
    precomputation_bytes_received = channel->get_num_bytes_received();
  }
  auto start = benchmarker->StartTimer();
  if (channel->get_id() == 0) {
    ASSIGN_OR_RETURN(auto result, gilboa->RunVectorProvider<T>(input));
  } else {
    ASSIGN_OR_RETURN(auto result, gilboa->RunValueProvider<T>(T(23), size));
  }
  benchmarker->AddSecondsSinceStart("time", start);
  if (channel->is_measured()) {
    benchmarker->AddAmount("bytes_sent", channel->get_num_bytes_sent() -
                                             precomputation_bytes_sent);
    benchmarker->AddAmount("bytes_received", channel->get_num_bytes_received() -
                                                 precomputation_bytes_received);
  }
  return mpc_utils::OkStatus();
}

template <typename T>
mpc_utils::Status RunSPFSS(int size, mpc_utils::comm_channel *channel,
                           mpc_utils::Benchmarker *benchmarker) {
  ASSIGN_OR_RETURN(auto spfss,
                   distributed_vector_ole::SPFSSKnownIndex::Create(channel));
  channel->sync();
  int64_t precomputation_bytes_sent = 0, precomputation_bytes_received = 0;
  if (channel->is_measured()) {
    precomputation_bytes_sent = channel->get_num_bytes_sent();
    precomputation_bytes_received = channel->get_num_bytes_received();
  }
  auto start = benchmarker->StartTimer();
  if (channel->get_id() == 0) {
    ASSIGN_OR_RETURN(auto result, spfss->RunIndexProvider<T>(T(0), 0, size));
  } else {
    ASSIGN_OR_RETURN(auto result, spfss->RunValueProvider<T>(T(0), size));
  }
  benchmarker->AddSecondsSinceStart("time", start);
  if (channel->is_measured()) {
    benchmarker->AddAmount("bytes_sent", channel->get_num_bytes_sent() -
                                             precomputation_bytes_sent);
    benchmarker->AddAmount("bytes_received", channel->get_num_bytes_received() -
                                                 precomputation_bytes_received);
  }
  return mpc_utils::OkStatus();
}

template <typename T>
mpc_utils::StatusOr<ExperimentResult> RunExperiment(
    absl::string_view protocol_name, int size, int bit_width, int num_threads,
    bool measure_communication, mpc_utils::party *p) {
  comm_channel channel = p->connect_to(1 - p->get_id(), measure_communication);
  mpc_utils::Benchmarker benchmarker;
  omp_set_num_threads(num_threads);
  if (protocol_name == "VOLE") {
    RETURN_IF_ERROR(RunVOLE<T>(size, &channel, &benchmarker));
  } else if (protocol_name == "Gilboa") {
    RETURN_IF_ERROR(RunGilboa<T>(size, &channel, &benchmarker));
  } else if (protocol_name == "SPFSS") {
    RETURN_IF_ERROR(RunSPFSS<T>(size, &channel, &benchmarker));
  } else {
    return mpc_utils::InvalidArgumentError("Unknown protocol");
  }
  return ExperimentResult{
    protocol_name : std::string(protocol_name),
    type_name : TypeParseTraits<T>::name,
    bit_width : bit_width,
    size : size,
    num_threads : num_threads,
    time : benchmarker.Get("time"),
    measure_communication : measure_communication,
    bytes_sent : benchmarker.Get("bytes_sent"),
    bytes_received : benchmarker.Get("bytes_received"),

  };
}

mpc_utils::Status VoleVsGilboaExperiment(mpc_utils::party *p,
                                         bool measure_communication,
                                         int num_runs) {
  // Set up parameter ranges.
  std::vector<int> sizes(8);
  for (int i = 0; i < int(sizes.size()); i++) {
    sizes[i] = 1 << (10 + 2 * i);
  }
  std::vector<std::string> protocol_names = {"VOLE", "Gilboa"};

  // Vary sizes for VOLE vs. Gilboa
  for (int i = 0; i < num_runs; i++) {
    for (auto size : sizes) {
      for (const auto &protocol_name : protocol_names) {
        // 60-bit prime field.
        NTL::zz_p::init((1LL << 60) - 93);
        ASSIGN_OR_RETURN(auto result,
                         RunExperiment<NTL::zz_p>(protocol_name, size, 60, 1,
                                                  measure_communication, p));
        result.Print();

        // 32-bit prime field.
        NTL::zz_p::init((1LL << 32) - 5);
        ASSIGN_OR_RETURN(result,
                         RunExperiment<NTL::zz_p>(protocol_name, size, 32, 1,
                                                  measure_communication, p));
        result.Print();

        // 64-bit integers
        ASSIGN_OR_RETURN(result,
                         RunExperiment<uint64_t>(protocol_name, size, 64, 1,
                                                 measure_communication, p));
        result.Print();

        // 32-bit integers
        ASSIGN_OR_RETURN(result,
                         RunExperiment<uint32_t>(protocol_name, size, 32, 1,
                                                 measure_communication, p));
        result.Print();
      }
    }
  }
  return mpc_utils::OkStatus();
}

mpc_utils::Status VoleParallelismExperiment(mpc_utils::party *p, int num_runs) {
  std::vector<int> thread_nums(6);
  for (int i = 0; i < int(thread_nums.size()); i++) {
    thread_nums[i] = 1 << i;
  }

  // Vary num_threads for showing VOLE scalability.
  NTL::zz_p::init((1LL << 60) - 93);
  for (int i = 0; i < num_runs; i++) {
    for (int num_threads : thread_nums) {
      // 60-bit prime field.
      ASSIGN_OR_RETURN(
          auto result,
          RunExperiment<NTL::zz_p>("VOLE", 1 << 20, 60, num_threads, false, p));
      result.Print();
      // 64-bit integers
      ASSIGN_OR_RETURN(result, RunExperiment<uint64_t>("VOLE", 1 << 20, 64,
                                                       num_threads, false, p));
      result.Print();
    }
  }
  return mpc_utils::OkStatus();
}

mpc_utils::Status SPFSSExperiment(mpc_utils::party *p, int num_runs) {
  // Set up parameter ranges.
  std::vector<int> sizes(8);
  for (int i = 0; i < int(sizes.size()); i++) {
    sizes[i] = 1 << (10 + 2 * i);
  }
  std::vector<int> thread_nums(6);
  for (int i = 0; i < int(thread_nums.size()); i++) {
    thread_nums[i] = 1 << i;
  }

  // Vary sizes for SPFSS
  NTL::zz_p::init((1LL << 60) - 93);
  for (int i = 0; i < num_runs; i++) {
    for (auto size : sizes) {
      for (int num_threads : thread_nums) {
        // 60-bit prime field.
        ASSIGN_OR_RETURN(
            auto result,
            RunExperiment<NTL::zz_p>("SPFSS", size, 60, num_threads, false, p));
        result.Print();
      }
    }
  }
  return mpc_utils::OkStatus();
}

}  // namespace

int main(int argc, const char *argv[]) {
  mpc_config config;
  try {
    config.parse(argc, argv);
  } catch (boost::program_options::error &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
  if (config.servers.size() < 2) {
    std::cerr << "At least two servers are needed\n";
    return 1;
  }

  ExperimentResult::PrintHeaders();
  mpc_utils::party p(config);
  for (auto status : {
           VoleVsGilboaExperiment(&p, true, 1),
           VoleVsGilboaExperiment(&p, false, 10),
           VoleParallelismExperiment(&p, 10),
//           SPFSSExperiment(&p, 10),
       }) {
    if (!status.ok()) {
      std::cerr << status.message() << "\n";
      return 1;
    }
  }
  return 0;
}