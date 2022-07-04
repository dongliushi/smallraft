#pragma once

#include "smallnet/NetAddr.h"
#include <vector>

struct Config {
  int id;
  NetAddr serverAddr;
  std::vector<NetAddr> peerAddr;
};