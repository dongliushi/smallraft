#include "raftservice.h"

using namespace smalljson;

RaftService::RaftService(RpcServer &server, std::shared_ptr<Raft> raftPtr)
    : raftPtr_(raftPtr) {
  server.registerService("Raft", this);
  addProcedureReturn(
      "RequestVote",
      new Procedure(std::bind(&RaftService::RequestVote, this,
                              std::placeholders::_1, std::placeholders::_2)));
  addProcedureReturn(
      "AppendEntries",
      new Procedure(std::bind(&RaftService::AppendEntries, this,
                              std::placeholders::_1, std::placeholders::_2)));
}

void RaftService::RequestVote(Value &request, Value &response) {
  RequestVoteArgs args;
  RequestVoteReply reply;
  args.term = request["params"]["term"].to_integer();
  args.candidateId = request["params"]["candidateId"].to_integer();
  args.lastLogIndex = request["params"]["lastLogIndex"].to_integer();
  args.lastLogTerm = request["params"]["lastLogTerm"].to_integer();
  raftPtr_->RequestVote(args, reply);
  response["term"] = reply.term;
  response["voteGranted"] = reply.voteGranted;
}
void RaftService::AppendEntries(Value &request, Value &response) {
  AppendEntriesArgs args;
  AppendEntriesReply reply;
  args.term = request["params"]["term"].to_integer();
  args.leaderId = request["params"]["leaderId"].to_integer();
  args.prevLogIndex = request["params"]["prevLogIndex"].to_integer();
  args.prevLogTerm = request["params"]["prevLogTerm"].to_integer();
  Array &entries = request["params"]["entries"].to_array();
  Raft::Log log;
  for (int i = 0; i < entries.size(); i++) {
    LogEntry entry;
    entry.index = entries[i]["index"].to_integer();
    entry.term = entries[i]["term"].to_integer();
    entry.command = entries[i]["command"];
    log.emplace_back(entry);
  }
  args.entries = std::move(log);
  args.leaderCommit = request["params"]["leaderCommit"].to_integer();
  raftPtr_->AppendEntries(args, reply);
  response.to_object();
  response["term"] = reply.term;
  response["success"] = reply.success;
//   response["prevIndex"] = reply.prevIndex;
}