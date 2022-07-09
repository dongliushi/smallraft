#pragma once

#include "config.h"
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

struct LogEntry;
struct RequestVoteArgs;
struct RequestVoteReply;
struct AppendEntriesArgs;
struct AppendEntriesReply;
class RaftPeer;

class Raft : public std::enable_shared_from_this<Raft> {
public:
  enum class State { Follower, Candidate, Leader };
  typedef std::shared_ptr<RaftPeer> RaftPeerPtr;
  typedef std::vector<LogEntry> Log;
  friend class RaftService;
  friend class RaftPeer;

public:
  Raft() = delete;
  Raft(EventLoop *loop, const Config &config);
  void start();
  void tick();
  void info();
  bool isLeader() { return state_ == State::Leader; }
  void provideCommand(const smalljson::Value &command);

private:
  State state() const { return state_; }
  std::string stateString();
  void resetTimer();
  void heartbeat();
  void RequestVote(const RequestVoteArgs &args, RequestVoteReply &reply);
  void FinishRequestVote(int peerId, const RequestVoteArgs &args,
                         const RequestVoteReply &reply);

  void AppendEntries(const AppendEntriesArgs &args, AppendEntriesReply &reply);
  void FinishAppendEntries(int peerId, const AppendEntriesArgs &args,
                           const AppendEntriesReply &reply);

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
  State state_ = State::Follower; // 状态
  int id_;                        // 该服务器的id
  int peerNum_;                   // 集群个数
  int currentTerm_ =
      0; // 服务器已知最新的任期（在服务器首次启动时初始化为0，单调递增）
  int voteCount_ = 0; // 获得的投票数
  int votedFor_ = -1; // 当前任期内收到选票的
                      // candidateId，如果没有投给任何候选人则为空
  Log logs_; // 日志条目；每个条目包含了用于状态机的命令，以及领导人接收到该条目时的任期（初始索引为1）
  int commitIndex_ =
      0; // 已知已提交的最高的日志条目的索引（初始值为0，单调递增）
  int lastApplied_ =
      0; // 已经被应用到状态机的最高的日志条目的索引（初始值为0，单调递增）
  std::vector<int>
      nextIndex_; // 对于每一台服务器，发送到该服务器的下一个日志条目的索引（初始值为领导人最后的日志条目的索引+1）
  std::vector<int>
      matchIndex_; // 对于每一台服务器，已知的已经复制到该服务器的最高日志条目的索引（初始值为0，单调递增）
};

struct LogEntry {
  LogEntry() : index(0), term(0) {}
  LogEntry(int kIndex, int kTerm, const smalljson::Value &kCommand)
      : index(kIndex), term(kTerm), command(kCommand) {}
  int index;
  int term;
  smalljson::Value command;
};

struct RequestVoteArgs {
  int term;
  int candidateId;
  int lastLogIndex;
  int lastLogTerm;
};

struct RequestVoteReply {
  int term;
  bool voteGranted;
};

struct AppendEntriesArgs {
  int term;
  int leaderId;
  int prevLogIndex;
  int prevLogTerm;
  Raft::Log entries;
  int leaderCommit;
};

struct AppendEntriesReply {
  int term;
  bool success;
  int prevIndex;
};