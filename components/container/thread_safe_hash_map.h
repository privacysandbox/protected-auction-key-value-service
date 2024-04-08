/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMPONENTS_CONTAINER_THREAD_SAFE_HASH_MAP_H_
#define COMPONENTS_CONTAINER_THREAD_SAFE_HASH_MAP_H_

#include <functional>
#include <memory>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "absl/synchronization/mutex.h"

namespace kv_server {

// Implements a mutex based "thread-safe" container wrapper around Abseil
// maps. Exposes key-level locking such that two values associated two separate
// keys can be modified concurrently.
//
// Note that synchronization is done using `absl::Mutex` which is not re-entrant
// so trying to obtain a `MutableLockedNode` on key while already holding
// another `MutableLockedNode` for the same key will result in a deadlock. For
// example, the following code will deadlock:
//
//   ThreadSafeHashMap<int32_t, int32_t> map;
//   auto node = map.PutIfAbsent(10, 20);
//   map.Get(10); // This call deadlocks.
//
// For convenience, `LockedNode` provides a function `release()` to release a
// lock on a key early (instead of relying on RAII), but accessing the
// `LockedNode`'s contents after releasing the lock results in undefined
// behavior.
template <typename KeyT, typename ValueT>
class ThreadSafeHashMap {
 public:
  class const_iterator;
  template <typename ValueLockT>
  class LockedNode;
  // Locked read-only view for a specific key value associtation in the map.
  // Must call `is_present()` before calling and dereferencing `key()` and
  // `value()`.
  class ConstLockedNode;
  // Locked view for a specific key value associtation in the map with an
  // in-place modifable value. Must call `is_present()` before calling and
  // dereferencing `key()` and `value()`.
  using MutableLockedNode = LockedNode<absl::WriterMutexLock>;

  ThreadSafeHashMap() : maps_mutex_(std::make_unique<absl::Mutex>()) {}

  // Returns a locked read-only view for `key` and it's associated value. If
  // `key` does not exist in the map, then: `ConstLockedNode.is_present()` is
  // false.
  // Prefer this function for reads because it uses a shared
  // `absl::ReaderMutexLock` for synchronization and concurrent reads do not
  // block.
  template <typename Key = KeyT>
  ConstLockedNode CGet(Key&& key) const;

  // Returns a locked view for `key` with a modifable value. If `key` does not
  // exist in the map, then: `MutableLockedNode.is_present()` is false.
  // Uses an exclusive lock `absl::WriterMutexLock` so prefer `CGet()` above for
  // reads only.
  template <typename Key = KeyT>
  MutableLockedNode Get(Key&& key) const;

  // Inserts `key` and `value` mapping into the map if `key` does not exist
  // in the map.
  // Returns:
  // - `true` and view to the newly inserted `key`, `value` mapping if `key`
  // does not exist.
  // - `false` and view to the existing `key`, `value` mapping if `key` exist.
  template <typename Key = KeyT, typename Value = ValueT>
  std::pair<MutableLockedNode, bool> PutIfAbsent(Key&& key, Value&& value);

  // Removes `key` and it's associated value from the map if `predicate(value)`
  // is true.
  template <typename Key = KeyT>
  void RemoveIf(
      Key&& key, std::function<bool(const ValueT&)> predicate =
                     [](const ValueT&) { return true; });

  const_iterator begin() ABSL_NO_THREAD_SAFETY_ANALYSIS;
  const_iterator end() ABSL_NO_THREAD_SAFETY_ANALYSIS;

 private:
  using KeyValueMapType =
      absl::flat_hash_map<const KeyT*, std::unique_ptr<ValueT>>;
  using KeyMutexMapType =
      absl::node_hash_map<KeyT, std::unique_ptr<absl::Mutex>>;

  template <typename ValueLockT, typename NodeT, typename Key = KeyT>
  NodeT GetNode(Key&& key) const;

