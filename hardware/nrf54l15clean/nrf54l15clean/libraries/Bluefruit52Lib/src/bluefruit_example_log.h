#pragma once

#include <Arduino.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

template <size_t QueueDepth, size_t LineLen>
class BluefruitExampleLogQueue {
 public:
  BluefruitExampleLogQueue()
      : head_(0U), tail_(0U), count_(0U), dropped_(0UL) {
    clear();
  }

  void clear() {
    memset(lines_, 0, sizeof(lines_));
    head_ = 0U;
    tail_ = 0U;
    count_ = 0U;
    dropped_ = 0UL;
  }

  void queue(const char* text) {
    if (text == nullptr) {
      return;
    }
    if (count_ >= QueueDepth) {
      ++dropped_;
      return;
    }

    snprintf(lines_[head_], LineLen, "%s", text);
    head_ = nextIndex(head_);
    ++count_;
  }

  void queuef(const char* format, ...) {
    if (format == nullptr) {
      return;
    }

    char line[LineLen];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    queue(line);
  }

  void flush(Stream& stream) {
    if (dropped_ != 0UL) {
      stream.print("[Log] dropped ");
      stream.print(static_cast<unsigned long>(dropped_));
      stream.println(" lines");
      dropped_ = 0UL;
    }

    while (count_ != 0U) {
      stream.println(lines_[tail_]);
      tail_ = nextIndex(tail_);
      --count_;
    }
  }

 private:
  static size_t nextIndex(size_t index) {
    return (index + 1U) % QueueDepth;
  }

  char lines_[QueueDepth][LineLen];
  size_t head_;
  size_t tail_;
  size_t count_;
  unsigned long dropped_;
};
