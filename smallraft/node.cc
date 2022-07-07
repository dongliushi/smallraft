#include "node.h"
#include <iostream>

void Node::startInLoop() {
  loop_->assertInLoopThread();
  rpcServer_.start();
  raft_->start();
  loop_->runEvery(std::chrono::seconds(3), [this]() { raft_->info(); });
  loop_->runEvery(Timer::milliseconds(50), [this]() { raft_->tick(); });
  // loop_->runInLoop(std::bind(&Raft::tick, raft_));
}
