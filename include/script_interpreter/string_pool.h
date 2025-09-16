#pragma once

#include <array>
#include <bitset>
#include <string>

// Fast string pool for performance optimization
// Reuses string objects to reduce memory allocations
class FastStringPool {
 private:
  static constexpr size_t POOL_SIZE = 128;
  static constexpr size_t STRING_CAPACITY = 512;

  std::array<std::string, POOL_SIZE> pool;
  std::bitset<POOL_SIZE> in_use;
  size_t next_search = 0;

 public:
  FastStringPool() {
    // Pre-allocate capacity for all strings in the pool
    for (auto& str : pool) {
      str.reserve(STRING_CAPACITY);
    }
  }

  // Acquire a string from the pool
  std::string* acquire() {
    for (size_t i = 0; i < POOL_SIZE; ++i) {
      size_t idx = (next_search + i) % POOL_SIZE;
      if (!in_use[idx]) {
        in_use[idx] = true;
        next_search = (idx + 1) % POOL_SIZE;
        pool[idx].clear();
        return &pool[idx];
      }
    }
    return nullptr;  // Pool exhausted - fall back to regular allocation
  }

  // Release a string back to the pool
  void release(std::string* str) {
    if (!str)
      return;

    auto it = std::find_if(pool.begin(), pool.end(),
                           [str](const auto& s) { return &s == str; });
    if (it != pool.end()) {
      size_t idx = std::distance(pool.begin(), it);
      in_use[idx] = false;

      // Shrink string if it grew too large
      if (it->capacity() > STRING_CAPACITY * 2) {
        it->clear();
        it->shrink_to_fit();
        it->reserve(STRING_CAPACITY);
      }
    }
  }

  // RAII wrapper for automatic string release
  class PooledString {
   private:
    std::string* str_;
    FastStringPool* pool_;

   public:
    PooledString(FastStringPool* pool) : pool_(pool) {
      str_ = pool_->acquire();
    }

    ~PooledString() {
      if (pool_ && str_) {
        pool_->release(str_);
      }
    }

    // Move constructor
    PooledString(PooledString&& other) noexcept
        : str_(other.str_), pool_(other.pool_) {
      other.str_ = nullptr;
      other.pool_ = nullptr;
    }

    // Disable copy constructor and assignment
    PooledString(const PooledString&) = delete;
    PooledString& operator=(const PooledString&) = delete;
    PooledString& operator=(PooledString&&) = delete;

    std::string* get() {
      return str_;
    }
    std::string& operator*() {
      return *str_;
    }
    std::string* operator->() {
      return str_;
    }

    bool valid() const {
      return str_ != nullptr;
    }
  };

  // Get a RAII-managed string from the pool
  PooledString get_pooled_string() {
    return PooledString(this);
  }
};

// Thread-local string pool for maximum performance
extern thread_local FastStringPool g_string_pool;