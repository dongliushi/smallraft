#include "smallraft/config.h"
#include "smallraft/node.h"
#include <iostream>
#include <smalljson/smalljson.h>
#include <sys/fcntl.h>
#include <unistd.h>
using namespace smalljson;
using namespace std;

int main(int args, char **argv) {
  string file_name;
  if (args > 1) {
    file_name = argv[1];
  }
  int fd = open(file_name.c_str(), O_RDONLY);
  assert(fd != -1);
  char buf[1024] = {0};
  read(fd, buf, sizeof buf);
  Value c = Parser::parse(buf);
  Config config;
  config.id = c["me"].to_integer();
  string ip = c["servers"][to_string(config.id)]["ip"].to_string();
  size_t port = c["servers"][to_string(config.id)]["port"].to_integer();
  size_t peerNum = c["servers"].to_object().size();
  config.serverAddr = {ip, port};
  for (int i = 1; i <= peerNum; i++) {
    string peer_ip = c["servers"][to_string(i)]["ip"].to_string();
    size_t peer_port = c["servers"][to_string(i)]["port"].to_integer();
    config.peerAddr.emplace_back(peer_ip, peer_port);
  }
  EventLoopThread loopThread;
  EventLoop *rpcServerLoop = loopThread.startLoop();
  Node node(rpcServerLoop, config);
  EventLoop commandLoop;
  commandLoop.runEvery(1s, [&] {
    if (node.isLeader()) {
      node.provideCommand("raft example");
    }
  });
  node.start();
  commandLoop.loop();
  return 0;
}