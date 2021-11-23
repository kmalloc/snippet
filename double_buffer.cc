#include <atomic>

template <typename T>
class DoubleBuffer {
  T value_[2];
  std::atomic<uint32_t> idx_;

 public:
  DoubleBuffer() : idx_(0) {}

  T& Value() { return value_[idx_.load(std::memory_order_acquire)]; }

  T& Writable() { return value_[!idx_.load(std::memory_order_acquire)]; }
  const T& Readable() { return value_[idx_.load(std::memory_order_acquire)]; }

  void SwitchReadWrite() {
    auto curr = idx_.load(std::memory_order_acquire);
    idx_.store(!curr, std::memory_order_release);
  }
  void UpdateValue(T v) {
    Writable() = std::move(v);
    SwitchReadWrite();
  }
};
