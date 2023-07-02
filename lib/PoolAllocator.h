#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <stack>

template<typename T, size_t pools_number, std::array<size_t, pools_number> pools_sizes,
    std::array<size_t, pools_number> chunks_sizes>
class PoolAllocator {
 public:
  static_assert(pools_number > 0, "Pools number should be > 0!");

  using value_type = T;

  template<class Other>
  struct rebind {
    using other = PoolAllocator<Other, pools_number, pools_sizes, chunks_sizes>;
  };

  PoolAllocator() {
    for (size_t i = 0; i < pools_number; ++i) {
      pools.push_back(Pool(chunks_sizes[i], pools_sizes[i]));
    }
  }

  PoolAllocator(const PoolAllocator& from)
      : pools(from.pools) {
  }

  ~PoolAllocator() {
    for (auto pool : pools) {
      free(pool.begin);
    }
  }

  T* allocate(size_t n) {
    for (size_t i = 0; i < pools.size(); ++i) {
      if (pools[i].chunk_size >= (sizeof(T) * n)) {
        if (!pools[i].free_chunks.empty()) {
          T* to_allocate = pools[i].free_chunks.top();
          pools[i].free_chunks.pop();
          return to_allocate;
        }
      }
    }
    throw std::bad_alloc(); // no acceptable pool found
  }

  void deallocate(T* p, size_t n) {
    for (Pool& pool : pools) {
      if (pool.begin <= p && p <= pool.end) {
        pool.free_chunks.push(p);
      }
    }
  }

  template<typename U, size_t rhs_pools_number, std::array<size_t, rhs_pools_number> rhs_pools_sizes,
      std::array<size_t, rhs_pools_number> rhs_chunks_sizes>
  bool operator==(const PoolAllocator<U, rhs_pools_number, rhs_pools_sizes, rhs_chunks_sizes>& rhs) {
    // checking if all chunks of this alloc can be deallocated by rhs
    for (auto pool : pools) {
      for (auto chunk : pool.chunks) {
        bool found = false;
        for (auto rhs_pool : rhs.pools) {
          for (auto rhs_chunk : rhs_pool) {
            if (chunk == rhs_chunk) {
              found = true;
              break;
            }
          }
        }
        if (!found) return false;
      }
    }
    return true;
  }

  PoolAllocator& operator=(const PoolAllocator& rhs) {
    pools = rhs.pools;
    return *this;
  }

  struct Pool {
    Pool(size_t chunk_size_arg, size_t chunk_number_arg) : chunk_size(chunk_size_arg), chunk_number(chunk_number_arg) {
      assert(chunk_size > 0);
      assert(chunk_number > 0);
      begin = reinterpret_cast<T*>(malloc(chunk_number * chunk_size));
      T* cur = begin;
      free_chunks.push(cur);
      chunks.push_back(cur);
      for (size_t i = 1; i < chunk_number; ++i) {
        cur = reinterpret_cast<T*>(reinterpret_cast<char*>(cur) + chunk_size);
        free_chunks.push(cur);
        chunks.push_back(cur);
      }
      end = cur;
    }
    T* begin;
    T* end;
    size_t chunk_size;
    size_t chunk_number;
    std::stack<T*> free_chunks;
    std::vector<T*> chunks;
  };
  std::vector<Pool> pools;
};