#include "raft.h"
#include "raftpeer.h"
#include <smallnet/Logger.h>

using namespace smalljson;
using namespace std::placeholders;

Raft::Raft(EventLoop *loop, const Config &config)
    : id_(config.id), loop_(loop), when_(now()),
      clientLoop_(loopThread_.startLoop()) {
  peerNum_ = config.peerAddr.size();
  for (size_t i = 0; i < peerNum_; i++) {
    peerList_.emplace_back(
        new RaftPeer(i + 1, clientLoop_, config.peerAddr[i]));
  }
}

void Raft::start() {
  log_.emplace_back(Log::LogEntry());
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
}

void Raft::becomeCandidate() {
  std::unique_lock<std::mutex> lock(mutex_);
  state_ = State::Candidate;
  currentTerm_ += 1;
  votedFor_ = id_;
}

void Raft::startRequestVote() {
  RequestVoteArgs args;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    args.term = currentTerm_;
    args.candidateId = id_;
    args.lastLogIndex = log_.lastLogIndex();
    args.lastLogTerm = log_.lastLogTerm();
  }
  for (int i = 0; i < peerNum_; i++) {
    if (i + 1 != id_) {
      clientLoop_->runInLoop(
          std::bind(&RaftPeer::RequestVote, peerList_[i], args));
    }
  }
}

void Raft::tick() {
  loop_->assertInLoopThread();
  // while (true) {
    switch (state_) {
    case State::Follower:
      if (now() - when_ >= randomizedElectionTimeout_) {
        becomeCandidate();
        resetTimer();
      }
      break;
    case State::Candidate:
      if (now() - when_ >= randomizedElectionTimeout_) {
        startRequestVote();
        resetTimer();
      }
      break;
    case State::Leader:
      //   heartbeat();
      break;
    default:
      assert(false && "bad state");
    }
  // }
}

void Raft::election() {
  // if (now() - when_ >= randomizedElectionTimeout_) {
  //   becomeCandidate();
  // }
}

std::string Raft::stateString() {
  const char *stateStr[3] = {"Follower", "Candidate", "Leader"};
  return stateStr[int(state_)];
}

void Raft::info() {
  char buf[512];
  std::snprintf(buf, sizeof buf, "raft[%d] %s, term %d, #votes %d, commit %d",
                id_, stateString().c_str(), currentTerm_, votedFor_,
                commitIndex_);
  LOG_DEBUG << buf;
}

void Raft::heartbeat() {}

void Raft::RequestVoteService(Value &request, Value &response) {
  RequestVoteArgs args;
  RequestVoteReply reply;
  args.term = request["params"]["term"].to_integer();
  args.candidateId = request["params"]["candidateId"].to_integer();
  args.lastLogIndex = request["params"]["lastLogIndex"].to_integer();
  args.lastLogTerm = request["params"]["lastLogTerm"].to_integer();
  {
    std::unique_lock<std::mutex> lock(mutex_);
    RequestVote(args, reply);
  }
  response.to_object();
  response["result"]["term"] = reply.term;
  response["result"]["voteGranted"] = reply.voteGranted;
}

void Raft::AppendEntriesService(Value &request, Value &response) {
  AppendEntriesArgs args;
  AppendEntriesReply reply;
  args.term = request["params"]["term"].to_integer();
  args.leaderId = request["params"]["leaderId"].to_integer();
  args.prevLogIndex = request["params"]["prevLogIndex"].to_integer();
  args.prevLogTerm = request["params"]["prevLogTerm"].to_integer();
  Log log;
  for (auto &logentry : request["params"]["entries"].to_array()) {
    Log::LogEntry entry;
    entry.index = logentry["index"].to_integer();
    entry.term = logentry["term"].to_integer();
    entry.command = logentry["command"];
    log.emplace_back(entry);
  }
  args.entries = std::move(log);
  args.leaderCommit = request["params"]["leaderCommit"].to_integer();
  {
    std::unique_lock<std::mutex> lock(mutex_);
    AppendEntries(args, reply);
  }
  response.to_object();
  response["result"]["term"] = reply.term;
  response["result"]["success"] = reply.success;
  response["result"]["prevIndex"] = reply.prevIndex;
}

void Raft::RequestVote(const RequestVoteArgs &args, RequestVoteReply &reply) {
  reply.term = currentTerm_;
  if (args.term < currentTerm_) {
    reply.voteGranted = false;
    return;
  }
  if (args.term > currentTerm_)
    becomeFollower(args.term);
  if (votedFor_ == -1 ||
      votedFor_ == args.candidateId &&
          log_.isUpdate(args.lastLogTerm, args.lastLogIndex)) {
    state_ = State::Follower;
    votedFor_ = args.candidateId;
    reply.voteGranted = true;
  }
  reply.voteGranted = false;
}

void Raft::AppendEntries(const AppendEntriesArgs &args,
                         AppendEntriesReply &reply) {
  reply.term = currentTerm_;
  if (args.term < currentTerm_) {
    reply.success = false;
    return;
  }
  if (args.term > currentTerm_)
    becomeFollower(args.term);

  size_t currentLogLen = log_.size() - 1;
  if (args.prevLogIndex > currentLogLen ||
      log_[args.prevLogIndex].term != args.prevLogTerm) {
    if (args.prevLogIndex > currentLogLen) {
      reply.prevIndex = currentLogLen;
      reply.success = false;
      return;
    }
    log_.erase(log_.begin() + args.prevLogIndex, log_.end());
    reply.success = false;
    return;
  }
  log_.insert(log_.end(), args.entries);
  if (args.leaderCommit > commitIndex_) {
    int oldCommitIndex = commitIndex_;
    int index = log_.size() - 1;
    commitIndex_ = std::min(args.leaderCommit, index);
  }
  reply.success = true;
}

void Raft::FinishRequestVote(Value &response) {
  RequestVoteReply reply;
  reply.term = response["result"]["term"].to_integer();
  reply.voteGranted = response["result"]["voteGranted"].to_boolean();
  loop_->runInLoop(std::bind(&Raft::runInLoopRequestVote, this, reply));
}

void Raft::runInLoopRequestVote(RequestVoteReply &reply) {
  loop_->assertInLoopThread();
  LOG_DEBUG << "on_request\n";
  if (currentTerm_ < reply.term) {
    becomeFollower(reply.term);
  }
}

void Raft::FinishAppendEntries(Value &response) {
  AppendEntriesReply reply;
  reply.term = response["result"]["term"].to_integer();
  reply.success = response["result"]["success"].to_boolean();
  reply.prevIndex = response["result"]["prevIndex"].to_integer();
  loop_->runInLoop(std::bind(&Raft::runInLoopAppendEntries, this, reply));
}

void Raft::runInLoopAppendEntries(AppendEntriesReply &reply) {
  loop_->assertInLoopThread();
}

void Raft::resetTimer() {
  randomizedElectionTimeout_ = Timer::milliseconds(u(e));
  when_ = now();
}