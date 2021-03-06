#ifndef Q_LIST_H
#define Q_LIST_H

#include <cstddef>
#include <initializer_list>
#include <memory>

namespace sona {

class q_list_impl {
public:
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using value_type = void *;
  using reference = void *&;
  using pointer = value_type *;

  using iterator = value_type *;
  using const_iterator = value_type const *;

  q_list_impl();
  q_list_impl(std::size_t reserved_size);
  ~q_list_impl();

  size_type size() const noexcept;

  void push_back(void *data);
  void pop_back();

  reference front();
  reference back();

  iterator begin() noexcept;
  iterator end() noexcept;
  const_iterator begin() const noexcept;
  const_iterator end() const noexcept;
  const_iterator cbegin() const noexcept;
  const_iterator cend() const noexcept;

  void swap(q_list_impl &that) noexcept;

private:
  void reallocate_when_need();

  q_list_impl &operator=(q_list_impl &&that);
  void **storage_begin, **storage_end, **storage_usage;
};

/// @class This class acts as Qt's QList but does not contain metadata
template <typename T> class q_list {
  using raw_data_iterator = q_list_impl::iterator;
  using const_raw_data_iterator = q_list_impl::const_iterator;

public:
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using value_type = T;
  using reference = T &;
  using pointer = T *;
  using const_reference = T const &;
  using const_pointer = T const *;

  class iterator {
  public:
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = q_list::value_type;
    using reference = q_list::reference;
    using pointer = q_list::pointer;

    iterator(raw_data_iterator raw_data_iter) noexcept
        : raw_data_iter(raw_data_iter) {}

    iterator(iterator const &that) noexcept
        : raw_data_iter(that.raw_data_iter) {}

    reference operator*() const noexcept {
      return *(reinterpret_cast<value_type *>(*raw_data_iter));
    }

    iterator &operator=(iterator const &that) noexcept {
      raw_data_iter = that.raw_data_iter;
      return *this;
    }

    iterator &operator++() noexcept {
      ++raw_data_iter;
      return *this;
    }

    iterator &operator--() noexcept {
      --raw_data_iter;
      return *this;
    }

    iterator operator++(int)noexcept {
      iterator iter = *this;
      ++(*this);
      return iter;
    }

    iterator operator--(int)noexcept {
      iterator iter = *this;
      --(*this);
      return iter;
    }

    iterator &operator+=(difference_type diff) noexcept {
      raw_data_iter += diff;
      return *this;
    }

    iterator &operator-=(difference_type diff) noexcept {
      (*this) += (-diff);
      return *this;
    }

    iterator operator+(difference_type diff) const noexcept {
      iterator ret = *this;
      ret += diff;
      return ret;
    }

    iterator operator-(difference_type diff) const noexcept {
      iterator ret = *this;
      ret -= diff;
      return ret;
    }

    difference_type operator-(iterator const &that) const noexcept {
      return raw_data_iter - that.raw_data_iter;
    }

    bool operator==(iterator const &that) const noexcept {
      return raw_data_iter == that.raw_data_iter;
    }

    bool operator!=(iterator const &that) const noexcept {
      return !(*this == that);
    }

    bool operator<(iterator const &that) const noexcept {
      return raw_data_iter < that.raw_data_iter;
    }

    bool operator>(iterator const &that) const noexcept {
      return raw_data_iter > that.raw_data_iter;
    }

    bool operator<=(iterator const &that) const noexcept {
      return !(*this > that);
    }

    bool operator>=(iterator const &that) const noexcept {
      return !(*this < that);
    }

  private:
    raw_data_iterator raw_data_iter;
  };

  class const_iterator {
    using self_type = const_iterator;

  public:
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = q_list::value_type;
    using reference = const_reference;
    using pointer = const_pointer;

    const_iterator(const_raw_data_iterator raw_data_iter) noexcept
        : raw_data_iter(raw_data_iter) {}

    const_iterator(self_type const &that) noexcept
        : raw_data_iter(that.raw_data_iter) {}

    reference operator*() const noexcept {
      return reinterpret_cast<value_type *>(*raw_data_iter);
    }

    self_type &operator=(iterator const &that) noexcept {
      raw_data_iter = that.raw_data_iter;
      return *this;
    }

    self_type &operator++() noexcept {
      ++raw_data_iter;
      return *this;
    }

    self_type &operator--() noexcept {
      --raw_data_iter;
      return *this;
    }

    self_type operator++(int)noexcept {
      self_type iter = *this;
      ++(*this);
      return iter;
    }

    self_type operator--(int)noexcept {
      self_type iter = *this;
      --(*this);
      return iter;
    }

    self_type &operator+=(difference_type diff) noexcept {
      raw_data_iter += diff;
      return *this;
    }

    self_type &operator-=(difference_type diff) noexcept {
      raw_data_iter -= diff;
      return *this;
    }

    difference_type operator-(self_type const &that) const noexcept {
      return raw_data_iter - that.raw_data_iter;
    }

    bool operator==(self_type const &that) const noexcept {
      return raw_data_iter == that.raw_data_iter;
    }

    bool operator!=(self_type const &that) const noexcept {
      return !(*this == that);
    }

    bool operator<(self_type const &that) const noexcept {
      return raw_data_iter < that.raw_data_iter;
    }

    bool operator>(self_type const &that) const noexcept {
      return raw_data_iter > that.raw_data_iter;
    }

    bool operator<=(self_type const &that) const noexcept {
      return !(*this > that);
    }

    bool operator>=(self_type const &that) const noexcept {
      return !(*this < that);
    }

  private:
    const_raw_data_iterator raw_data_iter;
  };

  q_list() : impl() {}

  q_list(value_type const &value, size_type count) : impl(count) {
    for (size_type i = 0; i < count; ++i)
      impl.push_back((void *)(new value_type(value)));
  }

  q_list(std::initializer_list<value_type> ilist) : impl(ilist.size()) {
    for (auto &x : ilist) impl.push_back(new value_type(std::move(x)));
  }

  ~q_list() {
    for (void *data : impl) {
      value_type *casted_data = reinterpret_cast<value_type *>(data);
      delete casted_data;
    }
  }

  reference front() { return *(reinterpret_cast<value_type *>(impl.front())); }
  reference back() { return *(reinterpret_cast<value_type *>(impl.back())); }

  void push_back(value_type const &value) {
    impl.push_back((void *)(new value_type(value)));
  }

  void push_back(value_type &&value) {
    impl.push_back((void *)(new value_type(std::move(value))));
  }

  void pop_back() {
    value_type *casted_data = reinterpret_cast<value_type *>(impl.back());
    delete casted_data;
    impl.pop_back();
  }

  iterator begin() noexcept { return iterator(impl.begin()); }
  iterator end() noexcept { return iterator(impl.end()); }

private:
  q_list_impl impl;
};

} // namespace sona

#endif // Q_LIST_H