  std::unique_ptr<absl::Mutex> maps_mutex_;
  KeyValueMapType key_value_map_ ABSL_GUARDED_BY(*maps_mutex_);
  KeyMutexMapType key_mutex_map_ ABSL_GUARDED_BY(*maps_mutex_);
};

template <typename KeyT, typename ValueT>
template <typename ValueLockT, typename NodeT, typename Key>
NodeT ThreadSafeHashMap<KeyT, ValueT>::GetNode(Key&& key) const {
  absl::ReaderMutexLock maps_lock(maps_mutex_.get());
  if (auto mutex_iter = key_mutex_map_.find(std::forward<Key>(key));
      mutex_iter != key_mutex_map_.end()) {
    if (auto value_iter = key_value_map_.find(&mutex_iter->first);
        ABSL_PREDICT_TRUE(value_iter != key_value_map_.end())) {
      return NodeT(&mutex_iter->first, value_iter->second.get(),
                   std::make_unique<ValueLockT>(mutex_iter->second.get()));
    }
  }
  return NodeT(nullptr, nullptr, nullptr);
}

template <typename KeyT, typename ValueT>
template <typename Key>
typename ThreadSafeHashMap<KeyT, ValueT>::ConstLockedNode
ThreadSafeHashMap<KeyT, ValueT>::CGet(Key&& key) const {
  return GetNode<absl::ReaderMutexLock, ConstLockedNode, Key>(
      std::forward<Key>(key));
}

template <typename KeyT, typename ValueT>
template <typename Key>
typename ThreadSafeHashMap<KeyT, ValueT>::MutableLockedNode
ThreadSafeHashMap<KeyT, ValueT>::Get(Key&& key) const {
  return GetNode<absl::WriterMutexLock, MutableLockedNode, Key>(
      std::forward<Key>(key));
}

template <typename KeyT, typename ValueT>
template <typename Key, typename Value>
std::pair<typename ThreadSafeHashMap<KeyT, ValueT>::MutableLockedNode, bool>
ThreadSafeHashMap<KeyT, ValueT>::PutIfAbsent(Key&& key, Value&& value) {
  absl::WriterMutexLock maps_lock(maps_mutex_.get());
  if (auto mutex_iter = key_mutex_map_.find(key);
      mutex_iter != key_mutex_map_.end()) {
    if (auto value_iter = key_value_map_.find(&mutex_iter->first);
        ABSL_PREDICT_TRUE(value_iter != key_value_map_.end())) {
      return std::make_pair(
          MutableLockedNode(&mutex_iter->first, value_iter->second.get(),
                            std::make_unique<absl::WriterMutexLock>(
                                mutex_iter->second.get())),
          false);
    }
  }
  auto mutex_result = key_mutex_map_.try_emplace(
      std::forward<Key>(key), std::make_unique<absl::Mutex>());
  auto value_result = key_value_map_.insert_or_assign(
      &mutex_result.first->first,
      std::make_unique<ValueT>(std::forward<Value>(value)));
  return std::make_pair(
      MutableLockedNode(&mutex_result.first->first,
                        value_result.first->second.get(),
                        std::make_unique<absl::WriterMutexLock>(
                            mutex_result.first->second.get())),
      true);
}

template <typename KeyT, typename ValueT>
template <typename Key>
void ThreadSafeHashMap<KeyT, ValueT>::RemoveIf(
    Key&& key, std::function<bool(const ValueT&)> predicate) {
  absl::WriterMutexLock maps_lock(maps_mutex_.get());
  auto mutex_iter = key_mutex_map_.find(std::forward<Key>(key));
  if (mutex_iter == key_mutex_map_.end()) {
    return;
  }
  auto value_iter = key_value_map_.find(&mutex_iter->first);
  if (ABSL_PREDICT_FALSE(value_iter == key_value_map_.end())) {
    return;
  }
  bool should_remove = false;
  {
    absl::WriterMutexLock row_lock(mutex_iter->second.get());
    if (should_remove = predicate(*value_iter->second); should_remove) {
      key_value_map_.erase(value_iter);
    }
  }
  if (should_remove) {
    key_mutex_map_.erase(mutex_iter);
  }
}

template <typename KeyT, typename ValueT>
typename ThreadSafeHashMap<KeyT, ValueT>::const_iterator
ThreadSafeHashMap<KeyT, ValueT>::begin() {
  return const_iterator(
      std::make_unique<absl::ReaderMutexLock>(maps_mutex_.get()),
      key_mutex_map_.begin(), *this);
}

template <typename KeyT, typename ValueT>
typename ThreadSafeHashMap<KeyT, ValueT>::const_iterator
ThreadSafeHashMap<KeyT, ValueT>::end() {
  return const_iterator(nullptr, key_mutex_map_.end(), *this);
}

template <typename KeyT, typename ValueT>
template <typename ValueLockT>
class ThreadSafeHashMap<KeyT, ValueT>::LockedNode {
 public:
  bool is_present() const { return value_ != nullptr; }
  const KeyT* key() const { return key_; }
  ValueT* value() const { return value_; }
  void release() { lock_ = nullptr; }

 private:
  LockedNode() : LockedNode(nullptr, nullptr, nullptr) {}
  LockedNode(const KeyT* key, ValueT* value, std::unique_ptr<ValueLockT> lock)
      : key_(key), value_(value), lock_(std::move(lock)) {}

  friend class ThreadSafeHashMap;

  const KeyT* key_;
  ValueT* value_;
  std::unique_ptr<ValueLockT> lock_;
};

template <typename KeyT, typename ValueT>
class ThreadSafeHashMap<KeyT, ValueT>::ConstLockedNode
    : LockedNode<absl::ReaderMutexLock> {
  using Base = typename ThreadSafeHashMap::ConstLockedNode::LockedNode;

 public:
  using Base::is_present;
  using Base::key;
  using Base::release;
  const ValueT* value() const { return Base::value(); }

 private:
  using Base::Base;
};

template <typename KeyT, typename ValueT>
class ThreadSafeHashMap<KeyT, ValueT>::const_iterator {
 public:
  using value_type = ConstLockedNode;
  using pointer = value_type*;
  using reference = value_type&;

  reference operator*() ABSL_NO_THREAD_SAFETY_ANALYSIS {
    auto value_iter = map_.key_value_map_.find(&mutex_iter_->first);
    node_ = std::move(ConstLockedNode(
        &mutex_iter_->first, value_iter->second.get(),
        std::make_unique<absl::ReaderMutexLock>(mutex_iter_->second.get())));
    return node_;
  }
  pointer operator->() { return &operator*(); }
  const_iterator& operator++() {
    mutex_iter_++;
    return *this;
  }
  friend bool operator==(const const_iterator& a, const const_iterator& b) {
    return a.mutex_iter_ == b.mutex_iter_;
  }
  friend bool operator!=(const const_iterator& a, const const_iterator& b) {
    return !(a == b);
  }

 private:
  const_iterator(std::unique_ptr<absl::ReaderMutexLock> maps_lock,
                 typename KeyMutexMapType::iterator mutex_iter,
                 ThreadSafeHashMap<KeyT, ValueT>& map)
      : maps_lock_(std::move(maps_lock)), mutex_iter_(mutex_iter), map_(map) {}

  friend class ThreadSafeHashMap;

  ConstLockedNode node_;
  std::unique_ptr<absl::ReaderMutexLock> maps_lock_;
  typename KeyMutexMapType::iterator mutex_iter_;
  ThreadSafeHashMap& map_;
};

}  // namespace kv_server

#endif  // COMPONENTS_CONTAINER_THREAD_SAFE_HASH_MAP_H_
