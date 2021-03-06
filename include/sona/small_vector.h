#ifndef SMALL_VECTOR_H
#define SMALL_VECTOR_H

#include <cstddef>
#include <cstring>
#include <memory>

#include "util.h"

namespace sona {

/// Now we must provide move constructor for small_vector<>
/// So letme think think...

template <typename T, std::size_t PossibleSize> class small_vector {
public:
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using value_type = T;
  using reference = T &;
  using pointer = T *;
  using iterator = T *;
  using const_iterator = T const *;

  small_vector() {
    std::memset(reinterpret_cast<void *>(&u), 0, sizeof(u));
    u_status = uninitialized;
  }

  small_vector(size_type count, value_type const &value = value_type()) {
    if (count > PossibleSize) {
      u.heap.dyn_mem_start = ::operator new(count * sizeof(value_type));
      u.heap.dyn_mem_end = u.heap.dyn_mem_start + count;
      u.heap.dyn_mem_use = u.heap.dyn_mem_end;

      for (T *mem_iter = u.heap.dyn_mem_start; mem_iter != u.heap.dyn_mem_use;
           ++mem_iter) {
        construct<T>(mem_iter, value);
      }

      u_status = locally;
    } else {
      u.s.usage = count;
      T *storage = reinterpret_cast<T *>(&(u.s.storage));
      for (T *mem_iter = storage; mem_iter != storage + u.s.usage; ++mem_iter) {
        construct<T>(mem_iter, value);
      }
    }
  }

  small_vector(small_vector const &that) {
    if (that.u_status == uninitialized)
      u_status = uninitialized;
    else if (that.u_status == locally) {
      std::uninitialized_copy(that.cbegin(), that.cend(),
                              reinterpret_cast<T *>(&(u.s.storage)));
      u.s.usage = that.u.s.usage;
      u_status = locally;
    } else if (that.u_status == onheap) {
      T *dyn_mem_start =
          reinterpret_cast<T *>(::operator new(that.size() * sizeof(T)));
      T *dyn_mem_end = dyn_mem_start + that.size();
      T *dyn_mem_usage = dyn_mem_end;

      std::uninitialized_copy(that.cbegin(), that.cend(), dyn_mem_start);
      u.heap.dyn_mem_start = dyn_mem_start;
      u.heap.dyn_mem_end = dyn_mem_end;
      u.heap.dyn_mem_usage = dyn_mem_usage;
      u_status = onheap;
    } else {
      sona_unreachable();
    }
  }

  small_vector(small_vector &&that) {
    if (that.u_status == uninitialized) {
      u_status = uninitialized;
    } else if (that.u_status == locally) {
      u.s.usage = 0;
      T *storage = reinterpret_cast<T *>(&(u.s.storage));
      for (auto &value : that) {
        construct<T>(storage + u.s.usage, std::move(value));
        destroy_at(&value);
        ++u.s.usage;
      }
      u_status = locally;
    } else if (that.u_status == onheap) {
      u.heap.dyn_mem_start = that.u.heap.dyn_mem_start;
      u.heap.dyn_mem_end = that.u.heap.dyn_mem_end;
      u.heap.dyn_mem_usage = that.u.heap.dyn_mem_usage;
      u_status = onheap;
    } else {
      sona_unreachable();
    }

    that.u_status = uninitialized;
  }

  small_vector(std::initializer_list<T> init_list) {
    if (init_list.size() <= PossibleSize) {
      std::uninitialized_copy(init_list.begin(), init_list.end(),
                              reinterpret_cast<T *>(&(u.s.storage)));
      u.s.usage = init_list.size();
      u_status = locally;
    } else {
      /// @todo Reuse previous codes
      T *dyn_mem_start =
          reinterpret_cast<T *>(::operator new(init_list.size() * sizeof(T)));
      T *dyn_mem_end = dyn_mem_start + init_list.size();
      T *dyn_mem_usage = dyn_mem_end;

      std::uninitialized_copy(init_list.begin(), init_list.end(),
                              dyn_mem_start);
      u.heap.dyn_mem_start = dyn_mem_start;
      u.heap.dyn_mem_end = dyn_mem_end;
      u.heap.dyn_mem_usage = dyn_mem_usage;
      u_status = onheap;
    }
  }

  ~small_vector() {
    if (u_status == uninitialized) return;
    for (auto &value : *this) destroy_at(&value);
    if (u_status == onheap) { ::operator delete(u.heap.dyn_mem_start); }
  }

  iterator begin() {
    if (u_status == locally)
      return reinterpret_cast<T *>(&(u.s.storage));
    else if (u_status == onheap)
      return u.heap.dyn_mem_start;
    else if (u_status == uninitialized)
      return nullptr;
    else {
      sona_unreachable();
      return nullptr;
    }
  }

  iterator end() {
    if (u_status == locally)
      return reinterpret_cast<T *>(&(u.s.storage)) + u.s.usage;
    else if (u_status == onheap)
      return u.heap.dyn_mem_usage;
    else if (u_status == uninitialized)
      return nullptr;
    else {
      sona_unreachable();
      return nullptr;
    }
  }

  const_iterator begin() const { return cbegin(); }

  const_iterator end() const { return cend(); }

  const_iterator cbegin() const {
    if (u_status == locally)
      return reinterpret_cast<T const *>(&(u.s.storage));
    else if (u_status == onheap)
      return u.heap.dyn_mem_start;
    else if (u_status == uninitialized)
      return nullptr;
    else {
      sona_unreachable();
      return nullptr;
    }
  }

  const_iterator cend() const {
    if (u_status == locally)
      return reinterpret_cast<T const *>(&(u.s.storage)) + u.s.usage;
    else if (u_status == onheap)
      return u.heap.dyn_mem_usage;
    else if (u_status == uninitialized)
      return nullptr;
    else {
      sona_unreachable();
      return nullptr;
    }
  }

  value_type &operator[](size_type n) { return *(begin() + n); }
  value_type const &operator[](size_type n) const { return *(cbegin() + n); }
  size_type size() const { return cend() - cbegin(); }

  void push_back(value_type const &value) {
    if (u_status == uninitialized) {
      u_status = locally;
      u.s.usage = 0;
    }
    /// @attention fallthrough

    if (u_status == locally) {
      if (u.s.usage < PossibleSize) {
        construct<T>(reinterpret_cast<T *>(&(u.s.storage)) + u.s.usage, value);
        u.s.usage++; /// The usage counter goes incorrect here
        return;
      } else {
        T *dyn_mem_start = reinterpret_cast<T *>(
            ::operator new(PossibleSize * 2 * sizeof(value_type)));
        T *dyn_mem_end = dyn_mem_start + 2 * PossibleSize;
        T *dyn_mem_usage = dyn_mem_start + PossibleSize;

        {
          T *mem_iter = dyn_mem_start;
          for (auto iter = begin(); iter != end(); ++iter, ++mem_iter)
            construct<T>(mem_iter, std::move(*iter));
        }

        for (auto iter = begin(); iter != end(); ++iter)
          destroy_at<T>(&(*iter));

        u.heap.dyn_mem_start = dyn_mem_start;
        u.heap.dyn_mem_end = dyn_mem_end;
        u.heap.dyn_mem_usage = dyn_mem_usage;
        u_status = onheap;
        /// @attention fallthrough!
      }
    }

    if (u_status == onheap) {
      if (u.heap.dyn_mem_usage == u.heap.dyn_mem_end) {
        T *dyn_mem_start = reinterpret_cast<T *>(
            ::operator new(size() * 2 * sizeof(value_type)));
        T *dyn_mem_end = dyn_mem_start + size() * 2 * PossibleSize;
        T *dyn_mem_usage = dyn_mem_start + size();

        /// @todo uninitialized_move
        {
          T *mem_iter = dyn_mem_start;
          for (auto iter = begin(); iter != end(); ++iter, ++mem_iter)
            construct<T>(mem_iter, std::move(*iter));
        }

        for (auto iter = begin(); iter != end(); ++iter)
          destroy_at<T>(&(*iter));

        u.heap.dyn_mem_start = dyn_mem_start;
        u.heap.dyn_mem_end = dyn_mem_end;
        u.heap.dyn_mem_usage = dyn_mem_usage;
      }
      /// @attention fallthrough

      construct<T>(u.heap.dyn_mem_usage, value);
      u.heap.dyn_mem_usage++;
      return;
    }

    sona_unreachable();
  }

  /// @todo reuse codes
  /// And then how to reuse?
  /// Any suggestions?
  /*
  void push_back(value_type&& value) {

  }

  template <typename ...Args>
  void emplace_back(Args&& ...args) {

  }*/

  void pop_back() {
    sona_assert1(size() != 0, "Empty small vector!");

    destroy_at<T>(end() - 1);
    if (u_status == locally) {
      u.s.usage--;
    } else if (u_status == onheap) {
      u.heap.dyn_mem_usage--;
    }
  }

private:
  union {
    struct {
      std::aligned_storage_t<PossibleSize * sizeof(T), alignof(T)> storage;
      size_type usage;
    } s;
    struct {
      T *dyn_mem_start, *dyn_mem_end, *dyn_mem_usage;
    } heap;
  } u;
  enum union_status { uninitialized, locally, onheap } u_status;
  /// Locally means elements are stored in the aligned storage
  /// while onheap means now it's just a common vector, allocates on heap
};

} // namespace sona

#endif // SMALL_VECTOR_H
