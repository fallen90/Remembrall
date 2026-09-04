#include "audio/ring_buffer.h"

#include <algorithm>
#include <cstring>

namespace lta {

RingBuffer::RingBuffer(size_t capacity_bytes)
    : capacity_(std::max<size_t>(capacity_bytes, 64)),
      buffer_(capacity_) {}

size_t RingBuffer::Write(const uint8_t* data, size_t bytes) {
  if (!data || bytes == 0) {
    return 0;
  }
  const size_t head = head_.load(std::memory_order_relaxed);
  const size_t tail = tail_.load(std::memory_order_acquire);
  const size_t used = head - tail;
  const size_t free = capacity_ - used;
  const size_t to_write = std::min(bytes, free);
  if (to_write == 0) {
    return 0;
  }

  const size_t write_pos = head % capacity_;
  const size_t first = std::min(to_write, capacity_ - write_pos);
  std::memcpy(buffer_.data() + write_pos, data, first);
  if (to_write > first) {
    std::memcpy(buffer_.data(), data + first, to_write - first);
  }
  head_.store(head + to_write, std::memory_order_release);
  return to_write;
}

size_t RingBuffer::Read(uint8_t* data, size_t bytes) {
  if (!data || bytes == 0) {
    return 0;
  }
  const size_t tail = tail_.load(std::memory_order_relaxed);
  const size_t head = head_.load(std::memory_order_acquire);
  const size_t available = head - tail;
  const size_t to_read = std::min(bytes, available);
  if (to_read == 0) {
    return 0;
  }

  const size_t read_pos = tail % capacity_;
  const size_t first = std::min(to_read, capacity_ - read_pos);
  std::memcpy(data, buffer_.data() + read_pos, first);
  if (to_read > first) {
    std::memcpy(data + first, buffer_.data(), to_read - first);
  }
  tail_.store(tail + to_read, std::memory_order_release);
  return to_read;
}

size_t RingBuffer::Size() const {
  const size_t head = head_.load(std::memory_order_acquire);
  const size_t tail = tail_.load(std::memory_order_acquire);
  return head - tail;
}

double RingBuffer::OccupancyPercent() const {
  if (capacity_ == 0) {
    return 0;
  }
  return 100.0 * static_cast<double>(Size()) / static_cast<double>(capacity_);
}

void RingBuffer::Clear() {
  const size_t head = head_.load(std::memory_order_acquire);
  tail_.store(head, std::memory_order_release);
}

}  // namespace lta
