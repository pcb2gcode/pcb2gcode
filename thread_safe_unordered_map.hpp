#ifndef THREAD_SAFE_UNORDERED_MAP_HPP
#define THREAD_SAFE_UNORDERED_MAP_HPP

#include <mutex>
#include <unordered_map>

#include <boost/optional.hpp>

template <typename Key,
          typename Value,
          typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class ThreadSafeUnorderedMap {
 public:
  using key_type = Key;
  using mapped_type = Value;
  using map_type = std::unordered_map<Key, Value, Hash, KeyEqual>;

  bool contains(const key_type& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.find(key) != values_.cend();
  }

  boost::optional<mapped_type> get(const key_type& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(key);
    if (it == values_.cend()) {
      return boost::none;
    }
    return it->second;
  }

  const mapped_type* find_ptr(const key_type& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(key);
    if (it == values_.cend()) {
      return nullptr;
    }
    return &it->second;
  }

  template <typename ValueFactory>
  const mapped_type& get_or_insert(
      const key_type& key,
      ValueFactory&& value_factory) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(key);
    if (it != values_.cend()) {
      return it->second;
    }
    auto inserted = values_.emplace(key, std::forward<ValueFactory>(value_factory)());
    return inserted.first->second;
  }

  void set(const key_type& key, const mapped_type& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    values_[key] = value;
  }

  void set(const key_type& key, mapped_type&& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    values_[key] = std::move(value);
  }

  bool erase(const key_type& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.erase(key) > 0;
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    values_.clear();
  }

  std::size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.size();
  }

 private:
  mutable std::mutex mutex_;
  map_type values_;
};

#endif // THREAD_SAFE_UNORDERED_MAP_HPP
