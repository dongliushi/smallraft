#pragma once

#include <smalljrpc/RpcClient.h>
#include <smallnet/TcpClient.h>

struct RequestVoteArgs;
struct AppendEntriesArgs;
class Raft;

class RaftPeer {
public:
  RaftPeer(int id, EventLoop *loop, const NetAddr &serverAddr);
  void RequestVote(const RequestVoteArgs &args);
  void FinishRequestVote(const RequestVoteArgs &args,
                         smalljson::Value &response);
  void AppendEntries(const AppendEntriesArgs &args);
  void FinishAppendEntries(const AppendEntriesArgs &args,
                           smalljson::Value &response);
  void start() { client_.start(); }
  void addRaft(Raft *raftPtr);

private:
  int peerId;
  Raft *raftPtr_;
  RpcClient client_;
};