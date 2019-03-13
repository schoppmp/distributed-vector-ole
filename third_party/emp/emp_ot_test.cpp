#include <iostream>
#include <random>
#include "boost/thread.hpp"
#include "boost/thread/thread_guard.hpp"
#include "gtest/gtest.h"
#include "test/test.h"

namespace {

void RunParty(int party, int port) {
  int length = 1 << 20;
  NetIO io(party == ALICE ? nullptr : "127.0.0.1", port);
  std::cout << "NPOT\t"
            << 10000.0 / test_ot<NetIO, OTNP>(&io, party, 10000) * 1e6
            << " OTps" << endl;
  std::cout << "Semi Honest OT Extension\t"
            << double(length) /
                   test_ot<NetIO, SHOTExtension>(&io, party, length) * 1e6
            << " OTps" << endl;
  std::cout << "Semi Honest COT Extension\t"
            << double(length) /
                   test_cot<NetIO, SHOTExtension>(&io, party, length) * 1e6
            << " OTps" << endl;
  std::cout << "Semi Honest ROT Extension\t"
            << double(length) /
                   test_rot<NetIO, SHOTExtension>(&io, party, length) * 1e6
            << " OTps" << endl;
}

}  // namespace

TEST(EmpOt, TestOt) {
  initialize_relic();
  std::random_device rd;
  std::uniform_int_distribution<> dis(1025,
                                      std::numeric_limits<uint16_t>::max());
  int port = dis(rd);
  boost::thread thread1([port] { RunParty(ALICE, port); });
  boost::thread_guard<> g(thread1);
  RunParty(BOB, port);
}