#include "node.h"
#include <iostream>

void Node::startInLoop() {
  loop_->assertInLoopThread();
  rpcServer_.start();
  raft_->start();
  loop_->runEvery(Timer::seconds(3), [this]() { raft_->info(); });
  loop_->runEvery(Timer::milliseconds(50), [this]() { raft_->tick(); });
}
