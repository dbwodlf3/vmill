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

#ifndef VMILL_MEMORY_ADDRESSSPACE_H_
#define VMILL_MEMORY_ADDRESSSPACE_H_

#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "vmill/Program/MappedRange.h"

struct Memory {};

namespace vmill {

enum class CodeVersion : uint64_t;
enum class PC : uint64_t;

// Basic memory implementation.
class AddressSpace : public Memory {
 public:
  AddressSpace(void);

  // Creates a copy/clone of another address space.
  explicit AddressSpace(const AddressSpace &);

  // Kill this address space. This prevents future allocations, and removes
  // all existing ranges.
  void Kill(void);

  // Returns `true` if this address space is "dead".
  bool IsDead(void) const;

  // Returns `true` if the byte at address `addr` is readable,
  // writable, or executable, respectively.
  bool CanRead(uint64_t addr) const;
  bool CanWrite(uint64_t addr) const;
  bool CanExecute(uint64_t addr) const;

  bool TryRead(uint64_t addr, void *val, size_t size);
  bool TryWrite(uint64_t addr, const void *val, size_t size);

  // Read/write a byte to memory. Returns `false` if the read or write failed.
  bool TryRead(uint64_t addr, uint8_t *val);
  bool TryWrite(uint64_t addr, uint8_t val);

  // Read/write a word to memory. Returns `false` if the read or write failed.
  bool TryRead(uint64_t addr, uint16_t *val);
  bool TryWrite(uint64_t addr, uint16_t val);

  // Read/write a dword to memory. Returns `false` if the read or write failed.
  bool TryRead(uint64_t addr, uint32_t *val);
  bool TryWrite(uint64_t addr, uint32_t val);

  // Read/write a qword to memory. Returns `false` if the read or write failed.
  bool TryRead(uint64_t addr, uint64_t *val);
  bool TryWrite(uint64_t addr, uint64_t val);

  // Read/write a float to memory. Returns `false` if the read or write failed.
  bool TryRead(uint64_t addr, float *val);
  bool TryWrite(uint64_t addr, float val);

  // Read/write a double to memory. Returns `false` if the read or write failed.
  bool TryRead(uint64_t addr, double *val);
  bool TryWrite(uint64_t addr, double val);

  // Return the virtual address of the memory backing `addr`.
  void *ToVirtualAddress(uint64_t addr);

  // Read a byte as an executable byte. This is used for instruction decoding.
  // Returns `false` if the read failed. This function operates on the state
  // of a page, and may result in broad-reaching cache invalidations.
  bool TryReadExecutable(PC addr, uint8_t *val);

  // Change the permissions of some range of memory. This can split memory
  // maps.
  void SetPermissions(uint64_t base, size_t size, bool can_read,
                      bool can_write, bool can_exec);

  // Adds a new memory mapping with default read/write permissions.
  void AddMap(uint64_t base, size_t size, const char *name=nullptr,
              uint64_t offset=0);

  // Removes a memory mapping.
  void RemoveMap(uint64_t base, size_t size);

  // Log out the current state of the memory maps.
  void LogMaps(std::ostream &stream);

  // Find the smallest mapped memory range limit address that is greater
  // than `find`.
  bool NearestLimitAddress(uint64_t find, uint64_t *next_end) const;

  // Find the largest mapped memory range base address that is less-than
  // or equal to `find`.
  bool NearestBaseAddress(uint64_t find, uint64_t *next_end) const;

  // Have we observed a write to executable memory since our last attempt
  // to read from executable memory?
  bool CodeVersionIsInvalid(void) const;

  // Returns a hash of all executable code. Useful for getting the current
  // version of the code.
  //
  // NOTE(pag): If the code version is invalid, then this will recompute it,
  //            thereby making it valid.
  CodeVersion ComputeCodeVersion(void);

  // Mark some PC in this address space as being a known trace head. This is
  // used for helping the decoder to not repeat past work.
  void MarkAsTraceHead(PC pc);

  // Check to see if a given program counter is a trace head.
  bool IsMarkedTraceHead(PC pc) const;

 private:
  AddressSpace(AddressSpace &&) = delete;
  AddressSpace &operator=(const AddressSpace &) = delete;
  AddressSpace &operator=(const AddressSpace &&) = delete;

  // Check that the ranges are sane.
  void CheckRanges(std::vector<MemoryMapPtr> &);

  // Recreate the `range_base_to_index` and `range_limit_to_index` indices.
  void CreatePageToRangeMap(void);

  // Find the memory map containing `addr`. If none is found then a "null"
  // map pointer is returned, whose operations will all fail.
  const MemoryMapPtr &FindRange(uint64_t addr);
  const MemoryMapPtr &FindWNXRange(uint64_t addr);

  // Used to represent an invalid memory map.
  MemoryMapPtr invalid_map;

  // Sorted list of mapped memory page ranges.
  std::vector<MemoryMapPtr> maps;

  // A cache mapping pages accessed to the range.
  using PageCache = std::unordered_map<uint64_t, MemoryMapPtr>;
  PageCache page_to_map;
  PageCache wnx_page_to_map;

  PageCache::iterator last_read_map;
  PageCache::iterator last_written_map;

  // Sets of pages that are readable, writable, and executable.
  std::unordered_set<uint64_t> page_is_readable;
  std::unordered_set<uint64_t> page_is_writable;
  std::unordered_set<uint64_t> page_is_executable;

  // Is the address space dead? This means that all operations on it
  // will be muted.
  bool is_dead;

  // Has there been a write to executable memory since the previous read from
  // executable memory?
  bool code_version_is_invalid;

  // An identifier that links together this address space, and any ones that
  // are cloned from it, so that we can partition code caches according to
  // a common ancestry.
  uint64_t code_version;

  // Set of lifted trace heads observed for this code version.
  std::unordered_set<uint64_t> trace_heads;
};

}  // namespace vmill

#endif  // VMILL_MEMORY_ADDRESSSPACE_H_
