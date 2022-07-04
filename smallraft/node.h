#pragma once

#include "config.h"
#include "raft.h"
#include "raftpeer.h"
#include "raftservice.h"
#include <smalljrpc/RpcServer.h>
#include <smallnet/EventLoop.h>
#include <smallnet/EventLoopThread.h>

class Node {
  typedef std::shared_ptr<Raft> RaftPtr;

public:
  Node(EventLoop *rpcLoop, const Config &c)
      : loop_(loopThread_.startLoop()), rpcServer_(rpcLoop, c.serverAddr),
        raft_(new Raft(loop_, c)), raftService_(rpcServer_, raft_) {}
          
  void start() { loop_->runInLoop(std::bind(&Node::startInLoop, this)); }

private:
  void startInLoop();
  EventLoopThread loopThread_;
  EventLoop *loop_;
  RaftPtr raft_;
  RpcServer rpcServer_;
  RaftService raftService_;
  std::chrono::milliseconds tickInterval_;
};
