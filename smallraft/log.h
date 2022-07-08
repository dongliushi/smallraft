#pragma once

#include <algorithm>
#include <smalljson/smalljson.h>
#include <vector>

struct LogEntry {
  LogEntry() : index(0), term(0) {}
  int index;
  int term;
  smalljson::Value command;
};

// class Log {
// public:
//   typedef std::vector<LogEntry>::iterator iterator;
//   typedef std::vector<LogEntry>::const_iterator const_iterator;

//   Log() = default;
//   Log(std::vector<LogEntry> entries) : log_(entries) {}
//   Log(std::initializer_list<LogEntry> init_list) : log_(init_list) {}
//   int firstLogTerm() const { return firstLogEntry().term; }
//   int firstLogIndex() const { return firstLogEntry().index; }
//   int lastLogTerm() const { return lastLogEntry().term; }
//   int lastLogIndex() const { return lastLogEntry().index; }
//   iterator begin() { return log_.begin(); }
//   iterator end() { return log_.end(); }
//   const_iterator begin() const { return log_.begin(); }
//   const_iterator end() const { return log_.end(); }
//   size_t size() const { return log_.size(); }
//   int atTerm(size_t idx) const { return log_.at(idx).term; }
//   int atIndex(size_t idx) const { return log_.at(idx).index; }
//   const smalljson::Value &atCommand(size_t idx) const {
//     return log_.at(idx).command;
//   }
//   bool isUpdate(int term, int index) {
//     if (lastLogTerm() < term) {
//       return true;
//     } else if (lastLogTerm() == term && lastLogIndex() <= index) {
//       return true;
//     }
//     return false;
//   }
//   const LogEntry &operator[](size_t idx) const { return log_[idx]; }
//   LogEntry &operator[](size_t idx) { return log_[idx]; }
//   iterator erase(const_iterator first, const_iterator last) {
//     return log_.erase(first, last);
//   }
//   void insert(const_iterator pos, const Log &log) {
//     for (auto &logEntry : log) {
//       log_.insert(pos, logEntry);
//     }
//   }
//   iterator insert(const_iterator pos, const LogEntry &value) {
//     return log_.insert(pos, value);
//   }
//   std::vector<LogEntry> getEntries(int index) {
//     return {log_.begin() + index, log_.end()};
//   }

// public:
//   template <typename... Args>
//   std::vector<LogEntry>::reference emplace_back(Args &&...args) {
//     return log_.emplace_back(std::forward<Args>(args)...);
//   }

// private:
//   const LogEntry &firstLogEntry() const { return *log_.begin(); }
//   const LogEntry &lastLogEntry() const { return *(log_.end() - 1); }
//   std::vector<LogEntry> log_; //日志
// };