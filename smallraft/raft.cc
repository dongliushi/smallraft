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
  reply.prevIndex = args.prevLogIndex; // 返回给leader用于更新nextIndex
  if (args.term < currentTerm_) {
    reply.success = false;
    return;
  }
  if (args.term > currentTerm_) {
    becomeFollower(args.term);
  }
  resetTimer();
  size_t currentLogLen = logs_.size() - 1;
  if (args.prevLogIndex > currentLogLen ||
      logs_[args.prevLogIndex].term != args.prevLogTerm) {
    reply.success = false;
    if (args.prevLogIndex > currentLogLen) {
      reply.prevIndex = currentLogLen + 1;
      return;
    }
    logs_.erase(logs_.begin() + args.prevLogIndex, logs_.end());
    return;
  }
  for (int i = 0; i < args.entries.size(); i++) {
    logs_.emplace_back(args.entries[i]);
  }
  reply.success = true;
  if (args.leaderCommit > commitIndex_) {
    int oldCommitIndex = commitIndex_;
    int index = logs_.size() - 1;
    commitIndex_ = std::min(args.leaderCommit, index);
    // go apply(); to_do
  }
}

void Raft::FinishRequestVote(int peerId, const RequestVoteArgs &args,
                             const RequestVoteReply &reply) {
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

void Raft::FinishAppendEntries(int peerId, const AppendEntriesArgs &args,
                               const AppendEntriesReply &reply) {
  loop_->assertInLoopThread();
  if (currentTerm_ < reply.term) {
    becomeFollower(reply.term);
    return;
  }
  if (state_ != State::Leader || currentTerm_ > reply.term) {
    return;
  }
  if (!reply.success) {
    nextIndex_[peerId - 1] = reply.prevIndex;
  } else {
    nextIndex_[peerId - 1] = args.prevLogIndex + args.entries.size() + 1;
    matchIndex_[peerId - 1] = args.prevLogIndex + args.entries.size();
    auto save_matchIndex = matchIndex_;
    for (int i = save_matchIndex[peerId - 1]; i >= args.prevLogIndex + 1; i--) {
      if (logs_[i].term < currentTerm_ || i <= commitIndex_) {
        break;
      }
      int replication = 0;
      for (int j = 0; j < peerNum_; j++) {
        if (i <= save_matchIndex[j]) {
          replication++;
        }
      }
      if (replication * 2 > peerNum_) {
        commitIndex_ = i;
        break;
      }
    }
  }
}

void Raft::provideCommand(const smalljson::Value &command) {
  int index = (logs_.end() - 1)->index + 1;
  int currentTerm = currentTerm_;
  bool isLeader = (state_ == State::Leader);
  LogEntry entry(index, currentTerm, command);
  if (isLeader) {
    logs_.emplace_back(std::move(entry));
  }
}

void Raft::resetTimer() {
  randomizedElectionTimeout_ = Timer::milliseconds(u(e));
  when_ = now();
}