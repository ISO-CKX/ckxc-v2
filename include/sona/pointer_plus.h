#ifndef POINTER_PLUS_H
#define POINTER_PLUS_H

#include <functional>
#include <memory>

namespace sona {

template <typename T> class ref_ptr {
public:
  ref_ptr(T *ptr) : ptr(ptr) {}

  ref_ptr(T &ref) : ptr(std::addressof(ref)) {}
  ref_ptr(T &&) = delete;

  ref_ptr(ref_ptr const &) = default;
  ref_ptr &operator=(ref_ptr const &) = default;

  operator ref_ptr<T const>() const noexcept {
    return ref_ptr<T const>(*ptr);
  }

  T* operator->() noexcept { return ptr; }
  T const* operator->() const noexcept { return ptr; }

  T& get() noexcept { return *ptr; }
  T const& get() const noexcept { return *ptr; }

  template <typename U>
  ref_ptr<U> cast_unsafe() noexcept {
    return ref_ptr<U>(static_cast<U*>(ptr));
  }

  template <typename U>
  ref_ptr<U const> cast_unsafe() const noexcept {
    return ref_ptr<U>(static_cast<U const*>(ptr));
  }

  friend bool operator== (ref_ptr<T> lhs, ref_ptr<T> rhs) {
    return lhs.ptr == rhs.ptr;
  }

  friend bool operator== (ref_ptr<T> lhs, T* rhs) {
    return lhs.ptr == rhs;
  }

  friend bool operator== (T* lhs, ref_ptr<T> rhs) {
    return lhs == rhs.ptr;
  }

  friend bool operator!= (ref_ptr<T> lhs, ref_ptr<T> rhs) {
    return lhs.ptr != rhs.ptr;
  }

  friend bool operator!= (ref_ptr<T> lhs, T* rhs) {
    return lhs.ptr != rhs;
  }

  friend bool operator!= (T* lhs, ref_ptr<T> rhs) {
    return lhs != rhs.ptr;
  }

private:
  T *ptr;
};

} // namespace sona

namespace std {

template<typename T>
struct hash<sona::ref_ptr<T>> {
  std::size_t operator()(sona::ref_ptr<T> const& r) const noexcept {
    return ::std::hash<const void*>()(static_cast<const void*>(&(r.get())));
  }
};

} // namespace std

namespace sona {

template <typename T> class owner {
public:
  owner(T *ptr) : ptr(ptr) {}
  ~owner() { delete ptr; }

  owner(owner const &) = delete;
  owner(owner &&that) {
    if (&that == this) {
      return;
    }
    ptr = that.ptr;
    that.ptr = nullptr;
  }

  owner& operator=(owner const&) = delete;
  owner& operator=(owner &&that) {
    if (&that == this) {
      return *this;
    }
    ptr = that.ptr;
    that.ptr = nullptr;
    return *this;
  }

  ref_ptr<T> borrow() noexcept { return ref_ptr<T>(*ptr); }
  ref_ptr<T const> borrow() const noexcept { return ref_ptr<T>(*ptr); }

  template <typename U>
  owner<U> cast_unsafe() && noexcept {
    owner<U> ret (static_cast<U*>(ptr));
    this->ptr = nullptr;
    return ret;
  }

  T* get() && noexcept {
    T* ret = ptr;
    ptr = nullptr;
    return ret;
  }

private:
  T *ptr;
};

template <typename T> using refcount = std::shared_ptr<T>;

} // namespace sona

#endif // POINTER_PLUS_H
