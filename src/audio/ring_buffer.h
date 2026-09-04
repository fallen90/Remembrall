#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lta {

// Single-producer single-consumer lock-free byte ring buffer.
// Capture thread writes; preprocess thread reads.
class RingBuffer {
 public:
  explicit RingBuffer(size_t capacity_bytes);

  // Returns bytes written (may be less than requested if full — caller should drop).
  size_t Write(const uint8_t* data, size_t bytes);

  // Returns bytes read.
  size_t Read(uint8_t* data, size_t bytes);

  [[nodiscard]] size_t Size() const;
  [[nodiscard]] size_t Capacity() const { return capacity_; }
  [[nodiscard]] double OccupancyPercent() const;
  void Clear();

 private:
  size_t capacity_;
  std::vector<uint8_t> buffer_;
  std::atomic<size_t> head_{0};  // write index
  std::atomic<size_t> tail_{0};  // read index
};

}  // namespace lta
