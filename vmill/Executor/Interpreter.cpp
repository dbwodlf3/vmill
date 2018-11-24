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
#include <glog/logging.h>

#include "llvm/InstVisitor.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/Instruction.h"

#include "vmill/Executor/TraceManager.h"
#include "vmill/Executor/Runtime.h"
#include "vmill/Executor/Interpreter.h"

#include "vmill/Program/AddressSpace.h"

#include "vmill/third_party/klee/klee.h"
#include "vmill/third_party/klee/Interpreter.h"
#include "vmill/third_party/llvm/Interpreter.h"

namespace llvm {
  class ExecutionEngine;
  class VmillInterpreter;
  class Function;
  class Module;
}

namespace klee {
  class Interpreter;
  class InterpreterOptions;
  class InterpreterHandler;
  class ExecutionState;
}

//perform concrete execution
//mark variables as symbolic in emulator and address space
//when encountered throw function into symbolic klee interpreter

namespace vmill {

class Interpreter{
  public:
    Interpreter *Create(llvm::Module *module);
    void symbolic_execute(llvm::Function *func, llvm::Value *args) = 0;
    void concrete_execute(llvm::Function *func, llvm::Value *args) = 0;
  protected:
    Interpreter(void){}
    ~Interpreter(void) = 0;
};

//utility class that will handle calls to the vmill runtime
class Handler {
  public:
    Handler(void){}
    ~Handler(void){}
    void handle(llvm::Instruction *instr, 
       std::deque<TaskContinuation> &tasks);
};


void Handler::handle(llvm::Instruction *instr,
              std::deque<TaskContinuation> &tasks) {
  //basically a huge switch case that performs actions based off of the instruction
  //and calls into the runtime an example would be something like
  //marking a buffer as symbolic in the read syscall runtime implementation
}

//*** FOCUS ON CONCRETE EXECUTION AND HOOKING FOR NOW IN THE INTERPRETER

class InterpreterImpl: public llvm::VmillInterpreter, 
                       public klee::Interpreter,
                       public Interpreter {
    public:
      explicit InterpreterImpl(llvm::Module *module_, 
                               std::deque<TaskContinuation> &tasks_):
          llvm::VmillInterpreter(std::unique_ptr<Module>(module_)),
          klee::Interpreter(klee::InterpreterOptions()),
          Interpreter(),
          handler(Handler());
          module(module_),
          tasks(tasks_){}

      //will call to klee's interpreter once all buffers have been marked symbolic
      //and the handler has already added to the tasks and address space
      void symbolic_execute(llvm::Function *func, llvm::Value *args){
      
      }

      void run_and_handle(){
        while(!ECStack.empty()){
          ExecutionContext &SF = ECStack.back();
          Instruction &I = *SF.CurInst++;
		  if (I.getOpcode() == Instruction::Call){
          	Handler.handle(I, tasks); //can appropriately hook functions
		  } else {
            visit(I);
          }
          ++NumDynamicInsts;
        }
      }

      //handles emulator runtime with handler and executes function concretely
      void run_function(llvm::Function *func, ArrayRef<GenericValue> ArgValues){
		const size_t ArgCount = F->getFunctionType()->getNumParams();
		ArrayRef<GenericValue> ActualArgs =
			ArgValues.slice(0, std::min(ArgValues.size(), ArgCount));
		llvm::VmillInterpreter::callFunction(F, ActualArgs);
		run_and_handle();
	  }
    
      void concrete_execute(llvm::Function *func, llvm::Value *args){
        llvm::ArrayRef<GenericValue> argv({args[0], args[1], args[2]});
        run_function(func, argv);
      }
     
    private:
      Handler handler;
      std::shared_ptr<llvm::Module> module;
      std::deque<TaskContinuation> &tasks;
};
}  // namespace vmill
