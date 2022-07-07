#include "raftpeer.h"
#include "raft.h"
#include <iostream>
#include <smalljson/smalljson.h>
#include <smallnet/Logger.h>

using namespace smalljson;
using namespace std::placeholders;

RaftPeer::RaftPeer(int id, EventLoop *loop, const NetAddr &serverAddr)
    : peerId(id), client_(loop, serverAddr) {}

void RaftPeer::addRaft(Raft *raftPtr) { raftPtr_ = raftPtr; }

void RaftPeer::RequestVote(const RequestVoteArgs &args) {
  if (!client_.isConnected()) {
    return;
  }
  Value request;
  request.to_object();
  request["term"] = args.term;
  request["candidateId"] = args.candidateId;
  // request["lastLogIndex"] = args.lastLogIndex;
  // request["lastLogTerm"] = args.lastLogTerm;
  client_.Call("Raft.RequestVote", request,
               std::bind(&RaftPeer::FinishRequestVote, this, _1));
}

void RaftPeer::FinishRequestVote(Value &response) {
  RequestVoteReply reply;
  reply.term = response["result"]["term"].to_integer();
  reply.voteGranted = response["result"]["voteGranted"].to_boolean();
  raftPtr_->FinishRequestVote(reply);
}

void RaftPeer::AppendEntries(const AppendEntriesArgs &args) {
  if (!client_.isConnected()) {
    return;
  }
  Value request;
  request.to_object();
  request["term"] = args.term;
  request["leaderId"] = args.leaderId;
  // request["prevLogIndex"] = args.prevLogIndex;
  // request["prevLogTerm"] = args.prevLogTerm;
  // request["entries"].to_array();
  // Array entries;
  // for (int i = 0; i < args.entries.size(); i++) {
  //   const Log::LogEntry &logentry = args.entries[i];
  //   Value entry;
  //   entry.to_object();
  //   entry["index"] = logentry.index;
  //   entry["term"] = logentry.term;
  //   entry["command"] = logentry.command;
  //   entries.emplace_back(entry);
  // }
  // request["entries"] = entries;
  // request["leaderCommit"] = args.leaderCommit;
  // LOG_DEBUG << request.to_print();
  // LOG_DEBUG << "SEND_APPENDENTRIES";
  client_.Call("Raft.AppendEntries", request,
               std::bind(&RaftPeer::FinishAppendEntries, this, _1));
}
void RaftPeer::FinishAppendEntries(smalljson::Value &response) {
  AppendEntriesReply reply;
  reply.term = response["result"]["term"].to_integer();
  reply.success = response["result"]["success"].to_boolean();
  // reply.prevIndex = response["result"]["prevIndex"].to_integer();
  raftPtr_->FinishAppendEntries(reply);
}
