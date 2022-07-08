#include "raft.h"
#include <smalljrpc/Procedure.h>
#include <smalljrpc/RpcServer.h>
#include <smalljrpc/RpcService.h>

class RaftService : public RpcService {
public:
  RaftService(RpcServer &server, std::shared_ptr<Raft> raftPtr);

private:
  void RequestVote(smalljson::Value &request, smalljson::Value &response);
  void AppendEntries(smalljson::Value &request, smalljson::Value &response);

private:
  std::shared_ptr<Raft> raftPtr_;
};