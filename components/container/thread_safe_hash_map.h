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
  class LockedNodePtr;
  // Locked read-only view for a specific key value associtation in the map.
  // Must call `is_present()` before calling and dereferencing `key()` and
  // `value()`.
  class ConstLockedNodePtr;
  // Locked view for a specific key value associtation in the map with an
  // in-place modifable value. Must call `is_present()` before calling and
  // dereferencing `key()` and `value()`.
  using MutableLockedNodePtr = LockedNodePtr<absl::WriterMutexLock>;

  ThreadSafeHashMap() : nodes_map_mutex_(std::make_unique<absl::Mutex>()) {}

  // Returns a locked read-only view for `key` and it's associated value. If
  // `key` does not exist in the map, then: `ConstLockedNode.is_present()` is
  // false.
  // Prefer this function for reads because it uses a shared
  // `absl::ReaderMutexLock` for synchronization and concurrent reads do not
  // block.
  template <typename Key = KeyT>
  ConstLockedNodePtr CGet(Key&& key) const;

  // Returns a locked view for `key` with a modifable value. If `key` does not
  // exist in the map, then: `MutableLockedNode.is_present()` is false.
  // Uses an exclusive lock `absl::WriterMutexLock` so prefer `CGet()` above for
  // reads only.
  template <typename Key = KeyT>
  MutableLockedNodePtr Get(Key&& key) const;

  // Inserts `key` and `value` mapping into the map if `key` does not exist
  // in the map.
  // Returns:
  // - `true` and view to the newly inserted `key`, `value` mapping if `key`
  // does not exist.
  // - `false` and view to the existing `key`, `value` mapping if `key` exist.
  template <typename Key = KeyT, typename Value = ValueT>
  std::pair<MutableLockedNodePtr, bool> PutIfAbsent(Key&& key, Value&& value);

  // Removes `key` and it's associated value from the map if `predicate(value)`
  // is true.
  template <typename Key = KeyT>
  void RemoveIf(
      Key&& key, std::function<bool(const ValueT&)> predicate =
                     [](const ValueT&) { return true; });

  const_iterator begin() ABSL_NO_THREAD_SAFETY_ANALYSIS;
  const_iterator end() ABSL_NO_THREAD_SAFETY_ANALYSIS;

 private:
  struct ValueNode {
    template <typename Value>
    explicit ValueNode(Value&& val)
        : value(std::forward<Value>(val)),
          mutex(std::make_unique<absl::Mutex>()) {}
    ValueT value;
    std::unique_ptr<absl::Mutex> mutex;
  };
  using KeyValueNodesMapType =
      absl::node_hash_map<KeyT, std::unique_ptr<ValueNode>>;

  template <typename ValueLockT, typename NodeT, typename Key = KeyT>
  NodeT GetNode(Key&& key) const;

  std::unique_ptr<absl::Mutex> nodes_map_mutex_;
  KeyValueNodesMapType key_value_nodes_map_ ABSL_GUARDED_BY(*nodes_map_mutex_);
};

template <typename KeyT, typename ValueT>
template <typename ValueLockT, typename NodeT, typename Key>
NodeT ThreadSafeHashMap<KeyT, ValueT>::GetNode(Key&& key) const {
  absl::ReaderMutexLock map_lock(nodes_map_mutex_.get());
  if (auto iter = key_value_nodes_map_.find(std::forward<Key>(key));
      iter == key_value_nodes_map_.end()) {
    return NodeT(nullptr, nullptr, nullptr);
  } else {
    return NodeT(&iter->first, &iter->second->value,
                 std::make_unique<ValueLockT>(iter->second->mutex.get()));
  }
}

template <typename KeyT, typename ValueT>
template <typename Key>
typename ThreadSafeHashMap<KeyT, ValueT>::ConstLockedNodePtr
ThreadSafeHashMap<KeyT, ValueT>::CGet(Key&& key) const {
  return GetNode<absl::ReaderMutexLock, ConstLockedNodePtr, Key>(
      std::forward<Key>(key));
}

template <typename KeyT, typename ValueT>
template <typename Key>
typename ThreadSafeHashMap<KeyT, ValueT>::MutableLockedNodePtr
ThreadSafeHashMap<KeyT, ValueT>::Get(Key&& key) const {
  return GetNode<absl::WriterMutexLock, MutableLockedNodePtr, Key>(
      std::forward<Key>(key));
}

