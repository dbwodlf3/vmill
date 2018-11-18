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

#ifndef VMILL_RUNTIME_TASKSTATUS_H_
#define VMILL_RUNTIME_TASKSTATUS_H_

#include <cstdint>

struct ArchState;
struct Memory;

namespace vmill {

class AddressSpace;
class Coroutine;

enum class PC : uint64_t;

enum TaskStatus : uint64_t {
  // This task is ready to run.
  kTaskStatusRunnable,

  // This task is paused doing async I/O. This is a runnable state.
  kTaskStatusResumable,

  // This task encountered an error.
  kTaskStatusError,

  // This task exited.
  kTaskStatusExited,
};

enum TaskStopLocation : uint64_t {
  kTaskNotYetStarted,
  kTaskStoppedAtSnapshotEntryPoint,
  kTaskStoppedAtJumpTarget,
  kTaskStoppedAtCallTarget,
  kTaskStoppedAtReturnTarget,
  kTaskStoppedAtError,
  kTaskStoppedAfterHyperCall,
  kTaskStoppedBeforeUnhandledHyperCall,
  kTaskStoppedAtUnsupportedInstruction,
  kTaskStoppedAtExit
};

enum MemoryAccessFaultKind : uint16_t {
  kMemoryAccessNoFault,
  kMemoryAccessFaultOnRead,
  kMemoryAccessFaultOnWrite,
  kMemoryAccessFaultOnExecute
};

enum MemoryValueType : uint16_t {
  kMemoryValueTypeInvalid,
  kMemoryValueTypeInteger,
  kMemoryValueTypeFloatingPoint,
  kMemoryValueTypeInstruction
};

// A task is like a thread, but really, it's the runtime that gives a bit more
// meaning to threads.
//
// `struct.Task {[u8 * N], ...}
struct Task {

  uint8_t opaque_state[sizeof(State)];

  inline struct State *State(void) {
    return reinterpret_cast<struct State *>(&(opaque_state[0]));
  }

};

extern "C" Task __vmill_task_0;

}  // namespace vmill

#endif  // VMILL_RUNTIME_TASKSTATUS_H_
