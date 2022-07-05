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
  request["lastLogIndex"] = args.lastLogIndex;
  request["lastLogTerm"] = args.lastLogTerm;
  client_.Call("Raft.RequestVote", request,
               std::bind(&Raft::FinishRequestVote, raftPtr_, _1));
}

void RaftPeer::AppendEntries(const AppendEntriesArgs &args) {
  if (!client_.isConnected()) {
    return;
  }
  Value request;
  request.to_object();
  request["term"] = args.term;
  request["leaderId"] = args.leaderId;
  request["prevLogIndex"] = args.prevLogIndex;
  request["prevLogTerm"] = args.prevLogTerm;
  request["entries"].to_array();
  for (int i = 0; i < args.entries.size(); i++) {
    const Log::LogEntry &logentry = args.entries[i];
    Value entry;
    entry.to_object();
    entry["index"] = logentry.index;
    entry["term"] = logentry.term;
    entry["command"] = logentry.command;
    request["entries"][i] = entry;
  }
  request["leaderCommit"] = args.leaderCommit;
  client_.Call("Raft.AppendEntries", request,
               std::bind(&Raft::FinishAppendEntries, raftPtr_, _1));
}