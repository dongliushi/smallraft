#pragma once

#include <smallnet/TcpClient.h>
#include <smalljrpc/RpcClient.h>

class RaftPeer {
public:
  RaftPeer(int id, EventLoop *loop, const NetAddr &serverAddr)
      : peerId(id), client_(loop, serverAddr) {}

// private:
  int peerId;
  TcpClient client_;
};