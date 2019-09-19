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

#include <cstdint>
#include <iostream>
#include "distributed_vector_ole/distributed_vector_ole.h"
#include "mpc_utils/comm_channel.hpp"
#include "mpc_utils/config.hpp"

// A very minimal example for distributed Vector-OLE generation.
// Can be called this way:
//
// $ bazel run //:example -- 0 & bazel run //:example -- 1
int main(int argc, char* argv[]) {
  // Parse command line and check usage.
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << "[0|1]\n";
    return 2;
  }
  int party_id = -1;
  if (!strcmp("0", argv[1])) {
    party_id = 0;
  } else if (!strcmp("1", argv[1])) {
    party_id = 1;
  } else {
    std::cerr << "Usage: " << argv[0] << "[0|1]\n";
    return 2;
  }

  // Create configuration for two servers.
  mpc_config config;
  config.servers = {
      server_info("127.0.0.1", 13141),
      server_info("127.0.0.1", 15926),
  };
  config.party_id = party_id;

  // Connect both parties.
  mpc_utils::party p(config);
  mpc_utils::comm_channel channel = p.connect_to(1 - party_id);

  // Create VOLE generator. We use 16-bit integers here because they can be
  // nicely printed and compared, but everything works with NTL modular integers
  // (ZZ_p, zz_p) as well.
  using T = uint16_t;
  auto status =
      distributed_vector_ole::DistributedVectorOLE<T>::Create(&channel);
  if (!status.ok()) {
    std::cerr << "Error creating VOLE generator: " << status.status().message()
              << "\n";
    return 1;
  }
  auto ole = std::move(status.ValueOrDie());

  // Run VOLE generator and print vectors.
  int size = 200000;
  if (party_id == 0) {
    // Party 0 has no inputs and gets two pseudorandom vectors.
    auto status2 = ole->RunSender(size);
    if (!status.ok()) {
      std::cerr << "Error running VOLE sender: " << status.status().message()
                << "\n";
      return 1;
    }
    distributed_vector_ole::Vector<T> u, v;
    std::tie(u, v) = std::move(status2.ValueOrDie());
    std::cout << "u = " << u.head(10) << " ...\n";
    std::cout << "v = " << v.head(10) << " ...\n";
  } else {
    // Party 1 inputs x and receives one pseudorandom vector.
    T x(23);
    auto status2 = ole->RunReceiver(size, x);
    if (!status.ok()) {
      std::cerr << "Error running VOLE receiver: " << status.status().message()
                << "\n";
      return 1;
    }
    distributed_vector_ole::Vector<T> w = std::move(status2.ValueOrDie());
    std::cout << "x = " << x << "\n";
    std::cout << "w = " << w.head(10) << " ...\n";
  }
  return 0;
}