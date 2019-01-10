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

	uint64_t __vmill_inc_task_num(){
	  return ++task_num;
	}

    llvm::Function *__vmill_modify_klee_pc( llvm::Function *func ){
      //  implemented in klee's function handler
	   return func;
    }

	Memory *__remill_jump(State &state, addr_t pc, Memory *memory){
      auto mem = gExecutor -> Memory(task_num);
      auto lifted_func = gExecutor -> GetLiftedFunction( mem, pc );
	  if (!lifted_func) {
          //llvm::dbgs() << "A Lifted Function Does Not Exist at" << pc;
          //llvm::dbgs() << '\n';
          exit(1337);
	  }
      //auto ins = &*(inst_begin(lifted_func));
	  __vmill_modify_klee_pc(lifted_func);
      return memory;
    }

    int __vmill_entrypoint(int argc, char *argv[], char *envp[]) {
      if ( gExecutor == nullptr ){
          //llvm::dbgs() << "The Executor is a NULL Value";
          //llvm::dbgs() << '\n';
          exit(0);
      }

      auto state = __vmill_task_0.state;
      addr_t pc = state.gpr.rip.aword;
      //llvm::dbgs() << "First PC is " << pc;
      //llvm::dbgs() << '\n';
     __remill_jump(state, pc , nullptr);
      auto mem = gExecutor -> Memory(task_num);
      auto lifted_func = gExecutor -> GetLiftedFunction( mem, pc );
	  if (!lifted_func) {
          exit(1337);
	  }
      //auto ins = &*(inst_begin(lifted_func));
	  __vmill_modify_klee_pc(lifted_func);
 
      return 0;
    }
  } // extern C

}  // namespace vmill
