#include "raftpeer.h"
#include "raft.h"
#include <smalljson/smalljson.h>

using namespace smalljson;
using namespace std::placeholders;

RaftPeer::RaftPeer(int id, EventLoop *loop, const NetAddr &serverAddr)
    : peerId(id), client_(loop, serverAddr) {}

void RaftPeer::addRaft(Raft *raftPtr) { raftPtr_ = raftPtr; }

void RaftPeer::RequestVote(const RequestVoteArgs &args) {
  Value request;
  request.to_object();
  request["term"] = long(args.term);
  request["candidateId"] = long(args.candidateId);
  request["lastLogIndex"] = long(args.lastLogIndex);
  request["lastLogTerm"] = long(args.lastLogTerm);
  client_.Call(request, std::bind(&Raft::FinishRequestVote, raftPtr_, _1));
}

void RaftPeer::AppendEntries(const AppendEntriesArgs &args) {
  Value request;
  request.to_object();
  request["term"] = long(args.term);
  request["leaderId"] = long(args.leaderId);
  request["prevLogIndex"] = long(args.prevLogIndex);
  request["prevLogTerm"] = long(args.prevLogTerm);
  request["entries"].to_array();
  for (int i = 0; i < args.entries.size(); i++) {
    const Log::LogEntry &logentry = args.entries[i];
    Value entry;
    entry.to_object();
    entry["index"] = long(logentry.index);
    entry["term"] = long(logentry.term);
    entry["command"] = logentry.command;
    request["entries"][i] = entry;
  }
  request["leaderCommit"] = long(args.leaderCommit);
  client_.Call(request, std::bind(&Raft::FinishAppendEntries, raftPtr_, _1));
}