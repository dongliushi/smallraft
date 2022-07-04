#include "raft.h"
#include <iostream>
void Raft::becomeFollower(int term) {
  state_ = State::Follower;
  currentTerm_ = term;
  votedFor_ = -1;
}

void Raft::becomeCandidate() {
  state_ = State::Candidate;
  currentTerm_ += 1;
  votedFor_ = id_;
  startRequestVote();
}

void Raft::startRequestVote() {
  RequestVoteArgs args;
  args.term = currentTerm_;
  args.candidateId = id_;
  args.lastLogIndex = log_.lastLogIndex();
  args.lastLogTerm = log_.lastLogTerm();
  for (int i = 0; i < peerNum_; i++) {
    if (i != id_) {
      // peerList_[i] ->RequestVote(args);
    }
  }
}

void Raft::tick() {
  switch (state_) {
  case State::Follower:
  case State::Candidate:
    election();
    break;
  case State::Leader:
    heartbeat();
    break;
  default:
    assert(false && "bad role");
  }
}

void Raft::election() {
  // if (::now() - when_ >= randomizedElectionTimeout_) {
  //   becomeCandidate();
  // }
}

void Raft::heartbeat() {}

void Raft::RequestVoteService(smalljson::Value &request,
                              smalljson::Value &response) {
  RequestVoteArgs args;
  RequestVoteReply reply;
  args.term = request["params"]["term"].to_integer();
  args.candidateId = request["params"]["candidateId"].to_integer();
  args.lastLogIndex = request["params"]["lastLogIndex"].to_integer();
  args.lastLogTerm = request["params"]["lastLogTerm"].to_integer();
  RequestVote(args, reply);
  response.to_object();
  response["result"]["term"] = long(reply.term);
  response["result"]["voteGranted"] = reply.voteGranted;
}

void Raft::AppendEntriesService(smalljson::Value &request,
                                smalljson::Value &response) {
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
  }
  args.entries = std::move(log);
  args.leaderCommit = request["params"]["leaderCommit"].to_integer();
  AppendEntries(args, reply);
  response.to_object();
  response["result"]["term"] = long(reply.term);
  response["result"]["success"] = reply.success;
  response["result"]["prevIndex"] = long(reply.prevIndex);
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
  if (args.leaderCommit > commitIndex) {
    int oldCommitIndex = commitIndex;
    int index = log_.size() - 1;
    commitIndex = std::min(args.leaderCommit, index);
  }
  reply.success = true;
}