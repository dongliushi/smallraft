#include "raft.h"
#include "raftpeer.h"
#include <smallnet/Logger.h>

using namespace smalljson;
using namespace std::placeholders;

Raft::Raft(EventLoop *loop, const Config &config)
    : loop_(loop), when_(now()), clientLoop_(loopThread_.startLoop()),
      id_(config.id), peerNum_(config.peerAddr.size()) {
  for (size_t i = 0; i < peerNum_; i++) {
    peerList_.emplace_back(
        new RaftPeer(i + 1, clientLoop_, config.peerAddr[i]));
  }
}

void Raft::start() {
  logs_.emplace_back(LogEntry());
  for (int i = 0; i < peerNum_; i++) {
    peerList_[i]->addRaft(this);
    if (i + 1 != id_) {
      peerList_[i]->start();
    }
  }
}

void Raft::becomeFollower(int term) {
  state_ = State::Follower;
  currentTerm_ = term;
  votedFor_ = -1;
  voteCount_ = 0;
  resetTimer();
}

void Raft::becomeCandidate() {
  state_ = State::Candidate;
  currentTerm_ += 1;
  votedFor_ = id_;
  voteCount_ = 1;
}

void Raft::becomeLeader() {
  state_ = State::Leader;
  nextIndex_.assign(peerNum_, (logs_.end() - 1)->index + 1);
  matchIndex_.assign(peerNum_, 0);
  resetTimer();
}

void Raft::tick() {
  loop_->assertInLoopThread();
  switch (state_) {
  case State::Follower:
  case State::Candidate:
    if (now() - when_ >= randomizedElectionTimeout_) {
      becomeCandidate();
      resetTimer();
      startRequestVote();
    }
    break;
  case State::Leader:
    heartbeat();
    break;
  default:
    assert(false && "bad state");
  }
}

void Raft::startRequestVote() {
  RequestVoteArgs args;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    args.term = currentTerm_;
    args.candidateId = id_;
    args.lastLogIndex = (logs_.end() - 1)->index;
    args.lastLogTerm = (logs_.end() - 1)->term;
  }
  for (int i = 0; i < peerNum_; i++) {
    if (i + 1 != id_) {
      clientLoop_->runInLoop(
          std::bind(&RaftPeer::RequestVote, peerList_[i], args));
    }
  }
}

void Raft::startAppendEntries() {
  for (int i = 0; i < peerNum_; i++) {
    if (i + 1 == id_)
      continue;
    AppendEntriesArgs args;
    args.term = currentTerm_;
    args.prevLogIndex = nextIndex_[i] - 1;
    args.prevLogTerm = logs_[args.prevLogIndex].term;
    args.entries = {logs_.begin() + nextIndex_[i], logs_.end()};
    args.leaderCommit = commitIndex_;
    clientLoop_->runInLoop(
        std::bind(&RaftPeer::AppendEntries, peerList_[i], args));
  }
}

std::string Raft::stateString() {
  const char *stateStr[3] = {"Follower", "Candidate", "Leader"};
  return stateStr[int(state_)];
}

void Raft::info() {
  char buf[512];
  std::snprintf(buf, sizeof buf,
                "raft[%d] %s, term %d, #votes %d, commit %d,#granted %d", id_,
                stateString().c_str(), currentTerm_, votedFor_, commitIndex_,
                voteCount_);
  LOG_DEBUG << buf;
}

void Raft::heartbeat() {
  if (now() - when_ >= heartbeatTimeout_) {
    resetTimer();
    startAppendEntries();
  }
}

void Raft::RequestVote(const RequestVoteArgs &args, RequestVoteReply &reply) {
  reply.term = currentTerm_;
  if (args.term < currentTerm_) {
    reply.voteGranted = false;
    return;
  }
  if (args.term > currentTerm_)
    becomeFollower(args.term);
  if (votedFor_ == -1 || votedFor_ == args.candidateId) {
    if (args.lastLogIndex > logs_[args.lastLogIndex].index ||
        (args.lastLogIndex == logs_[args.lastLogIndex].index &&
         args.lastLogTerm == logs_[args.lastLogIndex].term)) {
      state_ = State::Follower;
      votedFor_ = args.candidateId;
      reply.voteGranted = true;
      resetTimer();
      return;
    }
  }
  reply.voteGranted = false;
  return;
}

void Raft::AppendEntries(const AppendEntriesArgs &args,
                         AppendEntriesReply &reply) {
  reply.term = currentTerm_;
  if (args.term < currentTerm_) {
    reply.success = false;
    return;
  }
  if (args.term > currentTerm_) {
    becomeFollower(args.term);
  }
  reply.success = true;
  resetTimer();
  if (args.term == currentTerm_) {
    if (state_ != State::Follower)
      becomeFollower(args.term);
    reply.success = true;
  }
  return;
  /*
    // size_t currentLogLen = log_.size() - 1;
    // if (args.prevLogIndex > currentLogLen ||
    //     log_[args.prevLogIndex].term != args.prevLogTerm) {
    //   if (args.prevLogIndex > currentLogLen) {
    //     reply.prevIndex = currentLogLen;
    //     reply.success = false;
    //     return;
    //   }
    //   log_.erase(log_.begin() + args.prevLogIndex, log_.end());
    //   reply.success = false;
    //   return;
    // }
    // log_.insert(log_.end(), args.entries);
    // if (args.leaderCommit > commitIndex_) {
    //   int oldCommitIndex = commitIndex_;
    //   int index = log_.size() - 1;
    //   commitIndex_ = std::min(args.leaderCommit, index);
    // }
    // reply.success = true;
    */
}

void Raft::FinishRequestVote(RequestVoteReply &reply) {
  loop_->assertInLoopThread();
  if (state_ != State::Candidate)
    return;
  if (currentTerm_ < reply.term) {
    becomeFollower(reply.term);
    return;
  }
  if (reply.voteGranted) {
    voteCount_++;
    if (voteCount_ > peerNum_ / 2) {
      becomeLeader();
    }
  }
}

void Raft::FinishAppendEntries(AppendEntriesReply &reply) {
  loop_->assertInLoopThread();
  if (currentTerm_ < reply.term) {
    becomeFollower(reply.term);
    return;
  }
  if (state_ != State::Leader || currentTerm_ > reply.term) {
    return;
  }
  // if (!reply.success) {
  // }
}

void Raft::resetTimer() {
  randomizedElectionTimeout_ = Timer::milliseconds(u(e));
  when_ = now();
}