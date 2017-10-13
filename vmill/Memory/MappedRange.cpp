/*
 * Copyright (c) 2017 Trail of Bits, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <limits>
#include <memory>
#include <new>
#include <utility>

#include <sys/mman.h>

#include "vmill/Etc/xxHash/xxhash.h"
#include "vmill/Memory/MappedRange.h"
#include "vmill/Util/Compiler.h"

namespace vmill {
namespace {

enum : uint64_t {
  kPageSize = 4096ULL,
  kPageShift = (kPageSize - 1ULL),
  kPageMask = ~kPageShift
};

class MappedRangeBase;
class ArrayMemoryMap;
class EmptyMemoryMap;
class CopyOnWriteMemoryMap;
class InvalidMemoryMap;

// Basic information about some region of mapped memory within an address space.
class MappedRangeBase : public MappedRange {
 public:
  MappedRangeBase(uint64_t base_address_, uint64_t limit_address_);
  virtual ~MappedRangeBase(void);
  virtual bool IsValid(void) const;
  void InvalidateCodeVersion(void) final;
  MemoryMapPtr Copy(uint64_t clone_base, uint64_t clone_limit) final;

  uint64_t code_version;
  bool code_version_is_valid;
  uint8_t *data;
  MemoryMapPtr parent;
};

// Implements an invalid range of memory that is unfilled.
class InvalidMemoryMap : public MappedRangeBase {
 public:
  using MappedRangeBase::MappedRangeBase;

  virtual ~InvalidMemoryMap(void);
  bool Read(uint64_t, uint8_t *out_val) override;
  bool Write(uint64_t, uint8_t) override;
  MemoryMapPtr Clone(void) override;
  uint64_t CodeVersion(void) override;
};

// Implements an array-backed memory mapping that is filled with actual data
// bytes.
class ArrayMemoryMap : public MappedRangeBase {
 public:
  ArrayMemoryMap(uint64_t base_address_, uint64_t limit_address_);

  virtual ~ArrayMemoryMap(void);

  explicit ArrayMemoryMap(ArrayMemoryMap *steal);

  bool Read(uint64_t address, uint8_t *out_val) override;
  bool Write(uint64_t address, uint8_t val) override;
  MemoryMapPtr Clone(void) override;
  uint64_t CodeVersion(void) override;
};

// Implements an empty range of memory that is filled with zeroes.
class EmptyMemoryMap : public MappedRangeBase {
 public:
  using MappedRangeBase::MappedRangeBase;

  virtual ~EmptyMemoryMap(void);
  bool Read(uint64_t, uint8_t *out_val) override;
  bool Write(uint64_t address, uint8_t val) override;
  MemoryMapPtr Clone(void) override;
  uint64_t CodeVersion(void) override;
};

// Implements a copy-on-write range of memory.
class CopyOnWriteMemoryMap : public MappedRangeBase {
 public:
  explicit CopyOnWriteMemoryMap(MemoryMapPtr parent_);
  virtual ~CopyOnWriteMemoryMap(void);
  bool IsValid(void) const override;
  bool Read(uint64_t address, uint8_t *out_val) override;
  bool Write(uint64_t address, uint8_t val) override;
  MemoryMapPtr Clone(void) override;
  uint64_t CodeVersion(void) override;

 private:
  using MappedRangeBase::MappedRangeBase;
};

static_assert(sizeof(ArrayMemoryMap) == sizeof(MappedRangeBase),
              "Vtable overwriting won't work!");

static_assert(sizeof(EmptyMemoryMap) == sizeof(MappedRangeBase),
              "Vtable overwriting won't work!");

static_assert(sizeof(InvalidMemoryMap) == sizeof(MappedRangeBase),
              "Vtable overwriting won't work!");

static_assert(sizeof(CopyOnWriteMemoryMap) == sizeof(MappedRangeBase),
              "Vtable overwriting won't work!");

MappedRangeBase::MappedRangeBase(
    uint64_t base_address_, uint64_t limit_address_)
    : MappedRange(base_address_, limit_address_),
      code_version(0),
      code_version_is_valid(false),
      data(nullptr),
      parent(nullptr) {}

MappedRangeBase::~MappedRangeBase(void) {
  if (data) {
    auto data_addr = reinterpret_cast<uintptr_t>(data);
    munmap(reinterpret_cast<void *>(data_addr - kPageSize),
           Size() + 2 * kPageSize);
    data = nullptr;
  }
}

void MappedRangeBase::InvalidateCodeVersion(void) {
  code_version_is_valid = false;
}

bool MappedRangeBase::IsValid(void) const {
  return true;
}

MemoryMapPtr MappedRangeBase::Copy(uint64_t clone_base, uint64_t clone_limit) {
  auto array_backed = std::make_shared<ArrayMemoryMap>(clone_base, clone_limit);
  for (; clone_base < clone_limit; ++clone_base) {
    if (Contains(clone_base)) {
      uint8_t val = 0;
      Read(clone_base, &val);
      array_backed->Write(clone_base, val);
    }
  }
  return array_backed;
}


InvalidMemoryMap::~InvalidMemoryMap(void) {}

bool InvalidMemoryMap::Read(uint64_t, uint8_t *out_val) {
  *out_val = 0;
  return false;
}

bool InvalidMemoryMap::Write(uint64_t, uint8_t) {
  return false;
}

MemoryMapPtr InvalidMemoryMap::Clone(void) {
  return std::make_shared<InvalidMemoryMap>(base_address, limit_address);
}

uint64_t InvalidMemoryMap::CodeVersion(void) {
  return 0;
}

// Allocate memory for some data. This will redzone the allocation with
// two unreadable/unwritable pages around the allocation.
ArrayMemoryMap::ArrayMemoryMap(uint64_t base_address_, uint64_t limit_address_)
    : MappedRangeBase(base_address_, limit_address_) {

  CHECK(Size() == (Size() / kPageSize) * kPageSize)
      << "Invalid memory map size.";

  auto all_data = reinterpret_cast<uint8_t *>(
      mmap(nullptr, Size() + 2 * kPageSize, PROT_NONE,
           MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0));

  auto err = errno;
  CHECK(nullptr != all_data)
      << "Couldn't allocate page tables for `ArrayMemoryMap`: "
      << strerror(err);

  data = reinterpret_cast<uint8_t *>(
      mmap(&(all_data[kPageSize]), Size(), PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0));

  err = errno;
  CHECK(data == &(all_data[kPageSize]))
      << "Couldn't fixed-map the backing memory for `ArrayMemoryMap`: "
      << strerror(errno);

  memset(data, 0, Size());
}

ArrayMemoryMap::ArrayMemoryMap(ArrayMemoryMap *steal)
    : MappedRangeBase(steal->BaseAddress(), steal->LimitAddress()) {
  data = steal->data;
  steal->data = nullptr;
}

ArrayMemoryMap::~ArrayMemoryMap(void) {}

bool ArrayMemoryMap::Read(uint64_t address, uint8_t *out_val) {
  *out_val = data[address - base_address];
  return true;
}

bool ArrayMemoryMap::Write(uint64_t address, uint8_t val) {
  data[address - BaseAddress()] = val;
  return true;
}

// Creates a new `ArrayMemoryMap` that takes over the data of this array memory
// map, then we convert this array memory map into a copy-on-write memory map,
// and then clone it.
MemoryMapPtr ArrayMemoryMap::Clone(void) {
  auto parent = std::make_shared<ArrayMemoryMap>(this);
  auto self = new (this) CopyOnWriteMemoryMap(parent);
  return self->Clone();
}

uint64_t ArrayMemoryMap::CodeVersion(void) {
  if (code_version_is_valid) {
    return code_version;
  }

  XXH64_state_t state = {};
  XXH64_reset(&state, 0);
  XXH64_update(&state, data, Size());
  code_version = XXH64_digest(&state);
  code_version_is_valid = true;
  return code_version;
}

EmptyMemoryMap::~EmptyMemoryMap(void) {}

bool EmptyMemoryMap::Read(uint64_t, uint8_t *out_val) {
  *out_val = 0;
  return true;
}

bool EmptyMemoryMap::Write(uint64_t address, uint8_t val) {
  auto self = new (this) ArrayMemoryMap(base_address, limit_address);
  return self->Write(address, val);
}

MemoryMapPtr EmptyMemoryMap::Clone(void) {
  return std::make_shared<EmptyMemoryMap>(base_address, limit_address);
}

uint64_t EmptyMemoryMap::CodeVersion(void) {
  return 0;
}

CopyOnWriteMemoryMap::CopyOnWriteMemoryMap(MemoryMapPtr parent_)
    : MappedRangeBase(parent_->BaseAddress(), parent_->LimitAddress()) {
  while (parent_) {
    parent = parent_;
    parent_ = reinterpret_cast<MappedRangeBase &>(*parent).parent;
  }
}

CopyOnWriteMemoryMap::~CopyOnWriteMemoryMap(void) {}

bool CopyOnWriteMemoryMap::IsValid(void) const {
  return parent->IsValid();
}

bool CopyOnWriteMemoryMap::Read(uint64_t address, uint8_t *out_val) {
  return parent->Read(address, out_val);
}

bool CopyOnWriteMemoryMap::Write(uint64_t address, uint8_t val) {
  auto parent_ptr = parent;
  auto base_addr = BaseAddress();
  auto limit_addr = LimitAddress();
  parent.reset();

  auto self = new (this) ArrayMemoryMap(base_addr, limit_addr);
  for (uint64_t index = 0; base_addr < limit_addr; ++base_addr, ++index) {
    (void) parent_ptr->Read(base_addr, &(self->data[index]));
  }

  return self->Write(address, val);
}

MemoryMapPtr CopyOnWriteMemoryMap::Clone(void) {
  return std::make_shared<CopyOnWriteMemoryMap>(parent);
}

uint64_t CopyOnWriteMemoryMap::CodeVersion(void) {
  return parent->CodeVersion();
}

}  // namespace

MemoryMapPtr MappedRange::Create(uint64_t base_address_,
                                 uint64_t limit_address_) {
  MemoryMapPtr ptr(new EmptyMemoryMap(base_address_, limit_address_));
  return ptr;
}

MemoryMapPtr MappedRange::CreateInvalid(void) {
  MemoryMapPtr ptr(new InvalidMemoryMap(0, 0));
  return ptr;
}

MappedRange::MappedRange(uint64_t base_address_, uint64_t limit_address_)
    : base_address(base_address_),
      limit_address(limit_address_) {}

MappedRange::~MappedRange(void) {}

}  // namespace vmill
