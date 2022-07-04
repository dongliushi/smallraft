#include "node.h"

void Node::startInLoop(){
    loop_->assertInLoopThread();
    rpcServer_.start();
}