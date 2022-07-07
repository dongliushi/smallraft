#pragma once

#include "config.h"
#include "log.h"
#include <mutex>
#include <random>
#include <smalljrpc/Procedure.h>
#include <smalljrpc/RpcServer.h>
#include <smalljrpc/RpcService.h>
#include <smalljson/smalljson.h>
#include <smallnet/EventLoopThread.h>
#include <smallnet/Timer.h>
#include <string>
#include <vector>

struct RequestVoteArgs;
struct RequestVoteReply;
struct AppendEntriesArgs;
struct AppendEntriesReply;
class RaftPeer;

class Raft : public std::enable_shared_from_this<Raft> {
public:
  enum class State { Follower, Candidate, Leader };
  typedef std::shared_ptr<RaftPeer> RaftPeerPtr;
  friend class RaftService;
  friend class RaftPeer;

public:
  Raft() = delete;
  Raft(EventLoop *loop, const Config &config);
  void start();
  State state() const { return state_; }
  void tick();
  void heartbeat();
  void info();

private:
  std::string stateString();
  void resetTimer();
  void RequestVote(const RequestVoteArgs &args, RequestVoteReply &reply);
  void FinishRequestVote(RequestVoteReply &reply);

  void AppendEntries(const AppendEntriesArgs &args, AppendEntriesReply &reply);
  void FinishAppendEntries(AppendEntriesReply &reply);

  void startRequestVote();
  void startAppendEntries();
  void becomeFollower(int term);
  void becomeCandidate();
  void becomeLeader();

private:
  std::uniform_int_distribution<int> u{1000, 1400};
  std::default_random_engine e;
  Timer::milliseconds randomizedElectionTimeout_ = Timer::milliseconds(u(e));
  Timer::milliseconds heartbeatTimeout_ = Timer::milliseconds(150);
  EventLoopThread loopThread_;
  EventLoop *clientLoop_;
  EventLoop *loop_;
  std::vector<RaftPeerPtr> peerList_;
  std::mutex mutex_;
  TimeStamp when_;
  State state_; // 状态
  int voteCount_ = 0;
  int id_;
  int peerNum_;
  int currentTerm_; // 当前任期
  int votedFor_;    // 当前任期投票对象
  Log log_;         //日志
  int commitIndex_; //
  int lastApplied_;
  std::vector<int> nextIndex_;
  std::vector<int> matchIndex_;
};

struct RequestVoteArgs {
  int term = -1;
  int candidateId = -1;
  int lastLogIndex = -1;
  int lastLogTerm = -1;
};

struct RequestVoteReply {
  int term = -1;
  bool voteGranted = false;
};

struct AppendEntriesArgs {
  int term = -1;
  int leaderId = -1;
  int prevLogIndex = -1;
  int prevLogTerm = -1;
  Log entries;
  int leaderCommit = -1;
};

struct AppendEntriesReply {
  int term = -1;
  bool success = false;
  int prevIndex = -1;
};