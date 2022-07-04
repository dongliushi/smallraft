#include "node.h"

void Node::startInLoop() {
  loop_->assertInLoopThread();
  rpcServer_.start();
  raft_->start();
  loop_->runInLoop([this]{raft_->tick();});
}
