#include "raft.h"
#include <iostream>
#include <smalljrpc/Procedure.h>
#include <smalljrpc/RpcServer.h>
#include <smalljrpc/RpcService.h>

class RaftService : public RpcService {
public:
  RaftService(RpcServer &server, std::shared_ptr<Raft> raftPtr)
      : raftPtr_(raftPtr) {
    server.registerService("Raft", this);
    addProcedureReturn(
        "RequestVote",
        new Procedure(std::bind(&Raft::RequestVoteService, raftPtr,
                                std::placeholders::_1, std::placeholders::_2)));
    addProcedureReturn(
        "AppendEntries",
        new Procedure(std::bind(&Raft::AppendEntriesService, raftPtr,
                                std::placeholders::_1, std::placeholders::_2)));
  }

private:
  std::shared_ptr<Raft> raftPtr_;
};