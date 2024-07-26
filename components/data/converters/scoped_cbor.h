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

#ifndef SERVICES_COMMON_UTIL_SCOPED_CBOR_H_
#define SERVICES_COMMON_UTIL_SCOPED_CBOR_H_

#include "cbor.h"

// TODO(b/355941387): move to the common repo
namespace kv_server {

// Wrapper class for managing the ref-counted CBOR objects.
class ScopedCbor {
 public:
  ScopedCbor() {}

  // Expects a pointer to an object that is ref-incremented at the
  // time of creation (e.g. an object created with cbor_build_string).
  explicit ScopedCbor(cbor_item_t* val) { ptr_ = val; }
  virtual ~ScopedCbor() {
    if (ptr_) {
      cbor_decref(&ptr_);
    }
  }
  ScopedCbor& operator=(cbor_item_t* val) {
    if (ptr_) {
      cbor_decref(&ptr_);
    }
    ptr_ = val;
    return *this;
  }

  cbor_item_t* operator->() const { return ptr_; }
  cbor_item_t* operator*() const { return ptr_; }
  cbor_item_t* get() const { return ptr_; }
  cbor_item_t* release() {
    cbor_item_t* tmp = ptr_;
    ptr_ = nullptr;
    return tmp;
  }

  explicit operator bool() const { return ptr_ != nullptr; }
  bool operator!() const { return ptr_ == nullptr; }

 private:
  cbor_item_t* ptr_ = nullptr;
};

}  // namespace kv_server

#endif  // SERVICES_COMMON_UTIL_SCOPED_CBOR_H_
