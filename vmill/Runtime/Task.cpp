/*
 * Copyright (c) 2018 Trail of Bits, Inc.
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

#include "vmill/Executor/Executor.h"
#include "vmill/Runtime/Task.h"
#include "remill/Arch/Runtime/State.h"
//#include "remill/Arch/Runtime/Intrinsics.cpp"

#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"


namespace vmill {
  extern thread_local Executor *gExecutor;
  
  extern "C" {
    uint64_t task_num = 0;
    
    Task __vmill_task_0 = {};

    typedef Memory * (LiftedFunc)(State &, addr_t, Memory *);

	LiftedFunc *__vmill_klee_hook(addr_t pc);

	Memory * __remill_jump(State &state, addr_t pc, Memory *memory) {
      state.gpr.rip.aword = pc;
      auto func = __vmill_klee_hook(pc);
      return func(state, pc, memory);
	}

    Memory * __remill_write_memory_64(Memory * memory, addr_t pc, uint64_t value){
      return memory; 
    }

    uint64_t __remill_read_memory_64(Memory * memory, addr_t pc){
      return pc; 
    }

	int __vmill_entrypoint(int argc, char *argv[3], char *envp[]) {
	  assert(argc == 1);
	  Memory *memory = nullptr;

	  memcpy(&(__vmill_task_0.state), argv[0], sizeof(__vmill_task_0.state));
	  //memcpy(&memory, argv[2], sizeof(memory));
	  __remill_jump(__vmill_task_0.state, __vmill_task_0.state.gpr.rip.aword, memory);
	  return 0;
	}

  } // extern C

}  // namespace vmill