template <typename KeyT, typename ValueT>
template <typename Key, typename Value>
std::pair<typename ThreadSafeHashMap<KeyT, ValueT>::MutableLockedNodePtr, bool>
ThreadSafeHashMap<KeyT, ValueT>::PutIfAbsent(Key&& key, Value&& value) {
  absl::WriterMutexLock map_lock(nodes_map_mutex_.get());
  if (auto iter = key_value_nodes_map_.find(key);
      iter != key_value_nodes_map_.end()) {
    return std::make_pair(
        MutableLockedNodePtr(
            &iter->first, &iter->second->value,
            std::make_unique<absl::WriterMutexLock>(iter->second->mutex.get())),
        false);
  }
  auto result = key_value_nodes_map_.emplace(
      std::forward<Key>(key),
      std::make_unique<ValueNode>(std::forward<Value>(value)));
  return std::make_pair(
      MutableLockedNodePtr(&result.first->first, &result.first->second->value,
                           std::make_unique<absl::WriterMutexLock>(
                               result.first->second->mutex.get())),
      true);
}

template <typename KeyT, typename ValueT>
template <typename Key>
void ThreadSafeHashMap<KeyT, ValueT>::RemoveIf(
    Key&& key, std::function<bool(const ValueT&)> predicate) {
  absl::WriterMutexLock map_lock(nodes_map_mutex_.get());
  auto iter = key_value_nodes_map_.find(std::forward<Key>(key));
  if (iter == key_value_nodes_map_.end()) {
    return;
  }
  {
    // Wait for any current threads using the value to release their locks.
    absl::WriterMutexLock value_lock(iter->second->mutex.get());
  }
  if (predicate(iter->second->value)) {
    key_value_nodes_map_.erase(iter);
  }
}

template <typename KeyT, typename ValueT>
typename ThreadSafeHashMap<KeyT, ValueT>::const_iterator
ThreadSafeHashMap<KeyT, ValueT>::begin() {
  return const_iterator(
      std::make_unique<absl::ReaderMutexLock>(nodes_map_mutex_.get()),
      key_value_nodes_map_.begin());
}

template <typename KeyT, typename ValueT>
typename ThreadSafeHashMap<KeyT, ValueT>::const_iterator
ThreadSafeHashMap<KeyT, ValueT>::end() {
  return const_iterator(nullptr, key_value_nodes_map_.end());
}

template <typename KeyT, typename ValueT>
template <typename ValueLockT>
class ThreadSafeHashMap<KeyT, ValueT>::LockedNodePtr {
 public:
  bool is_present() const { return key_ != nullptr; }
  const KeyT* key() const { return key_; }
  ValueT* value() const { return value_; }
  void release() { lock_ = nullptr; }

 private:
  LockedNodePtr() : LockedNodePtr(nullptr, nullptr, nullptr) {}
  LockedNodePtr(const KeyT* key, ValueT* value,
                std::unique_ptr<ValueLockT> lock)
      : key_(key), value_(value), lock_(std::move(lock)) {}

  friend class ThreadSafeHashMap;

  const KeyT* key_;
  ValueT* value_;
  std::unique_ptr<ValueLockT> lock_;
};

template <typename KeyT, typename ValueT>
class ThreadSafeHashMap<KeyT, ValueT>::ConstLockedNodePtr
    : LockedNodePtr<absl::ReaderMutexLock> {
  using Base = typename ThreadSafeHashMap::ConstLockedNodePtr::LockedNodePtr;

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
  using value_type = ConstLockedNodePtr;
  using pointer = value_type*;
  using reference = value_type&;

  reference operator*() ABSL_NO_THREAD_SAFETY_ANALYSIS {
    current_node_ = std::move(ConstLockedNodePtr(
        &nodes_map_iter_->first, &nodes_map_iter_->second->value,
        std::make_unique<absl::ReaderMutexLock>(
            nodes_map_iter_->second->mutex.get())));
    return current_node_;
  }
  pointer operator->() { return &operator*(); }
  const_iterator& operator++() {
    nodes_map_iter_++;
    return *this;
  }
  friend bool operator==(const const_iterator& a, const const_iterator& b) {
    return a.nodes_map_iter_ == b.nodes_map_iter_;
  }
  friend bool operator!=(const const_iterator& a, const const_iterator& b) {
    return !(a == b);
  }

 private:
  const_iterator(std::unique_ptr<absl::ReaderMutexLock> nodes_map_lock,
                 typename KeyValueNodesMapType::iterator nodes_map_iter)
      : nodes_map_lock_(std::move(nodes_map_lock)),
        nodes_map_iter_(nodes_map_iter) {}

  friend class ThreadSafeHashMap;

  ConstLockedNodePtr current_node_;
  std::unique_ptr<absl::ReaderMutexLock> nodes_map_lock_;
  typename KeyValueNodesMapType::iterator nodes_map_iter_;
};

}  // namespace kv_server

#endif  // COMPONENTS_CONTAINER_THREAD_SAFE_HASH_MAP_H_
